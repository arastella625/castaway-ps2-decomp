#!/usr/bin/env python3
"""Read the objects placed on each lot, from DEFAULT.NGH on your own disc image.

Usage:
  python -I tools/placed.py check <game.iso>
  python -I tools/placed.py list  <game.iso> <lot_01>
  python -I tools/placed.py lot   <game.iso> <out.glb> <lot_01> [--posed]

check decodes every object of every house. list prints one lot's objects. lot writes the lot's
terrain, its scenery and every placed object that has a model to one .glb, which is game data:
keep it out of the repo. Terrain is the node named after the lot; placed objects and scenery are
the other nodes, with names starting 'placed ' and 'scenery '. COLOR_0 holds what VU1 sends the GS
per vertex, and material extras hold the draw order and any detail pass, whose texture rides in
occlusionTexture. docs/formats/world.md has the format.
"""
import collections
import math
import os
import struct
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import gamedata as gd  # noqa: E402
import gltf  # noqa: E402
import model as mdl  # noqa: E402

# Bit widths ReconCmprInt picks between, from the Scheme objects built at startup.
SCHEMES = {8: (2, 4, 6, 8), 16: (5, 8, 13, 16), 32: (6, 11, 21, 32)}
MARK_BASE = 12  # marks count from just after the chunk's own 12-byte header
TEMPS = 8
VARIABLE_KEYS = 72
DIRECTION_KEY = 1
OBJECT_ID_KEY = 0xB
UNITS_PER_TILE = 16
EIGHTH_TURN = math.pi / 4
FIRST_LOT_OFFSET = 1  # section 0 is lot_01: HouseData's row 0 is a placeholder
MISMATCHED_LOT = 'lot_28'  # see docs/formats/world.md
DEF_GUID = 0x1C
DEF_TYPE = 0x12
DEF_TILES = 0x14  # non-zero on the tiles of a multi-tile object
DEF_SUB_TILE = 0x16  # a part's place from its lead, unturned: x in the low byte, y in the high
DEF_ANCHOR = 0x3A  # set on the tile ISimsMultiTileObjectModel::SetObjOrient stands the model on
# ApplyMatrix turns a model by its direction, then EMat4::SwapXY swaps x and y. read_object swaps
# positions already, so models are left to mirror and turn the other way: glTF's turn about Y
# runs x towards -z, as the game's turn about Z runs x towards y.
MODEL_MIRROR = [1.0, 1.0, -1.0]
BOX_SLACK = 0.05  # metres a tile's middle may be outside a model's box and still count as covered
TYPE_PORTAL = 8
TYPE_REMOVED = 9  # DoStream swaps these for a placeholder and kills it
RELATIONS_NEW_FORM = -1  # RelMatrixImpl::DoStream: first word of the form every house uses
MT_INTS = 6
DEF_RES_DATA = 0xC0  # in ObjDefinition: a pointer to its ObjDefinition::ResData row
ANIM_DEF_SIZE = 0x20  # ObjAnimDef
ANIM_DEF_ANIMATION = 8
ANIM_DEF_GRAPHIC = 0x18
RES_RIG = 8  # the word of a ResData row naming the rig, when the model has one
ANY_GRAPHIC = -1  # in the first ObjAnimDef: the graphic is simply an index
GRAPHIC_KEY = 0  # the variable holding an object's graphic
HIDDEN_KEY = 0x22  # non-zero fails ISimsObjectModel::IsVisible
HIDES_ON_LOAD = {'zz_move_a__tron'}  # sets HIDDEN_KEY in its load tree; the only such type placed
# Stand-ins, not decoded. Nothing on the disc ties a palm's leaf to its trunk, and this object's
# table row names only a rig, so these come from model names and from looking at the models.
PALM_TRUNK = 'isPalmTreeTrunk'
PALM_LEAF = 'isPalmTreeLeaf'
PALM_CROWN_HEIGHT = 4.12  # treeanimate3 in isTreeSkeleton
PALM_LEAVES = 9
PALM_LEAF_LIFT = 20.0  # degrees above level
BOUNDS_TOLERANCE = 0.001
FADE_LIST = 2  # gamedata's third submodel list
FADE_SUFFIX = ' fade'
MISSING_TEXTURE = 'missingshader'  # the purple 16x16 behind Maya's default materials
ALPHA_ADD = 0x68  # the GS ALPHA register: source * FIX + destination


