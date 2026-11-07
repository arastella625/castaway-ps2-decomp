#!/usr/bin/env python3
"""Read objects' routing slots from QUICKDAT on your own disc image.

Usage:
  python -I tools/slots.py check  <game.iso>
  python -I tools/slots.py export <game.iso> <out.json>

export writes {"definitions": {signed GUID: [slot, ...]}, "global": [slot, ...]}, keyed like
objects.json's definitions. A slot's place in its list is its number in GotoRoutingSlot. Output
is game data: keep it out of the repo. docs/formats/world.md has the format.
"""
import collections
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import bhav  # noqa: E402
import gamedata as gd  # noqa: E402

RESFILE_SETS = 0x28  # a vector of 8-byte entries: pointer to a VECTOR of slots, then the set's ID
RESFILE_SEMI_GLOBAL = 0x34
SET_ENTRY_SIZE = 8
DEF_GUID = 0x1C
DEF_SLOT_SET = 0x28
DEF_SLOT_SET_CONTROL = 0x2A
SEMI_GLOBAL_SETS_FROM = 1000  # lower IDs are in the object's own row
GLOBAL_ROW = 'global'
GLOBAL_SET = 100
SLOT_SIZE = 0x48
SLOT_SIZE_CONTROL = 0x44
SLOT_TYPE_MASK = 0xFF
ROUTING = 3  # 0 is an object slot and 1 a sprite slot
SLOT_FORMAT = '<4x3f12x3iIi3ifii'  # RoutingSlot(const SlotDescriptor *) copies from +4 on
GOTO_ROUTING_SLOT = 0x2D
SLOT_IS_LITERAL = 1  # the operand at +2: +0 is the slot's number


def read_slot(data, pos):
    (x, y, z, standing, sitting, ground, flags, snap_target, minimum, maximum, optimal, gradient,
     facing, resolution) = struct.unpack_from(SLOT_FORMAT, data.image, pos)
    return {'offset': [x, y, z], 'multipliers': [standing, sitting, ground], 'flags': flags,
            'snap_target': snap_target, 'min': minimum, 'max': maximum, 'optimal': optimal,
            'gradient': gradient, 'facing': facing, 'resolution': resolution}


def read_sets(data, row, slot_size=SLOT_SIZE):
    """Return {set ID: [routing slots]} for a ResFile row."""
    start, count = data.vector(row + RESFILE_SETS)
    sets = {}
    for index in range(count):
        entry = start + index * SET_ENTRY_SIZE
        first, slots = data.vector(entry)
        positions = [first + item * slot_size for item in range(slots)]
        sets[data.word(entry + 4)] = [read_slot(data, pos) for pos in positions
                                      if data.word(pos) & SLOT_TYPE_MASK == ROUTING]
    return sets


def read_tables(disc):
    data, files = gd.find_table(gd.load_quickdata(disc), 'SimsObjects/ResFile')
    rows = [files.rows + index * files.stride for index in range(files.count)]
    return data, files, rows


def definition_slots(data, row, definition, slot_key=DEF_SLOT_SET):
    """cXObject::Initialize: the set's ID is in the definition, and high IDs are semi-global."""
    wanted = struct.unpack_from('<h', data.image, definition + slot_key)[0]
    source = row
    if wanted >= SEMI_GLOBAL_SETS_FROM:
        source = data.word(row + RESFILE_SEMI_GLOBAL)
    return wanted, read_sets(data, source).get(wanted) if source else None


def read_all(disc, slot_key=DEF_SLOT_SET):
    """Return ({GUID: routing slots}, {ResFile name: slots of its first definition}, counts)."""
    data, files, rows = read_tables(disc)
    slots, by_name = {}, {}
    totals = collections.Counter()
    for index, row in enumerate(rows):
        name = data.row_name(files, index)
        start, count = data.vector(row)
        for item in range(count):
            definition = data.word(start + 4 * item)
            wanted, found = definition_slots(data, row, definition, slot_key)
            totals['definitions'] += 1
            totals['without a set'] += wanted <= 0
            totals['set not found'] += wanted > 0 and found is None
            slots[data.word(definition + DEF_GUID)] = found or []
            by_name.setdefault(name, found or [])
    return slots, by_name, totals


def read_global(disc):
    """ObjectModule::LoadGlobalRoutingSlots: the global namespace's set 100."""
    data, files, rows = read_tables(disc)
    row = next(row for index, row in enumerate(rows) if data.row_name(files, index) == GLOBAL_ROW)
    return read_sets(data, row)[GLOBAL_SET]


def literal_uses(disc):
    """Yield (tree set, slot number) for each GotoRoutingSlot that names its slot outright."""
    archive = arc.find_archive(disc, 'EDITHTRE')
    for entry in arc.read_toc(disc, archive):
        data = disc.read(archive.lba, entry.offset, entry.size)
        for _, _, _, nodes in bhav.read_trees(data, entry.name):
            for node in nodes:
                opcode = struct.unpack_from('<H', node, 0)[0]
                slot, kind = struct.unpack_from('<2h', node, 4)
                if opcode == GOTO_ROUTING_SLOT and kind == SLOT_IS_LITERAL:
                    yield entry.name, slot


def fit(uses, by_name):
    known = [(name, slot) for name, slot in uses if name in by_name]
    return sum(0 <= slot < len(by_name[name]) for name, slot in known), len(known)


def check(disc):
    slots, by_name, totals = read_all(disc)
    for key, value in sorted(totals.items()):
        print('%-16s %5d' % (key, value))
    print('definitions with routing slots %d, slots %d'
          % (sum(bool(s) for s in slots.values()), sum(map(len, slots.values()))))
    print('global routing slots', len(read_global(disc)))
    uses = list(literal_uses(disc))
    fits, known = fit(uses, by_name)
    print('literal GotoRoutingSlot nodes in range of their own object: %d of %d' % (fits, known))
    names = sorted(by_name)
    moved = dict(zip(names, [by_name[name] for name in names[1:] + names[:1]]))
    wrong_fits, _ = fit(uses, moved)
    print('control, each tree set given the slots of the next object: %d of %d' % (wrong_fits, known))
    _, _, wrong = read_all(disc, DEF_SLOT_SET_CONTROL)
    print('control, set ID read at +0x%X: %d sets not found' % (DEF_SLOT_SET_CONTROL, wrong['set not found']))
    passed = fits == known and wrong_fits < known and wrong['set not found'] > totals['set not found']
    print('failures: %d' % (not passed))
    return 0 if passed else 1


def export(disc, out_path):
    slots, _, _ = read_all(disc)
    signed = {struct.unpack('<i', struct.pack('<I', guid))[0]: found
              for guid, found in slots.items() if found}
    with open(out_path, 'w', encoding='ascii') as out:
        json.dump({'definitions': signed, 'global': read_global(disc)}, out,
                  separators=(',', ':'))
    print('wrote %s: %d definitions' % (out_path, len(signed)))
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
