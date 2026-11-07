#!/usr/bin/env python3
"""Read each lot's tile arrays from DEFAULT.NGH on your own disc image, and where a Sim can walk.

Usage:
  python -I tools/lotgrid.py check <game.iso>
  python -I tools/lotgrid.py grid  <game.iso> <out.json> <lot_01>
  python -I tools/lotgrid.py routing <game.iso> <out.json>

check decodes every Arry chunk of every house to its exact last byte and tests what the flags
mean against the lot grids and the placed objects, with negative controls. From Python:

  lots = lotgrid.read_arrays(arc.Disc(iso))   # {'lot_01': {8: Array2D, 11: ..., 12: ...}, ...}
  grid = lotgrid.walkable(lots['lot_01'])     # grid[y][x] is True where a Sim may stand
  walls = lotgrid.wall_edges(lots['lot_11'])  # ((x, y), (x, y)) pairs a Sim can't cross

x and y are placed.py's metres, floored: x is the arrays' second dimension. Output is game
data: keep it out of the repo. docs/formats/world.md has the format.
"""
import collections
import math
import json
import os
import struct
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import gamedata as gd  # noqa: E402
import placed  # noqa: E402

FLAGS, FLOOR, WALLS = 8, 11, 12  # kFlagLayrerRsrcID, kExtFloorRsrcID, kExtWallRsrcID
ARRAY_KIND = 'Arry'
HEADER = struct.Struct('<I4i')
MAX_SIDE = 64  # cFixedWorld::GetMaxSize
ITEM_SIZES = {FLAGS: 1, FLOOR: 1, WALLS: 8}
RUN_BIT = 0x8000
COUNT_MASK = 0x7FFF

BUILDABLE = 0x01  # grid 0
INSIDE_BORDER = 0x02
OUTSIDE = 0x04  # cFixedWorld::IsOutside
ROOFED = 0x08  # set at run time by UpdateRoomStyles
NOT_ROUTABLE = 0x10  # cleared by grid 1
NOT_EDITABLE = 0x20  # cleared by grid 1
NO_PLACING = 0x80  # grid 2
OFF_GRID = NOT_ROUTABLE | NOT_EDITABLE
GRID_BITS = {0: BUILDABLE, 1: NOT_ROUTABLE, 2: NO_PLACING}

WALL_SEGMENT_STEPS = {1: (0, -1), 2: (-1, 0), 4: (0, 1), 8: (1, 0)}  # (x, y) to the far tile
DIAGONAL_SEGMENTS = 0x30


@dataclass
class Array2D:
    rows: int  # the first number in the header: the game's X size, placed.py's y
    columns: int  # the second: the game's Y size, placed.py's x
    item_size: int
    data: bytes
    pads: bytes  # the bytes skipped to keep packets on even offsets
    loaded: bytes = b''  # for the flags: as cFixedWorld::Load leaves them

    def item(self, x, y):
        pos = (y * self.columns + x) * self.item_size
        return self.data[pos:pos + self.item_size]


def decode_array(data):
    """_c2DArray::DoStream in read mode: a run-length stream of bytes, packets on even offsets."""
    if len(data) < HEADER.size:
        raise arc.FormatError('Arry: too short')
    zero, rows, columns, item_size, packed = HEADER.unpack_from(data, 0)
    if zero != 0 or packed != 1:
        raise arc.FormatError('Arry: not a packed stream')
    if not (0 < rows <= MAX_SIDE and 0 < columns <= MAX_SIDE and item_size in (1, 8)):
        raise arc.FormatError('Arry: bad size')
    total = rows * columns * item_size
    pos = HEADER.size
    out = bytearray()
    pads = bytearray()
    while len(out) < total:
        if pos + 2 > len(data):
            raise arc.FormatError('Arry: a stream runs off its chunk')
        word = struct.unpack_from('<H', data, pos)[0]
        pos += 2
        count = word & COUNT_MASK
        size = 1 if word & RUN_BIT else count
        if count == 0 or len(out) + count > total or pos + size > len(data):
            raise arc.FormatError('Arry: a packet runs past its array')
        out += data[pos:pos + 1] * count if word & RUN_BIT else data[pos:pos + count]
        pos += size
        if pos & 1:
            pads += data[pos:pos + 1]
            pos += 1
    if pos != len(data):
        raise arc.FormatError('Arry: %d bytes after the stream' % (len(data) - pos))
    return Array2D(rows, columns, item_size, bytes(out), bytes(pads))