class BitReader:
    """ReconBuffer in read mode: most significant bit first, with marks at byte boundaries."""

    def __init__(self, data, pos):
        self.data = data
        self.pos = pos
        self.mask = 0
        self.compressed = False

    def bits(self, count):
        value = 0
        for _ in range(count):
            if self.mask == 0:
                if self.pos >= len(self.data):
                    raise arc.FormatError('an object stream runs off its chunk')
                self.mask = 0x80
                self.pos += 1
            value = value << 1 | (1 if self.data[self.pos - 1] & self.mask else 0)
            self.mask >>= 1
        return value

    def compressed_int(self, width):
        """ReconCmprInt: a bit for non-zero, two bits picking a width, then a signed value."""
        if not self.bits(1):
            return 0
        size = SCHEMES[width][self.bits(2)]
        value = self.bits(size)
        return value - (1 << size) if value >> (size - 1) & 1 else value

    def integer(self, width):
        if self.compressed:
            return self.compressed_int(width)
        size = width // 8
        if self.pos + size > len(self.data):
            raise arc.FormatError('an object stream runs off its chunk')
        self.mask = 0
        raw = self.data[self.pos:self.pos + size]
        self.pos += size
        return int.from_bytes(raw, 'little', signed=True)

    def mark(self):
        """ReconMark: drop to a byte boundary and read where the next mark is."""
        self.mask = 0
        if self.pos + 4 > len(self.data):
            raise arc.FormatError('an object stream has no mark')
        value = struct.unpack_from('<I', self.data, self.pos)[0]
        self.pos += 4
        return value

    def jump(self, pos):
        self.pos = pos
        self.mask = 0


@dataclass
class StackFrame:
    """StackElem::ReconStream."""
    stack_object: int
    tree: int
    node: int
    params: list
    locals: list
    waiting: int
    owner: int  # the selector's number in objt, which says whose trees these are


@dataclass
class PlacedObject:
    object_id: int
    guid: int
    name: str
    box: tuple  # minimum x, minimum y, maximum x, maximum y, in sixteenths of a tile
    centre: tuple
    level: int
    attributes: list
    semi_attributes: list
    static_attributes: list
    temps: list
    variables: dict
    stack: list  # StackFrame, bottom first
    slots: list  # object ID in each object slot
    state: int  # ObjectModule's word for this ID: low 16 bits idle status, high 16 Sim flags
    leader: int  # the lead tile's ID on a multi-tile object, else 0

    @property
    def on_lot(self):
        return self.box[0] >= 0  # controllers sit at -16, -16

    @property
    def metres(self):
        return self.centre[0] / UNITS_PER_TILE, self.centre[1] / UNITS_PER_TILE

    @property
    def direction(self):
        return self.variables.get(DIRECTION_KEY, 0)


def read_variables(reader):
    """EdithVariableSet::ReconStream: a 72-bit set of keys, then one value for each."""
    words = [reader.integer(32) & 0xFFFFFFFF, reader.integer(32) & 0xFFFFFFFF,
             reader.integer(8) & 0xFF]
    keys = words[0] | words[1] << 32 | words[2] << 64
    return {k: reader.integer(16) for k in range(VARIABLE_KEYS) if keys >> k & 1}


def read_counted(reader):
    return [reader.integer(16) for _ in range(reader.integer(16))]


def read_frame(reader):
    stack_object, tree, node = reader.integer(16), reader.integer(16), reader.integer(16)
    local_count, param_count = reader.integer(8) & 0xFF, reader.integer(8) & 0xFF
    params = [reader.integer(16) for _ in range(param_count)]
    local_values = [reader.integer(16) for _ in range(local_count)]
    return StackFrame(stack_object, tree, node, params, local_values, reader.integer(32), reader.integer(16))


def read_relations(reader):
    """RelMatrixImpl::DoStream, in the form used from version 0x54. No house has any entries."""
    if reader.integer(32) != RELATIONS_NEW_FORM:
        raise arc.FormatError('ObjM: old relationship form')
    for _ in range(reader.integer(32)):
        if reader.integer(32):
            reader.integer(32)
            for _ in range(reader.integer(32)):
                reader.integer(32)


