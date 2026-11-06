#!/usr/bin/env python3
"""Reshape a Sim's parsed meshes the way the game does: body lattices and face targets.

Usage:
  python -I tools/morph.py check <game.iso>

check parses every lattice in every model, proves each array ends exactly where the next
data starts, proves an unmorphed lattice gives back every vertex inside it, and checks all
the face targets. Everything else here is for tools to import. For each part:

  lattices = read_lattices(data, name)[0]
  apply_lattices(meshes, data, lattices, part_code, lattice_values, name)

then for the face, after its lattices:

  wanted = face_targets_wanted('af', morph_target_values)    # [(model name, weight)]
  apply_face(face_meshes, [(parsed[name], weight) for name, weight in wanted])

part_code is the model name's second field, such as ti. lattice_values and
morph_target_values are a CasSimDescription's four floats. Both functions change
batch.positions in place and leave normals alone, as the game does.
"""
import collections
import math
import os
import struct
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import model as mdl  # noqa: E402

# The header before the lattice array: a size, a 48-byte block, a byte and a word.
LATTICE_COUNT_OFFSET = 0x3D
LATTICE_VERSION = 0
LATTICE_NAME_SIZE = 0x80
LATTICE_HEADER = struct.Struct('<5i')  # id, then x, y and z sizes, then frames
MAX_LATTICES = 16
MAX_GRID = 32
MAX_FRAMES = 32
MIN_GRID = 4  # the cubic reads four control points on each axis
# After the lattices: dummy, camera and light counts, a byte, the model's position scale,
# the submodel count, then 12 bytes more up to the first mesh record.
AFTER_LATTICES_TO_FIRST_RECORD = 33
ARRAYS_AFTER_LATTICES = 3

SLOT_TAG = 4  # the first byte of a vertex's slot 4 is half its lattice's id
TAG_TO_ID = 2

S16_RANGE = 1 << 16
NEUTRAL_TOLERANCE = 1.5  # 4096ths: a vertex and the lattice each round once
FIXED_ONE = mdl.FIXED_ONE

# Sim::LatticeMorphTSC6 and the morph table, read from Sim::BodyPartTypeInfoTableTSC6.
HEAD_REGION, TORSO_REGION, ARM_REGION, LEG_REGION = 0, 1, 2, 3
BODY_NORMAL = 1.0
BODY_MAX = 2.0
HEAD_NORMAL = 3.0  # the head has no keyframe for this value: it is the unmorphed shape
HEAD_MAX = 7.0
HEAD_KEYFRAMES = 7
FACE_NORMAL = 1.0
FACE_STOPS = (0.0, 2.0, 3.0)

# eBodyPart by model code.
FACE_PART = 'ft'
BODY_PARTS = {'ft': 0x1A, 'ff': 0x1B, 'hh': 0x1C, 'hb': 0x1C, 'ha1': 0x1C, 'ha2': 0x1C,
              'ha3': 0x1C, 'gl': 0x1D, 'ti': 0x1E, 'lb': 0x1F, 'sh': 0x20, 'ht': 0x21,
              'jw': 0x22, 'bc': 0x23, 'bt': 0x24, 'slti': 0x27}
HEAD_LATTICE_PARTS = (0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x21, 0x22)
# MorphUtilities::ApplyLattice, with GetLatticeIndex folded in: for each body region, which
# lattice in a part's list it drives. It goes by place in the list, not by name.
BODY_LATTICES = {
    TORSO_REGION: {0x1A: 1, 0x1B: 1, 0x1C: 1, 0x1E: 1, 0x22: 1, 0x1F: 0, 0x24: 0},
    ARM_REGION: {0x23: None, 0x27: None},  # both lattices in the list
    LEG_REGION: {0x1F: 1},
}

# Region to feature, from SimModelPartHead::GetMorphTargetResID: the head's clothing set
# lists the face, then three targets each for chin, eyes, lips and nose.
FACE_REGIONS = ('eyes', 'nose', 'lips', 'chin')
FACE_CODES = (FACE_PART,) + FACE_REGIONS
FACE_TARGETS = {'chin': ('1', '2', '3'), 'eyes': ('big', 'asian', 'caveman'),
                'lips': ('full', 'narrow', 'thin'), 'nose': ('flat', 'hook', 'short')}