def load_flags(flags, level):
    """The grid half of cFixedWorld::Load: grids replace three flags on every tile."""
    buildable, routable, no_placing = level.grids
    has_routing = (routable.x + routable.width) & 0xFF and (routable.y + routable.height) & 0xFF
    out = bytearray(flags.data)
    for y in range(flags.rows):
        for x in range(flags.columns):
            value = out[y * flags.columns + x]
            value &= ~(BUILDABLE | OFF_GRID | NO_PLACING)
            if has_routing and not routable.get(x, y):
                value |= OFF_GRID
            value |= BUILDABLE if buildable.get(x, y) else 0
            value |= NO_PLACING if no_placing.get(x, y) else 0
            out[y * flags.columns + x] = value
    return bytes(out)


def read_levels(disc):
    archive = arc.find_archive(disc, 'LEVELS')
    return {e.name: gd.read_level(disc.read(archive.lba, e.offset, e.size), e.name)
            for e in arc.read_toc(disc, archive)}


def house_arrays(disc):
    """Return one {array id: chunk bytes} for each house, in file order."""
    entry = disc.find(gd.NGH_FILE)
    sections = gd.read_ngh_sections(disc.read(entry.lba, 0, entry.size))
    return [{number: data for kind, number, data in gd.house_chunks(s) if kind == ARRAY_KIND}
            for s in sections if s.kind == gd.NGH_KIND_HOUSE]


def lot_name(index, offset=placed.FIRST_LOT_OFFSET):
    return 'lot_%02d' % (index + offset)


def read_arrays(disc):
    """Return {lot name: {array id: Array2D}}. The flags array also has its loaded form."""
    levels = read_levels(disc)
    lots = {}
    for index, chunks in enumerate(house_arrays(disc)):
        arrays = {number: decode_array(data) for number, data in chunks.items()}
        for number, array in arrays.items():
            if array.item_size != ITEM_SIZES.get(number):
                raise arc.FormatError('Arry %d: item size %d' % (number, array.item_size))
        name = lot_name(index)
        arrays[FLAGS].loaded = load_flags(arrays[FLAGS], levels[name])
        lots[name] = arrays
    return lots


def walkable(lot):
    """Rows of booleans, grid[y][x]: tiles with flag 0x10 clear, before objects and scripts."""
    flags = lot[FLAGS]
    return [[not flags.loaded[y * flags.columns + x] & NOT_ROUTABLE
             for x in range(flags.columns)] for y in range(flags.rows)]


def wall_edges(lot):
    """Return the sorted ((x, y), (x, y)) tile pairs with a wall between them."""
    walls = lot[WALLS]
    edges = set()
    for y in range(walls.rows):
        for x in range(walls.columns):
            segments = walls.item(x, y)[0]
            if segments & DIAGONAL_SEGMENTS:
                raise arc.FormatError('Arry: a diagonal wall, which no lot had when this was made')
            for segment, (dx, dy) in WALL_SEGMENT_STEPS.items():
                if segments & segment:
                    edges.add(tuple(sorted(((x, y), (x + dx, y + dy)))))
    return sorted(edges)


def grid_agreement(flags, level, bit, grid, swapped=False):
    """The share of tiles whose on-disc flag says what the grid says."""
    matches = 0
    for y in range(flags.rows):
        for x in range(flags.columns):
            is_set = bool(flags.item(x, y)[0] & bit)
            cell = level.grids[grid].get(y, x) if swapped else level.grids[grid].get(x, y)
            matches += is_set != cell if bit == NOT_ROUTABLE else is_set == cell
    return matches / (flags.rows * flags.columns)


def mean_agreement(flag_arrays, levels, offset, swapped=False):
    """Mean agreement per grid when house N is paired with lot N + offset."""
    pairs = [(f, levels[lot_name(i, offset)]) for i, f in enumerate(flag_arrays)
             if lot_name(i, offset) in levels]
    return [sum(grid_agreement(f, level, bit, grid, swapped) for f, level in pairs) / len(pairs)
            for grid, bit in GRID_BITS.items()]