def read_object(reader, object_id, kind, shape):
    """cXObject::ReconStream as the PS2 code reads it, with cXMTObject's fields, then the state."""
    values = [reader.integer(32) for _ in range(6)]
    level = reader.integer(32)
    reader.integer(16)  # always 1
    attributes, semi_attributes, static_attributes = (read_counted(reader) for _ in range(3))
    temps = [reader.integer(16) for _ in range(TEMPS)]
    variables = read_variables(reader)
    frames = reader.integer(32)
    reader.integer(32)  # memory reserved for the stack
    stack = [read_frame(reader) for _ in range(frames)]
    read_relations(reader)
    slots = [(reader.integer(16), reader.integer(16))[1] for _ in range(reader.integer(16))]
    read_counted(reader)  # scope 0x1b's array, empty in every house
    leader = 0
    if shape[1]:
        if shape[0] == TYPE_PORTAL:
            raise arc.FormatError('ObjM: a portal object, which no house has')
        for _ in range(MT_INTS):
            reader.integer(32)
        leader = reader.integer(16)
    state = reader.integer(32) & 0xFFFFFFFF
    return PlacedObject(object_id, kind[0], kind[2], tuple(values[:4]), (values[5], values[4]), level,
                        attributes, semi_attributes, static_attributes, temps, variables, stack, slots,
                        state, leader)


def read_objm(data, types, shapes):
    """ObjectModule::DoStream: a list of ids and types, then each object between two marks.

    Every object must end exactly on its mark. The exception is type 9, which the game replaces
    and skips to the mark. Shapes come from object_shapes, and a type it lacks is single-tile.
    """
    if data[8:12] != b'MjbO':
        raise arc.FormatError('ObjM: bad tag')
    reader = BitReader(data, MARK_BASE)
    reader.compressed = reader.integer(8) != 0
    listed = []
    object_id = reader.integer(16)
    while object_id:
        kind = reader.integer(16)
        if not 0 < kind <= len(types):
            raise arc.FormatError('ObjM: type %d is not in the list' % kind)
        listed.append((object_id, types[kind - 1]))
        object_id = reader.integer(16)
    next_mark = reader.mark()
    objects = []
    for object_id, kind in listed:
        end = next_mark + MARK_BASE
        if not reader.pos < end <= len(data) - 4:
            raise arc.FormatError('ObjM: a mark is out of order')
        shape = shapes.get(kind[0], (0, 0))
        objects.append(read_object(reader, object_id, kind, shape))
        if reader.pos > end or reader.pos < end and shape[0] != TYPE_REMOVED:
            raise arc.FormatError('ObjM: object %d does not end on its mark' % object_id)
        reader.jump(end)
        next_mark = reader.mark()
    if next_mark != 0:
        raise arc.FormatError('ObjM: the last mark is not zero')
    return objects


def house_streams(disc):
    """Return (object types, ObjM chunk) for each house, in file order."""
    entry = disc.find(gd.NGH_FILE)
    sections = gd.read_ngh_sections(disc.read(entry.lba, 0, entry.size))
    streams = []
    for house in (s for s in sections if s.kind == gd.NGH_KIND_HOUSE):
        chunks = {(kind, number): data for kind, number, data in gd.house_chunks(house)}
        streams.append((gd.read_objt(chunks[('objt', 0)]), chunks[('ObjM', 1)]))
    return streams


def object_shapes(disc):
    """Return {GUID: (object type, multi-tile word)} from ObjDefinition."""
    data, table = gd.find_table(gd.load_quickdata(disc), 'SimsObjects/ObjDefinition')
    rows = (table.rows + i * table.stride for i in range(table.count))
    return {data.word(p + DEF_GUID): struct.unpack_from('<2h', data.image, p + DEF_TYPE) for p in rows}


def read_houses(disc):
    """Return {lot name: placed objects}."""
    shapes = object_shapes(disc)
    return {'lot_%02d' % (index + FIRST_LOT_OFFSET): read_objm(data, types, shapes)
            for index, (types, data) in enumerate(house_streams(disc))}


def read_anim_defs(data, definition, names):
    """Return [(graphic, model, animation)]: a ResData row starts with VECTOR<ObjAnimDef>."""
    if definition + DEF_RES_DATA not in data.pointers:
        return []
    start, count = data.vector(data.word(definition + DEF_RES_DATA))
    anims = []
    for index in range(count):
        pos = start + index * ANIM_DEF_SIZE
        graphic = struct.unpack('<i', struct.pack('<I', data.word(pos + ANIM_DEF_GRAPHIC)))[0]
        anims.append((graphic, names.get(data.word(pos)),
                      names.get(data.word(pos + ANIM_DEF_ANIMATION))))
    return anims


