#!/usr/bin/env python3
"""Read the neighbourhood, its Neighbors and families from DEFAULT.NGH on your own disc image.

Usage:
  python -I tools/neighborhood.py check  <game.iso>
  python -I tools/neighborhood.py export <game.iso> <out.json>

check reads kind 1's NGBH, NBRS and FAMI chunks to their exact last byte and tests the result
against ObjDefinition, with controls. Output is game data: keep it out of the repo.
docs/formats/world.md has the format.
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import gamedata as gd  # noqa: E402
import placed  # noqa: E402

NEIGHBORS_SECTION = 1
CHUNK_BODY = 12  # the size word, the version and the tag
STREAM_VERSION = 0x82
NEIGHBORHOOD_VARS = 0x24
GOAL_UNLOCK_SIZE = 0x80
WEATHER_EVENTS = 100
WEATHER_FLOATS = 4
NEIGHBOR_DATA = 0x31
NEIGHBOR_FLOATS = 16
RELATIONS_NEW_FORM = -1
PLAYER_FAMILY_WORDS = 0x19
PLAYER_FAMILY_FLAG = 0x4000  # in a FAMI chunk's ID
WIDE_MARK = b'\xfe\xff'  # 0xFFFE, little-endian: UTF-16 text in the disc's byte order


def wide_string(reader):
    """ReconString for BString2: a mark, then UTF-16 to a zero, or just a zero."""
    start = reader.pos
    end = start
    while reader.data[end:end + 2] != b'\0\0':
        end += 2
        if end >= len(reader.data):
            raise arc.FormatError('a string runs off its chunk')
    text = reader.data[start:end]
    if text and not text.startswith(WIDE_MARK):
        raise arc.FormatError('a string at %#x has no mark' % start)
    reader.jump(end + 2)
    return text[2:].decode('utf-16-le')


def narrow_string(reader):
    """ReconString for StringBuffer: padded to an even length, with 0xCD."""
    end = reader.data.index(0, reader.pos)
    text = reader.data[reader.pos:end].decode('latin-1')
    length = end + 1 - reader.pos
    reader.jump(end + 1 + length % 2)
    return text


def integers(reader, width, count):
    return [reader.integer(width) for _ in range(count)]


def floats(reader, count):
    values = list(struct.unpack_from('<%df' % count, reader.data, reader.pos))
    reader.jump(reader.pos + 4 * count)
    return values


def open_chunk(data, tag):
    if data[8:12] != tag[::-1].encode('ascii'):
        raise arc.FormatError('a chunk is not %s' % tag)
    if struct.unpack_from('<I', data, 4)[0] != STREAM_VERSION:
        raise arc.FormatError('%s: unknown stream version' % tag)
    return placed.BitReader(data, CHUNK_BODY)


def finish(reader, tag):
    if reader.pos != len(reader.data):
        raise arc.FormatError('%s: stream ends at %d of %d' % (tag, reader.pos, len(reader.data)))


def read_neighborhood(data):
    """Neighborhood::DoStream at version 0x82."""
    reader = open_chunk(data, 'NGBH')
    result = {'vars': integers(reader, 16, NEIGHBORHOOD_VARS), 'name': wide_string(reader)}
    result['unknown'] = integers(reader, 32, 2)
    result['goal_unlocks'] = integers(reader, 8, GOAL_UNLOCK_SIZE)
    result['plans'] = integers(reader, 16, reader.integer(16))  # PlanManager
    result['lots'] = [[reader.integer(16), reader.integer(32)]  # LotManager
                      for _ in range(reader.integer(16))]
    result['lot_current'] = reader.integer(16)
    result['lot_flags'] = integers(reader, 8, reader.integer(16))
    result['weather_events'] = [[reader.integer(32), reader.integer(16), reader.integer(8),
                                 reader.integer(8)] for _ in range(WEATHER_EVENTS)]
    result['weather_state'] = integers(reader, 32, 2) + floats(reader, WEATHER_FLOATS)
    result['lot_resources'] = [integers(reader, 32, 2)  # LotHelper
                               for _ in range(reader.integer(16))]
    finish(reader, 'NGBH')
    return result


def read_relations(reader):
    """RelMatrixImpl::DoStream, the same stream a placed object has."""
    if reader.integer(32) != RELATIONS_NEW_FORM:
        raise arc.FormatError('NBRS: old relationship form')
    relations = []
    for _ in range(reader.integer(32)):
        if reader.integer(32):
            other = reader.integer(32)
            relations.append([other, integers(reader, 32, reader.integer(32))])
    return relations


def read_neighbor(reader, data_words=NEIGHBOR_DATA):
    """Neighbor::DoStream at version 0x82."""
    result = {'name': narrow_string(reader), 'unknown': reader.integer(32)}
    has_data = reader.integer(32)
    result['data'] = integers(reader, 16, data_words) if has_data else None
    result['data_flag'] = has_data
    result['floats'] = [floats(reader, NEIGHBOR_FLOATS), floats(reader, NEIGHBOR_FLOATS)]
    result['id'] = reader.integer(16)
    result['guid'] = reader.integer(32)
    result['relations'] = read_relations(reader)
    # Only chimps stream a name. The next word in any other case is a 0 or 1 flag.
    result['chimp_name'] = wide_string(reader) if reader.data.startswith(WIDE_MARK, reader.pos) else None
    return result


def read_neighbors(data, data_words=NEIGHBOR_DATA):
    """DoPtrVectorStream<Neighbor>: a count, then a present flag before each."""
    reader = open_chunk(data, 'NBRS')
    neighbors = []
    for _ in range(reader.integer(32)):
        if reader.integer(32):
            neighbors.append(read_neighbor(reader, data_words))
    finish(reader, 'NBRS')
    return neighbors


def read_family(data, chunk_id):
    """FamilyImpl::DoStream, and PlayerFamilyImpl's words after it."""
    reader = open_chunk(data, 'FAMI')
    keys = ('house', 'creation_order', 'funds', 'pet_points', 'house_value', 'friends', 'flags')
    result = {key: reader.integer(32) for key in keys}
    result['id'] = chunk_id
    result['members'] = integers(reader, 32, reader.integer(32))  # FamilyMember: a Neighbor's GUID
    result['name'] = wide_string(reader)
    items = reader.integer(16)
    if items:
        raise arc.FormatError('FAMI: inventory items are not decoded')
    result['inventory_id'] = reader.integer(8)
    result['token_words'] = integers(reader, 32, reader.integer(32))
    if chunk_id & PLAYER_FAMILY_FLAG:
        result['player_words'] = integers(reader, 16, PLAYER_FAMILY_WORDS)
    finish(reader, 'FAMI')
    return result