@dataclass
class Lattice:
    """A BSplineVolume. Frame 0 holds points; later frames hold offsets from frame 0."""
    name: str
    lattice_id: int
    size: tuple
    frames: list
    low: tuple = (0.0, 0.0, 0.0)
    scale: tuple = (0.0, 0.0, 0.0)

    def point(self, frame, i, j, k):
        return frame[(i * self.size[1] + j) * self.size[2] + k]


def wrap_s16(value):
    return (value + S16_RANGE // 2) % S16_RANGE - S16_RANGE // 2


def read_points(data, pos, count, name):
    if pos + count * 12 > len(data):
        raise arc.FormatError('%s: a lattice runs off the end of the file' % name)
    values = struct.unpack_from('<%df' % (count * 3), data, pos)
    if not all(math.isfinite(value) for value in values):
        raise arc.FormatError('%s: a lattice point is not a number' % name)
    fixed = [wrap_s16(int(value * FIXED_ONE)) for value in values]
    return [tuple(fixed[n:n + 3]) for n in range(0, len(fixed), 3)]


def bounds(lattice):
    """Set the box that maps a position into the grid: the inner points of frame 0."""
    nx, ny, nz = lattice.size
    inner = [lattice.point(lattice.frames[0], i, j, k) for i in range(1, nx - 1)
             for j in range(1, ny - 1) for k in range(1, nz - 1)]
    low = tuple(min(p[axis] for p in inner) / FIXED_ONE for axis in range(3))
    high = tuple(max(p[axis] for p in inner) / FIXED_ONE for axis in range(3))
    lattice.low = low
    lattice.scale = tuple(1.0 / (h - l) if h > l else 0.0 for l, h in zip(low, high))


def read_lattice(data, pos, name):
    if pos + 4 + LATTICE_NAME_SIZE + LATTICE_HEADER.size > len(data):
        raise arc.FormatError('%s: a lattice runs off the end of the file' % name)
    if struct.unpack_from('<I', data, pos)[0] != LATTICE_VERSION:
        raise arc.FormatError('%s: unknown lattice version' % name)
    pos += 4
    raw = data[pos:pos + LATTICE_NAME_SIZE]
    label = raw.split(b'\0')[0].decode('ascii', 'replace')
    pos += LATTICE_NAME_SIZE
    lattice_id, nx, ny, nz, frame_count = LATTICE_HEADER.unpack_from(data, pos)
    pos += LATTICE_HEADER.size
    if not all(MIN_GRID <= n <= MAX_GRID for n in (nx, ny, nz)) \
            or not 1 < frame_count <= MAX_FRAMES:
        raise arc.FormatError('%s: lattice %s has a bad size' % (name, label))
    count = nx * ny * nz
    frames = []
    for _ in range(frame_count):
        frames.append(read_points(data, pos, count, name))
        pos += count * 12
    base = frames[0]
    for frame in frames[1:]:  # GenerateControlPointDeltas
        frame[:] = [tuple(wrap_s16(p[axis] - b[axis]) for axis in range(3))
                    for p, b in zip(frame, base)]
    lattice = Lattice(label, lattice_id, (nx, ny, nz), frames)
    bounds(lattice)
    return lattice, pos


def read_lattices(data, name='model'):
    """Return (lattices, offset just past them), checking what follows them."""
    if len(data) < mdl.PREFIX_SIZE:
        raise arc.FormatError('%s is too short for a header' % name)
    start = mdl.PREFIX_SIZE + struct.unpack_from('<I', data, 12)[0] + LATTICE_COUNT_OFFSET
    if start + 4 > len(data):
        raise arc.FormatError('%s has no lattice count' % name)
    count = struct.unpack_from('<I', data, start)[0]
    if count > MAX_LATTICES:
        raise arc.FormatError('%s: too many lattices' % name)
    pos = start + 4
    lattices = []
    for _ in range(count):
        lattice, pos = read_lattice(data, pos, name)
        lattices.append(lattice)
    return lattices, pos


def batch_tags(data, name='model'):
    """Return each batch's slot 4 first bytes, in the order parse_model lists batches."""
    tags = []
    pos = data.find(mdl.BATCH_START)
    while pos >= 0:
        count = 0
        while data.startswith(mdl.BATCH_START, pos):
            slots, pos = mdl.read_slots(data, pos, name)
            size, offset = slots.get(SLOT_TAG, (0, 0))
            tags.append(bytes(data[offset:offset + size * 4:4])
                        if size else bytes(slots.get(mdl.SLOT_POSITION, (0, 0))[0]))
            count += 1
        pos = data.find(mdl.BATCH_START, mdl.read_group_tail(data, pos, count, name)[3])
    return tags


def blended_frame(lattice, weights):
    """BlendVolumes: frame 0 plus weighted offsets, cut to whole 4096ths after each one."""
    points = [list(p) for p in lattice.frames[0]]
    for keyframe, weight in sorted(weights.items()):
        if not weight:
            continue
        offsets = lattice.frames[keyframe + 1]
        for point, offset in zip(points, offsets):
            for axis in range(3):
                value = (point[axis] + offset[axis] * weight)
                point[axis] = wrap_s16(int(value))
    return points


def basis(t):
    """GetPosition's weights for four points: Hermite, with tangents p1-p0 and p3-p2."""
    t2 = t * t
    t3 = t2 * t
    h00 = 2 * t3 - 3 * t2 + 1
    h01 = 3 * t2 - 2 * t3
    h10 = t3 - 2 * t2 + t
    h11 = t3 - t2
    return -h10, h00 + h10, h01 - h11, h11


def parameterize(lattice, position):
    """ParameterizeVert: return (first control point on each axis, fraction on each axis)."""
    cells = []
    fractions = []
    for axis in range(3):
        u = (position[axis] - lattice.low[axis]) * lattice.scale[axis]
        u = min(1.0, max(0.0, u)) * (lattice.size[axis] - 3)
        cell = min(max(int(u), 0), lattice.size[axis] - 4)
        cells.append(cell)
        fractions.append(u - cell)
    return cells, fractions


def evaluate(lattice, points, cells, fractions):
    """GetPosition, in metres."""
    bx, by, bz = (basis(f) for f in fractions)
    ny, nz = lattice.size[1], lattice.size[2]
    x = y = z = 0.0
    for a in range(4):
        for b in range(4):
            row = ((cells[0] + a) * ny + cells[1] + b) * nz + cells[2]
            weight = bx[a] * by[b]
            for c in range(4):
                point = points[row + c]
                w = weight * bz[c]
                x += point[0] * w
                y += point[1] * w
                z += point[2] * w
    return x / FIXED_ONE, y / FIXED_ONE, z / FIXED_ONE


def body_keyframes(value):
    """GetSimBodyLatticeKeyframeIndicesAndWeights: 0 is keyframe 1, 1 none, 2 keyframe 0."""
    if 0.0 <= value < BODY_NORMAL:
        return {1: BODY_NORMAL - value}
    if BODY_NORMAL <= value <= BODY_MAX:
        return {0: value - BODY_NORMAL}
    return {}


def head_keyframes(value):
    """GetSimHeadLatticeKeyframeIndicesAndWeights. Whole values pick a keyframe; 3 is none."""
    value = min(HEAD_MAX, max(0.0, value))
    first = math.floor(value)
    weights = {first: 1.0 - (value - first)}
    if first < HEAD_MAX:
        weights[first + 1] = value - first
    skip = int(HEAD_NORMAL)
    return {k - (k > skip): w for k, w in weights.items() if k != skip}


def lattice_weights(part_code, lattices, values):
    """Return {place in the part's lattice list: {keyframe: weight}}."""
    part = BODY_PARTS.get(part_code)
    weights = {}

    def drive(place, keyframes):
        if place < len(lattices):
            usable = len(lattices[place].frames) - 1
            weights[place] = {k: w for k, w in keyframes.items() if k < usable}

    if part in HEAD_LATTICE_PARTS and lattices \
            and len(lattices[0].frames) - 1 >= HEAD_KEYFRAMES:
        drive(0, head_keyframes(values[HEAD_REGION]))
    for region, parts in BODY_LATTICES.items():
        if part in parts:
            keyframes = body_keyframes(values[region])
            for place in (0, 1) if parts[part] is None else (parts[part],):
                drive(place, keyframes)
    return weights


def tag_lattices(tag_values, lattices):
    """Return {tag: place of the first lattice it names}, as AllocateLatticeParameterVerts."""
    found = {}
    for tag in tag_values:
        for place, lattice in enumerate(lattices):
            if tag and tag * TAG_TO_ID == lattice.lattice_id:
                found[tag] = place
                break
    return found


def apply_lattices(meshes, data, lattices, part_code, values, name='model'):
    """Move each tagged vertex by how far its lattice moves it. Positions stay fixed point."""
    weights = lattice_weights(part_code, lattices, values)
    if part_code != FACE_PART and not any(w for found in weights.values()
                                          for w in found.values()):
        return  # SetLatticeWeight never marked the model dirty; the face always is
    batches = [batch for mesh in meshes for batch in mesh.batches]
    tags = batch_tags(data, name)
    if [len(t) for t in tags] != [len(b.positions) for b in batches]:
        raise arc.FormatError('%s: tags do not line up with its vertices' % name)
    places = tag_lattices({tag for batch_tag in tags for tag in batch_tag}, lattices)
    blends = {place: blended_frame(lattices[place], weights.get(place, {}))
              for place in set(places.values())}
    for batch, batch_tag in zip(batches, tags):
        moved = list(batch.positions)
        for index, tag in enumerate(batch_tag):
            if tag not in places:
                continue
            lattice = lattices[places[tag]]
            position = batch.positions[index]
            metres = [value / FIXED_ONE for value in position]
            cells, fractions = parameterize(lattice, metres)
            target = evaluate(lattice, blends[places[tag]], cells, fractions)
            moved[index] = tuple(p + int((t - m) * FIXED_ONE)
                                 for p, t, m in zip(position, target, metres))
        batch.positions = moved


def face_weights(value):
    """GetSimFacialMorphTargetIndicesAndWeights: return {target: weight}; 1 is no change."""
    low, middle, high = FACE_STOPS
    if low <= value < FACE_NORMAL:
        found = {0: FACE_NORMAL - value}
    elif FACE_NORMAL <= value < middle:
        found = {1: value - FACE_NORMAL}
    elif middle <= value <= high:
        found = {1: high - value, 2: value - middle}
    else:
        found = {}
    return {target: weight for target, weight in found.items() if weight}


def face_target_name(prefix, region, target):
    """Return the model name of one face target, such as af_chin_base_1."""
    feature = FACE_REGIONS[region]
    return '%s_%s_base_%s' % (prefix, feature, FACE_TARGETS[feature][target])


def face_targets_wanted(prefix, values):
    """Return [(model name, weight)] for a Sim's morphTargetValues."""
    return [(face_target_name(prefix, region, target), weight)
            for region, value in enumerate(values) if value != FACE_NORMAL
            for target, weight in sorted(face_weights(value).items())]


def apply_face(face, targets, name='face'):
    """Add each (meshes, weight) target to the face. Offsets line up vertex for vertex."""
    batches = [batch for mesh in face for batch in mesh.batches]
    for meshes, weight in targets:
        offsets = [batch for mesh in meshes for batch in mesh.batches]
        if [len(b.positions) for b in batches] != [len(b.positions) for b in offsets]:
            raise arc.FormatError('%s: a target does not fit the face' % name)
        for batch, offset in zip(batches, offsets):
            batch.positions = [tuple(p[i] + int(d[i] * weight) for i in range(3))
                               for p, d in zip(batch.positions, offset.positions)]


def check_fit(data, end, record_offsets, name):
    """Prove the array ended where it should: the next arrays, then the first mesh record."""
    if end + AFTER_LATTICES_TO_FIRST_RECORD > len(data):
        raise arc.FormatError('%s: the file ends after its lattices' % name)
    if any(struct.unpack_from('<%dI' % ARRAYS_AFTER_LATTICES, data, end)):
        raise arc.FormatError('%s: data follows the lattices' % name)
    if record_offsets and record_offsets[0] != end + AFTER_LATTICES_TO_FIRST_RECORD:
        raise arc.FormatError('%s: the first mesh is not where the lattices say' % name)


def inside(lattice, metres):
    return all(0.0 <= (metres[axis] - lattice.low[axis]) * lattice.scale[axis] <= 1.0
               for axis in range(3))


def check_neutral(lattice, positions, totals, name):
    """An unmorphed lattice must give back every vertex inside its box, to within rounding."""
    for position in positions:
        metres = [value / FIXED_ONE for value in position]
        if not inside(lattice, metres):
            totals['vertices outside their box'] += 1
            continue
        cells, fractions = parameterize(lattice, metres)
        target = evaluate(lattice, lattice.frames[0], cells, fractions)
        error = max(abs(t - m) * FIXED_ONE for t, m in zip(target, metres))
        totals['vertices inside their box'] += 1
        if error > NEUTRAL_TOLERANCE:
            raise arc.FormatError('%s: %s moves a vertex by %.1f when unmorphed'
                                  % (name, lattice.name, error))


def check_model(data, entry, shaders, totals):
    lattices, end = read_lattices(data, entry.name)
    if not lattices:
        return
    offsets = [record[0] for record in mdl.find_mesh_records(data, shaders)]
    check_fit(data, end, offsets, entry.name)
    totals['models with lattices'] += 1
    totals['lattices'] += len(lattices)
    meshes = mdl.parse_model(data, shaders, entry.name)
    batches = [batch for mesh in meshes for batch in mesh.batches]
    tags = batch_tags(data, entry.name)
    if [len(t) for t in tags] != [len(b.positions) for b in batches]:
        raise arc.FormatError('%s: tags do not line up' % entry.name)
    seen = collections.defaultdict(set)
    for batch, batch_tag in zip(batches, tags):
        for position, tag in zip(batch.positions, batch_tag):
            seen[tag].add(position)
    places = tag_lattices(seen, lattices)
    for tag, positions in seen.items():
        if tag in places:
            check_neutral(lattices[places[tag]], positions, totals, entry.name)
        else:
            totals['vertices tagged for no lattice'] += len(positions)
    code = entry.name.split('_')[1]
    if code not in BODY_PARTS:
        raise arc.FormatError('%s: lattices on an unknown part' % entry.name)
    driven = lattice_weights(code, lattices, (HEAD_MAX, BODY_MAX, BODY_MAX, BODY_MAX))
    totals['lattices no value reaches'] += len(set(places.values()) - set(driven))


def check_faces(sizes, totals):
    """Every target the head's list names must exist and fit the face, vertex for vertex."""
    for prefix in ('af', 'am'):
        face = sizes['%s_ft_base_face' % prefix]
        for region, feature in enumerate(FACE_REGIONS):
            for target in range(len(FACE_TARGETS[feature])):
                name = face_target_name(prefix, region, target)
                if sizes[name] != face:
                    raise arc.FormatError('%s does not fit the face' % name)
                totals['face targets'] += 1


def check(disc):
    _, shaders = mdl.shader_table(disc)
    totals = collections.Counter()
    failures = []
    sizes = {}
    for entry, data in mdl.each_model(disc, shaders):
        totals['models'] += 1
        try:
            check_model(data, entry, shaders, totals)
            if entry.name.split('_')[1:2] in [[code] for code in FACE_CODES]:
                sizes[entry.name] = [len(b.positions) for mesh in
                                     mdl.parse_model(data, shaders, entry.name)
                                     for b in mesh.batches]
        except arc.FormatError as error:
            failures.append(str(error))
    try:
        check_faces(sizes, totals)
    except (arc.FormatError, KeyError) as error:
        failures.append('face target: %s' % error)
    for key, value in sorted(totals.items()):
        print('%-32s %8d' % (key, value))
    print('failures: %d' % len(failures))
    for failure in failures[:20]:
        print('   ', failure)
    return 1 if failures else 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