def read_rig(data, definition, names):
    if definition + DEF_RES_DATA not in data.pointers:
        return None
    return names.get(data.word(data.word(definition + DEF_RES_DATA) + RES_RIG * 4))


def model_table(disc):
    """Return {GUID: (resource file name, [(graphic, model, animation)], rig)}."""
    quickdata = gd.load_quickdata(disc)
    names = gd.resource_names(disc)
    data, definitions = gd.find_table(quickdata, 'SimsObjects/ObjDefinition')
    _, files = gd.find_table(quickdata, 'SimsObjects/ResFile')
    rows = {definitions.rows + index * definitions.stride for index in range(definitions.count)}
    table = {}
    for index in range(files.count):
        name = data.row_name(files, index)
        start, count = data.vector(files.rows + index * files.stride)
        for item in range(count):
            definition = data.word(start + 4 * item)
            if definition in rows:
                table[data.word(definition + DEF_GUID)] = (
                    name, read_anim_defs(data, definition, names),
                    read_rig(data, definition, names))
    return table


def anim_def(anims, graphic):
    """ISimsObjectModel::GetAnimDef: pick by graphic, falling back on the first."""
    index = graphic
    if anims[0][0] != ANY_GRAPHIC:
        index = next((i for i, anim in enumerate(anims) if anim[0] == graphic), 0)
    return anims[index if 0 <= index < len(anims) else 0]


def is_hidden(placed, table):
    entry = table.get(placed.guid)
    return bool(placed.variables.get(HIDDEN_KEY)) or (entry is not None and entry[0] in HIDES_ON_LOAD)


def object_model(placed, table, available):
    """The model an object shows for its saved graphic, if any. Trees may change both later."""
    entry = table.get(placed.guid)
    if entry is None or not entry[1] or is_hidden(placed, table):
        return None
    name = anim_def(entry[1], placed.variables.get(GRAPHIC_KEY, 0))[1]
    return name if name in available else None


def undrawn(objects, table, available):
    """Visible objects on the lot that nothing draws. A multi-tile object counts as drawn when any
    of its tiles has a model: one definition per object has a ResData row, usually the lead's."""
    drawn = {p.leader or p.object_id for p in objects if object_model(p, table, available)}
    return [p for p in objects if p.on_lot and not is_hidden(p, table)
            and (p.leader or p.object_id) not in drawn]


def object_pose(placed, table):
    """Return (animation, rig) for an object's saved graphic, either or both None."""
    entry = table[placed.guid]
    return anim_def(entry[1], placed.variables.get(GRAPHIC_KEY, 0))[2], entry[2]


def anchor_guids(disc):
    """GUIDs of the definitions that mark a multi-tile object's anchor tile."""
    data, table = gd.find_table(gd.load_quickdata(disc), 'SimsObjects/ObjDefinition')
    rows = (table.rows + i * table.stride for i in range(table.count))
    return {data.word(p + DEF_GUID) for p in rows if struct.unpack_from('<h', data.image, p + DEF_ANCHOR)[0] > 0}