def read_chunks(disc):
    entry = disc.find(gd.NGH_FILE)
    sections = gd.read_ngh_sections(disc.read(entry.lba, 0, entry.size))
    return gd.house_chunks(next(s for s in sections if s.kind == NEIGHBORS_SECTION))


def read_all(chunks):
    by_kind = {}
    for kind, chunk_id, data in chunks:
        by_kind.setdefault(kind, []).append((chunk_id, data))
    return {'neighborhood': read_neighborhood(by_kind['NGBH'][0][1]),
            'neighbors': read_neighbors(by_kind['NBRS'][0][1]),
            'families': [read_family(data, chunk_id) for chunk_id, data in by_kind['FAMI']]}


def definition_guids(disc):
    data, table = gd.find_table(gd.load_quickdata(disc), 'SimsObjects/ObjDefinition')
    return {struct.unpack_from('<i', data.image, table.rows + index * table.stride + gd.OBJD_GUID)[0]
            for index in range(table.count)}


def check(disc):
    chunks = read_chunks(disc)
    world = read_all(chunks)
    neighbors, families = world['neighbors'], world['families']
    known = definition_guids(disc)
    print('%s: %d neighbors, %d families, %d plans, %d lots'
          % (world['neighborhood']['name'], len(neighbors), len(families),
             len(world['neighborhood']['plans']), len(world['neighborhood']['lots'])))
    passed = gd.report('neighbors with a definition', sum(n['guid'] in known for n in neighbors),
                       len(neighbors))
    ids = [n['id'] for n in neighbors]
    passed &= gd.report('neighbor IDs differ', len(set(ids)), len(ids))
    members = [m for f in families for m in f['members']]
    guids = {n['guid'] for n in neighbors}
    passed &= gd.report('family members are neighbors', sum(m in guids for m in members), len(members))
    print('neighbors with data words: %d, chimps: %d, inventory items: 0, token words set: %d'
          % (sum(n['data'] is not None for n in neighbors),
             sum(n['chimp_name'] is not None for n in neighbors),
             sum(any(f['token_words']) for f in families)))
    print('controls:')
    nbrs = next(data for kind, _, data in chunks if kind == 'NBRS')
    ngbh = next(data for kind, _, data in chunks if kind == 'NGBH')
    fami = next((data, chunk_id) for kind, chunk_id, data in chunks if kind == 'FAMI')
    passed &= gd.expect_failure('neighbors with one data word too few',
                                lambda: read_neighbors(nbrs, NEIGHBOR_DATA - 1))
    passed &= gd.expect_failure('neighbors a byte out of place',
                                lambda: read_neighbors(nbrs[:CHUNK_BODY] + nbrs[CHUNK_BODY + 1:]))
    passed &= gd.expect_failure('a neighbourhood cut short', lambda: read_neighborhood(ngbh[:-8]))
    passed &= gd.expect_failure('a family with a byte added',
                                lambda: read_family(fami[0] + b'\0', fami[1]))
    print('failures: %d' % (not passed))
    return 0 if passed else 1


def export(disc, out_path):
    world = read_all(read_chunks(disc))
    with open(out_path, 'w', encoding='ascii') as out:
        json.dump(world, out, separators=(',', ':'))
    print('wrote %s: %d neighbors, %d families'
          % (out_path, len(world['neighbors']), len(world['families'])))
    return 0


def main(argv):
    if len(argv) >= 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) >= 4 and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3])
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