def objects_on_walkable(houses, lots, offset, swapped=False):
    """The share of on-lot objects on a walkable tile, house N paired with lot N + offset."""
    hits = total = 0
    for index, objects in enumerate(houses):
        lot = lots.get(lot_name(index, offset))
        if lot is None:
            continue
        grid = walkable(lot)
        for item in (o for o in objects if o.on_lot):
            x, y = (math.floor(c) for c in item.metres)
            x, y = (y, x) if swapped else (x, y)
            total += 1
            hits += 0 <= y < len(grid) and 0 <= x < len(grid[0]) and grid[y][x]
    return hits / total


def check_decoding(lots):
    passed = True
    sizes = sum(len({(a.rows, a.columns) for a in arrays.values()}) == 1
                for arrays in lots.values())
    passed &= gd.report('lots with three same-sized arrays', sizes, len(lots))
    pads = [a.pads for arrays in lots.values() for a in arrays.values()]
    passed &= gd.report('pad bytes that are zero', sum(p.count(0) for p in pads),
                        sum(map(len, pads)))
    return passed


def is_rejected(data):
    try:
        decode_array(data)
    except arc.FormatError:
        return True
    return False


def check_controls(chunks):
    """Wrong streams must fail on every chunk, not just the one tried first."""
    def resized(data):
        rows = HEADER.unpack_from(data, 0)[1] - 1
        return data[:4] + struct.pack('<i', rows) + data[8:]

    passed = True
    for label, change in (
            ('moved by one byte', lambda d: d[:HEADER.size] + d[HEADER.size + 1:]),
            ('moved by two bytes', lambda d: d[:HEADER.size] + d[HEADER.size + 2:]),
            ('cut short', lambda d: d[:-2]),
            ('one row too few', resized)):
        passed &= gd.report('control: ' + label, sum(is_rejected(change(d)) for d in chunks),
                            len(chunks), 'rejected')
    return passed


def check_flags(lots):
    counts = collections.Counter()
    for arrays in lots.values():
        flags = arrays[FLAGS]
        for y in range(flags.rows):
            for x in range(flags.columns):
                value = flags.item(x, y)[0]
                border = x in (0, flags.columns - 1) or y in (0, flags.rows - 1)
                counts['tiles'] += 1
                counts['0x10 with 0x20'] += value & OFF_GRID in (0, OFF_GRID)
                counts['outside'] += bool(value & OUTSIDE)
                counts['0x02 off the border'] += bool(value & INSIDE_BORDER) != border
                counts['no roof'] += not value & ROOFED
    return all([gd.report(key, counts[key], counts['tiles'])
                for key in ('0x10 with 0x20', 'outside', '0x02 off the border', 'no roof')])


def check_walls(lots):
    """Each tile stores its side of an edge, so bit 4 must meet bit 1 on the next tile."""
    pairs = total = 0
    for arrays in lots.values():
        walls = arrays[WALLS]
        for y in range(walls.rows):
            for x in range(walls.columns):
                if walls.item(x, y)[0] & 4:
                    total += 1
                    pairs += bool(y + 1 < walls.rows and walls.item(x, y + 1)[0] & 1)
    print('wall edges in all lots: %d' % sum(len(wall_edges(a)) for a in lots.values()))
    return gd.report('walls seen from both sides', pairs, total)


def check_meaning(disc, lots):
    levels = read_levels(disc)
    flag_arrays = [lots[name][FLAGS] for name in sorted(lots)]
    print('on-disc flags against grids 0, 1, 2, house N with lot N + offset:')
    scores = {o: mean_agreement(flag_arrays, levels, o) for o in (1, 0, 2, 3, 7)}
    for offset, score in scores.items():
        print('  offset %+d: %.3f %.3f %.3f' % (offset, *score))
    print('  swapped:   %.3f %.3f %.3f' % tuple(mean_agreement(flag_arrays, levels, 1, True)))
    passed = all(scores[1][g] > max(s[g] for o, s in scores.items() if o != 1) for g in GRID_BITS)

    houses = list(placed.read_houses(disc).values())
    print('objects on walkable tiles, by offset:')
    fits = {o: objects_on_walkable(houses, lots, o) for o in (1, 0, 2, 3, 7)}
    for offset, fit in fits.items():
        print('  offset %+d: %.3f' % (offset, fit))
    print('  swapped:   %.3f' % objects_on_walkable(houses, lots, 1, True))
    tiles = [cell for lot in lots.values() for row in walkable(lot) for cell in row]
    print('  tiles walkable: %.3f' % (sum(tiles) / len(tiles)))
    passed &= fits[1] > max(fit for offset, fit in fits.items() if offset != 1) + 0.2

    last = len(flag_arrays) - 1
    print('last house, grid 1 agreement: lot_%02d %.3f, lot_%02d %.3f' % (
        last + 1, grid_agreement(flag_arrays[last], levels[lot_name(last)], NOT_ROUTABLE, 1),
        last, grid_agreement(flag_arrays[last], levels[lot_name(last - 1)], NOT_ROUTABLE, 1)))
    return passed