def model_anchor(item, objects, anchors):
    """Where the game stands an object's model, in metres. ISimsObjectModel::SetObjOrient uses its
    tile. ISimsMultiTileObjectModel's uses a tile its definition marks, else the middle of the box
    round all the object's tiles, so the model isn't on the tile that owns it."""
    group = item.leader or item.object_id
    tiles = [o for o in objects if (o.leader or o.object_id) == group]
    marked = [o for o in tiles if o.guid in anchors]
    if len(tiles) == 1 or marked:
        return (marked or [item])[-1].metres
    xs, ys = zip(*(o.metres for o in tiles))
    return (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2


def model_rotation(direction):
    """glTF rotation for a placed model, with MODEL_MIRROR as its scale. The game turns by
    direction eighths; after the swap that's the same angle the other way, a quarter turn on."""
    half = (math.pi / 2 - direction * EIGHTH_TURN) / 2
    return [0.0, math.sin(half), 0.0, math.cos(half)]


def turned_part(lead_direction, sub_tile):
    """A part's place from its lead, in tiles: its definition's offset turned by the lead's
    direction, clockwise as the routing code's direction steps go."""
    x, y = struct.unpack('<2b', struct.pack('<H', sub_tile))
    for _ in range((lead_direction // 2) % 4):
        x, y = y, -x
    return x, y


def model_point(point, anchor, direction, game_rule=True):
    """Where a point on the lot falls in a placed model's own x, y. The control, game_rule=False,
    is the old export's rule: the model turned the other way, unmirrored."""
    dx, dy = point[0] - anchor[0], point[1] - anchor[1]
    turn = -(math.pi / 2 - direction * EIGHTH_TURN) if game_rule else -direction * EIGHTH_TURN
    c, s = math.cos(turn), math.sin(turn)
    x, y = c * dx - s * dy, s * dx + c * dy
    return (x, -y) if game_rule else (x, y)


def check_facing(disc, houses, table):
    """Multi-tile objects test the turning rule: parts must land where their definition says, and
    a model should cover its tiles. Portals' tiles are where Sims walk, so a few won't be covered."""
    data, definitions = gd.find_table(gd.load_quickdata(disc), 'SimsObjects/ObjDefinition')
    rows = {data.word(definitions.rows + i * definitions.stride + DEF_GUID):
            definitions.rows + i * definitions.stride for i in range(definitions.count)}
    anchors = anchor_guids(disc)
    library = gltf.Library(disc)
    available = {e.name: (e, raw) for e, raw in mdl.each_model(disc, library.shaders)}
    boxes = {}
    parts = {True: 0, False: 0}
    covered = collections.Counter()
    tiles_seen = objects_seen = 0
    for objects in houses:
        by_id = {o.object_id: o for o in objects}
        for item in objects:
            if not item.leader:
                continue
            lead = by_id[item.leader]
            sub_tile = struct.unpack_from('<H', data.image, rows[item.guid] + DEF_SUB_TILE)[0]
            at = tuple(round(a - b) for a, b in zip(item.metres, lead.metres))
            parts[True] += turned_part(lead.direction, sub_tile) == at
            parts[False] += turned_part((8 - lead.direction) % 8, sub_tile) == at
        groups = collections.defaultdict(list)
        for item in objects:
            if item.on_lot and not is_hidden(item, table):
                groups[item.leader or item.object_id].append(item)
        for tiles in groups.values():
            drawn = [(o, object_model(o, table, available)) for o in tiles]
            drawn = [(o, name) for o, name in drawn if name]
            if len(tiles) < 2 or not drawn:
                continue
            item, name = drawn[0]
            if name not in boxes:
                points = [(p[0] * m.scale, p[1] * m.scale)
                          for m in mdl.parse_model(available[name][1], library.shaders, name)
                          for b in m.batches for p in b.positions]
                boxes[name] = [f(p[i] for p in points) for f in (min, max) for i in (0, 1)]
            x0, y0, x1, y1 = boxes[name]
            objects_seen += 1
            tiles_seen += len(tiles)
            for rule, anchor in (('game', model_anchor(item, objects, anchors)), ('old', item.metres)):
                inside = [x0 - BOX_SLACK <= x <= x1 + BOX_SLACK and y0 - BOX_SLACK <= y <= y1 + BOX_SLACK
                          for x, y in (model_point(t.metres, anchor, item.direction, rule == 'game')
                                       for t in tiles)]
                covered[(rule, 'tiles')] += sum(inside)
                covered[(rule, 'objects')] += all(inside)
    total = sum(1 for objects in houses for o in objects if o.leader)
    gd.report('parts where their offset turns', parts[True], total,
              'the diving platform on lot_24 has two odd offsets')
    print('  control: turned the other way, %d of %d parts land' % (parts[False], total))
    passed = parts[True] >= total - 2 and parts[False] < parts[True]
    for rule in ('game', 'old'):
        print('%-34s %7d of %7d objects, %d of %d tiles' % (
            'multi-tile models over their tiles' if rule == 'game' else '  control: the old export\'s rule',
            covered[(rule, 'objects')], objects_seen, covered[(rule, 'tiles')], tiles_seen))
    passed &= covered[('game', 'tiles')] > covered[('old', 'tiles')]
    return passed


def lot_fit(objects, level):
    """The share of a lot's objects on set cells of whichever of its grids suits them best."""
    points = [o.metres for o in objects if o.on_lot]
    grids = [g for g in level.grids if g.width]
    if not points or not grids:
        return 0.0
    return max(sum(bool(g.get(math.floor(x), math.floor(y))) for x, y in points)
               for g in grids) / len(points)


def fit_at_offset(houses, levels, offset):
    """Mean fit when house N is paired with lot N + offset. The wrong offsets are the control."""
    fits = [lot_fit(objects, levels['lot_%02d' % (index + offset)])
            for index, objects in enumerate(houses) if 'lot_%02d' % (index + offset) in levels]
    return sum(fits) / len(fits)


def check_submodels(disc, levels):
    """Each level's submodel bounds must be the exact box of the meshes read for that submodel."""
    library = gltf.Library(disc)
    models = {e.name: raw for e, raw in mdl.each_model(disc, library.shaders)}
    worst = 0.0
    checked = 0
    for name, level in levels.items():
        if not name.startswith('lot_') or name not in models:
            continue
        meshes = mdl.parse_model(models[name], library.shaders, name)
        groups = mdl.submodels(models[name], meshes, library.shaders)
        for instance in level.submodels:
            points = [[c * m.scale for c in p] for m in groups[instance.submodel]
                      for b in m.batches for p in b.positions]
            found = [min(p[i] for p in points) for i in range(3)] \
                + [max(p[i] for p in points) for i in range(3)]
            worst = max(worst, max(abs(a - b) for a, b in zip(found, instance.box)))
            checked += 1
    print('%d lot submodels match their bounds, worst by %.4f m' % (checked, worst))
    return checked > 0 and worst < BOUNDS_TOLERANCE


def check(disc):
    streams = house_streams(disc)
    shapes = object_shapes(disc)
    houses = [read_objm(data, types, shapes) for types, data in streams]
    table = model_table(disc)
    models = {e.name for e in arc.read_toc(disc, arc.find_archive(disc, mdl.MODEL_ARCHIVE))}
    totals = collections.Counter()
    for objects in houses:
        for placed in objects:
            totals['objects'] += 1
            totals['on a lot'] += placed.on_lot
            totals['with a type in ResFile'] += placed.guid in table
            totals['with a model'] += object_model(placed, table, models) is not None
            totals['turned'] += placed.direction != 0
            totals['with its ID in variable 0xB'] += placed.variables.get(OBJECT_ID_KEY) == placed.object_id
            totals['with one stack frame at node 0'] += len(placed.stack) == 1 and placed.stack[0].node == 0
    for key, value in sorted(totals.items()):
        print('%-26s %6d' % (key, value))
    passed = gd.report('houses end on a zero mark', len(houses), len(streams))
    missing = [p for objects in houses for p in undrawn(objects, table, models)]
    on_lot = sum(p.on_lot and not is_hidden(p, table) for objects in houses for p in objects)
    passed &= gd.report('visible objects something draws', on_lot - len(missing), on_lot,
                        ' '.join(sorted({p.name or hex(p.guid) for p in missing})))

    types, data = streams[0]
    print('controls:')
    passed &= gd.expect_failure('a stream one byte out of place',
                                lambda: read_objm(data[:MARK_BASE] + data[MARK_BASE + 1:], types, shapes))
    passed &= gd.expect_failure('a stream cut short', lambda: read_objm(data[:-8], types, shapes))
    passed &= gd.expect_failure('an empty type list', lambda: read_objm(data, [], shapes))
    passed &= gd.expect_failure('every object read as single-tile', lambda: read_objm(data, types, {}))
    unreachable = sum(len(undrawn(objects, table, set())) for objects in houses)
    print('  control: with no models on the disc, %d visible objects are undrawn' % unreachable)
    passed &= unreachable > 0

    archive = arc.find_archive(disc, 'LEVELS')
    levels = {e.name: gd.read_level(disc.read(archive.lba, e.offset, e.size), e.name)
              for e in arc.read_toc(disc, archive)}
    print('house N against lot N + offset:')
    scores = {offset: fit_at_offset(houses, levels, offset) for offset in (1, 0, 2, -1, 3, 7, 13)}
    for offset, score in scores.items():
        print('  offset %+3d: mean fit %.3f' % (offset, score))
    best = max(scores, key=scores.get)
    passed &= best == FIRST_LOT_OFFSET
    passed &= check_submodels(disc, levels)
    passed &= check_facing(disc, houses, table)
    print('failures: %d' % (not passed))
    return 0 if passed else 1


def list_lot(disc, lot):
    objects = read_houses(disc).get(lot)
    if objects is None:
        raise arc.FormatError('no placed objects for %s' % lot)
    table = model_table(disc)
    models = {e.name for e in arc.read_toc(disc, arc.find_archive(disc, mdl.MODEL_ARCHIVE))}
    for placed in objects:
        x, y = placed.metres
        print('%7.2f %7.2f turn %d  %-30s %s' % (x, y, placed.direction, placed.name[:30],
                                                 object_model(placed, table, models) or '-'))
    return 0


def ground_heights(meshes):
    """Return {(x, y): height} on a one-metre grid: the highest surface over each cell."""
    cells = {}
    for mesh in meshes:
        for batch in mesh.batches:
            points = [[c * mesh.scale for c in p] for p in batch.positions]
            for a, b, c in (tuple(points[i] for i in t) for t in mdl.triangles(batch)):
                add_triangle(cells, a, b, c)
    return cells


def add_triangle(cells, a, b, c):
    area = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
    if abs(area) < 1e-9:
        return
    for y in range(math.floor(min(a[1], b[1], c[1])), math.ceil(max(a[1], b[1], c[1])) + 1):
        for x in range(math.floor(min(a[0], b[0], c[0])), math.ceil(max(a[0], b[0], c[0])) + 1):
            px, py = x + 0.5, y + 0.5
            u = ((b[1] - c[1]) * (px - c[0]) + (c[0] - b[0]) * (py - c[1])) / area
            v = ((c[1] - a[1]) * (px - c[0]) + (a[0] - c[0]) * (py - c[1])) / area
            if u >= 0 and v >= 0 and u + v <= 1:
                z = u * a[2] + v * b[2] + (1 - u - v) * c[2]
                cells[(x, y)] = max(z, cells.get((x, y), z))


def adds_nothing(shader):
    """True if every pass blends to source * 0 + destination: the GS leaves the screen as it was."""
    return shader is not None and all(p.flags & mdl.PASS_BLEND_FLAG and p.alpha == ALPHA_ADD
                                      and p.fix == 0 for p in shader.passes)


def textured(meshes, library):
    """Leave out what isn't seen: meshes that add nothing, and those on the purple placeholder.

    What the game does with the placeholder is open. docs/formats/world.md has what's known.
    """
    def is_hidden(mesh):
        texture = library.texture_for(mesh.shader_hash)
        return (texture is not None and texture.name == MISSING_TEXTURE) \
            or adds_nothing(library.shader(mesh.shader_hash))
    return [mesh for mesh in meshes if not is_hidden(mesh)]


def add_mesh(builder, library, name, meshes, cache):
    primitives = []
    for mesh in textured(meshes, library):
        material = gltf.add_material(builder, library, mesh.shader_hash, cache, layers=True)
        primitive = gltf.add_primitive(builder, mesh, material, False,
                                       library.shader(mesh.shader_hash))
        if primitive:
            primitives.append(primitive)
    builder.gltf['meshes'].append({'name': name, 'primitives': primitives})
    return len(builder.gltf['meshes']) - 1


def multiply(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def euler_quaternion(degrees):
    """SetEuler: Qz * Qy * Qx, so X applies first."""
    halves = [math.radians(d) / 2 for d in degrees]
    qx = (math.sin(halves[0]), 0.0, 0.0, math.cos(halves[0]))
    qy = (0.0, math.sin(halves[1]), 0.0, math.cos(halves[1]))
    qz = (0.0, 0.0, math.sin(halves[2]), math.cos(halves[2]))
    return multiply(multiply(qz, qy), qx)


def palm_crown(builder, leaf_mesh):
    """Return child nodes for a palm: one frond repeated round the top of the trunk."""
    children = []
    for index in range(PALM_LEAVES):
        turn = euler_quaternion((0.0, PALM_LEAF_LIFT, index * 360.0 / PALM_LEAVES))
        children.append(builder.add_node({
            'name': 'stand-in leaf %d' % index, 'mesh': leaf_mesh,
            'translation': list(gltf.to_y_up((0.0, 0.0, PALM_CROWN_HEIGHT))),
            'rotation': list(gltf.quaternion_to_y_up(turn))}))
    return children


def read_lot_level(disc, lot):
    archive = arc.find_archive(disc, 'LEVELS')
    entry = next(e for e in arc.read_toc(disc, archive) if e.name == lot)
    return gd.read_level(disc.read(archive.lba, entry.offset, entry.size), entry.name)


def split_terrain(terrain, data, level, shader_hashes):
    """Return (drawn ground, list 3). List 3 is the floor Sims stand on, and the game never draws it."""
    groups = mdl.submodels(data, terrain, shader_hashes)
    ground, fading = [], []
    for reference, instance in enumerate(level.submodels, 1):
        (fading if reference in level.lists[FADE_LIST] else ground).extend(groups[instance.submodel])
    return ground, fading


def export_lot(disc, out_path, lot, posed=False):
    objects = read_houses(disc).get(lot)
    if objects is None:
        raise arc.FormatError('no placed objects for %s' % lot)
    if lot == MISMATCHED_LOT:
        print('warning: this house looks like an older lot_27, so objects will be misplaced')
    library = gltf.Library(disc)
    table = model_table(disc)
    anchors = anchor_guids(disc)
    available = {e.name: (e, raw) for e, raw in mdl.each_model(disc, library.shaders)}
    terrain = mdl.parse_model(available[lot][1], library.shaders, lot)
    level = read_lot_level(disc, lot)
    solid, fading = split_terrain(terrain, available[lot][1], level, library.shaders)
    # Objects go on the drawn ground. List 3 counts only where that has none.
    heights = {**ground_heights(textured(fading, library)),
               **ground_heights(textured(solid, library))}
    builder = gltf.GlbBuilder()
    cache = {}
    meshes = {}
    ground = add_mesh(builder, library, lot, solid, cache)
    nodes = [builder.add_node({'name': lot, 'mesh': ground})]
    if textured(fading, library):
        name = lot + FADE_SUFFIX
        nodes.append(builder.add_node({
            'name': name, 'mesh': add_mesh(builder, library, name, fading, cache)}))

    def mesh_for(name):
        if name not in meshes:
            parsed = mdl.parse_model(available[name][1], library.shaders, name)
            meshes[name] = add_mesh(builder, library, name, parsed, cache) if parsed else None
        return meshes[name]

    if posed:
        import anim  # only here, as pose.py needs this module
        import pose
        animations = pose.load_animations(disc)

    def posed_mesh_for(name, item):
        """The mesh for an object, standing as its animation says if it has one and a rig."""
        animation_name, rig = object_pose(item, table)
        if not posed or not animation_name or not rig or rig.lower() not in library.characters:
            return mesh_for(name)
        key = (name, animation_name)
        if key not in meshes:
            parsed = mdl.parse_model(available[name][1], library.shaders, name)
            animation = anim.parse_animation(animations[animation_name], animation_name)
            stood = pose.pose_meshes(parsed, library.characters[rig.lower()], animation)
            meshes[key] = add_mesh(builder, library, '%s %s' % key, stood, cache)                 if pose.triangle_count(stood) else None
        return meshes[key]

    names = gd.resource_names(disc)
    scenery_count = 0
    for index, item in enumerate(level.objects):
        name = names.get(item.model_hash)
        if name not in available or mesh_for(name) is None:
            continue
        sx, sy, sz = item.scale
        node = {
            'name': 'scenery %s %d' % (name, index), 'mesh': meshes[name],
            'translation': list(gltf.to_y_up(item.position)),
            'rotation': list(gltf.quaternion_to_y_up(euler_quaternion(item.rotation))),
            'scale': [sx, sz, sy]}
        if name.startswith(PALM_TRUNK):
            node['children'] = palm_crown(builder, mesh_for(PALM_LEAF))
        nodes.append(builder.add_node(node))
        scenery_count += 1
    placed_count = 0
    for placed in objects:
        name = object_model(placed, table, available) if placed.on_lot else None
        mesh = posed_mesh_for(name, placed) if name else None
        if mesh is None:
            continue
        x, y = model_anchor(placed, objects, anchors)
        z = heights.get((math.floor(x), math.floor(y)), 0.0)
        node = {'name': 'placed %s %d' % (placed.name, placed.object_id), 'mesh': mesh,
                'translation': list(gltf.to_y_up((x, y, z))),
                'rotation': model_rotation(placed.direction), 'scale': MODEL_MIRROR}
        if name.startswith(PALM_TRUNK):
            node['children'] = palm_crown(builder, mesh_for(PALM_LEAF))
        nodes.append(builder.add_node(node))
        placed_count += 1
    builder.gltf['scenes'][0]['nodes'] = nodes
    with open(out_path, 'wb') as out:
        out.write(builder.to_bytes())
    print('wrote %s: %d of %d objects, %d of %d scenery, %d models'
          % (out_path, placed_count, len(objects), scenery_count, len(level.objects), len(meshes)))
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 4 and argv[1] == 'list':
        return list_lot(arc.Disc(argv[2]), argv[3])
    if len(argv) in (5, 6) and argv[1] == 'lot' and argv[5:] in ([], ['--posed']):
        return export_lot(arc.Disc(argv[2]), argv[3], argv[4], len(argv) == 6)
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