def check(disc):
    chunks = [data for house in house_arrays(disc) for data in house.values()]
    decoded = sum(not is_rejected(data) for data in chunks)
    passed = gd.report('Arry chunks ending on their last byte', decoded, len(chunks))
    lots = read_arrays(disc)
    ids = collections.Counter(number for arrays in lots.values() for number in arrays)
    print('array ids: %s' % sorted(ids.items()))
    passed &= check_decoding(lots)
    passed &= check_controls(chunks)
    passed &= check_flags(lots)
    passed &= check_walls(lots)
    passed &= check_meaning(disc, lots)
    passed &= check_routing(disc, lots)
    print('failures: %d' % (not passed))
    return 0 if passed else 1


def routable_rows(entry, bit=NOT_ROUTABLE):
    rows = [bytes.fromhex(row) for row in entry['flags']]
    return [[not tile & bit for tile in row] for row in rows]


def check_routing(disc, lots):
    """The export must say what walkable does, and a single wall edge can't make a room."""
    data = routing_data(disc)
    same = sum(routable_rows(data[name]) == walkable(lots[name]) for name in lots)
    passed = gd.report('routing tiles match walkable', same, len(lots))
    wrong = sum(routable_rows(data[name], BUILDABLE) == walkable(lots[name]) for name in lots)
    print('  control: bit 0x01 instead matches on %d lots' % wrong)
    passed &= wrong == 0
    passed &= gd.report('lots with at most one wall edge',
                        sum(len(entry['wall_edges']) <= 1 for entry in data.values()), len(data))
    return passed


def write_grid(disc, out_path, lot):
    """Write the walkable tiles as rows of 0 and 1, row y first. This is game data."""
    rows = [''.join('1' if tile else '0' for tile in row)
            for row in walkable(read_arrays(disc)[lot])]
    with open(out_path, 'w', encoding='utf8') as out:
        json.dump({'lot': lot, 'rows': rows}, out)
    print('wrote %s: %d rows' % (out_path, len(rows)))
    return 0


def hex_rows(array, data):
    return [data[y * array.columns:(y + 1) * array.columns].hex() for y in range(array.rows)]


def walls_of(array):
    """Return [x, y, the tile's 8 TileWallStorage bytes as hex] for each tile that has any."""
    return [[x, y, array.item(x, y).hex()] for y in range(array.rows) for x in range(array.columns)
            if any(array.item(x, y))]


def routing_data(disc):
    """What XRoute reads, by lot, apart from objects and slots."""
    data = {}
    for name, lot in sorted(read_arrays(disc).items()):
        flags = lot[FLAGS]
        data[name] = {'columns': flags.columns, 'rows': flags.rows,
                      'flags': hex_rows(flags, flags.loaded),
                      'floor': hex_rows(lot[FLOOR], lot[FLOOR].data),
                      'walls': walls_of(lot[WALLS]), 'wall_edges': wall_edges(lot)}
    return data


def write_routing(disc, out_path):
    """Write every lot's routing data. This is game data."""
    with open(out_path, 'w', encoding='ascii') as out:
        json.dump(routing_data(disc), out, separators=(',', ':'))
    print('wrote %s' % out_path)
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 5 and argv[1] == 'grid':
        return write_grid(arc.Disc(argv[2]), argv[3], argv[4])
    if len(argv) == 4 and argv[1] == 'routing':
        return write_routing(arc.Disc(argv[2]), argv[3])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
