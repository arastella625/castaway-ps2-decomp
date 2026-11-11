#!/usr/bin/env python3
"""What an object's interaction needs: the clips its trees can play, read without running them.

Usage:
  python -I tools/interactions.py clips <game.iso> <ResFile row> <menu entry name>
  python -I tools/interactions.py check <game.iso>

clips walks the entry's action tree and every tree it calls, as Behavior::GetResFileID finds them:
0x100 up global, 0x1000 up the object's private set, 0x2000 up its semi-global set (ResFile +0x34).
Each Animate (opcode 0x2C) names a clip by operand +4: kind 0 is the controlling object's list 0x81
(its own row, then its semi-global row), kind 3 the global row's list 0x9C. An Animate whose number
is in parameter 0 (+5 bit 2) takes the calling node's first operand; one computed at run time is
listed as unresolved. docs/formats/bhav.md has the evidence for all of this.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import bhav  # noqa: E402
import gamedata as gd  # noqa: E402

RESFILE_ANIM_LISTS = 4  # 8-byte entries: a vector of AnimRefName rows, then an s16 set ID
RESFILE_TREE_TABLES = 0x24
RESFILE_SEMI = 0x34
TREE_TABLE_SIZE = 12
TREE_TABLE_ENTRY_SIZE = 0x24
TREE_TABLE_NAME = 0x1C
ADULT_LIST = 0x81  # 0x81 + body code 0
GLOBAL_ADULT_LIST = 0x9C
GLOBAL_ROW = 'global'
ANIMATE = 0x2C
NUMBER_IN_PARAMETER = 4  # Animate operand +5 bit 2
BLANK_NODE = 0x7FFF
PRIVATE_FROM, SEMI_FROM, SEMI_TO = 0x1000, 0x2000, 0x2328


class Rows:
    """ResFile rows by name: their semi-global row, clip lists and menu entries."""

    def __init__(self, disc):
        quickdata = gd.load_quickdata(disc)
        self.data, self.table = gd.find_table(quickdata, 'ResFile')
        _, self.anim_table = gd.find_table(quickdata, 'AnimRef')
        self.at = {self.data.row_name(self.table, i): self.table.rows + i * self.table.stride
                   for i in range(self.table.count)}
        self.name_at = {pos: name for name, pos in self.at.items()}

    def semi(self, row):
        pointer = self.data.word(self.at[row] + RESFILE_SEMI)
        return self.name_at.get(pointer, '')

    def clip_list(self, row, set_id):
        data = self.data
        start, count = data.vector(self.at[row] + RESFILE_ANIM_LISTS)
        for entry in range(start, start + count * 8, 8):
            if data.word(entry + 4) & 0xFFFF != set_id:
                continue
            names_at, names = data.vector(entry)
            out = []
            for index in range(names):
                pointer = data.word(names_at + index * 4)
                if not pointer:
                    out.append('')
                    continue
                ref = data.word(pointer)
                slot, rest = divmod(ref - self.anim_table.rows, self.anim_table.stride)
                out.append(data.row_name(self.anim_table, slot) if not rest else '')
            return out
        return []

    def action_tree(self, row, entry_name):
        data = self.data
        start, count = data.vector(self.at[row] + RESFILE_TREE_TABLES)
        for record in range(start, start + count * TREE_TABLE_SIZE, TREE_TABLE_SIZE):
            entries_at, entries = data.vector(record)
            for entry in range(entries_at, entries_at + entries * TREE_TABLE_ENTRY_SIZE,
                               TREE_TABLE_ENTRY_SIZE):
                name_at = data.word(entry + TREE_TABLE_NAME)
                if name_at and data.wide_string(data.word(name_at)) == entry_name:
                    return struct.unpack_from('<h', data.image, entry + 2)[0] & 0xFFFF
        return None


def tree_sets(disc):
    archive = arc.find_archive(disc, 'EDITHTRE')
    return {entry.name: {tid: (name, nodes) for tid, name, _h, nodes
                         in bhav.read_trees(disc.read(archive.lba, entry.offset, entry.size), entry.name)}
            for entry in arc.read_toc(disc, archive)}


def walk(rows, sets, row, start, semi=None):
    """Return ({clip: [where]}, [unresolved Animate], [calls with no tree], trees walked)."""
    semi = rows.semi(row) if semi is None else semi
    own = rows.clip_list(row, ADULT_LIST)
    shared = rows.clip_list(semi, ADULT_LIST) if semi else []
    global_list = rows.clip_list(GLOBAL_ROW, GLOBAL_ADULT_LIST)

    def clip(kind, number):
        if kind == 0:
            for names in (own, shared):  # an empty entry falls through to the next list
                if 0 <= number < len(names) and names[number]:
                    return names[number]
        elif kind == 3 and 0 <= number < len(global_list):
            return global_list[number] or None
        return None

    def find(opcode):
        if opcode < PRIVATE_FROM:
            return sets[GLOBAL_ROW].get(opcode)
        if opcode < SEMI_FROM:
            return sets.get(row, {}).get(opcode)
        return sets.get(semi, {}).get(opcode) if semi else None

    clips, unresolved, missing, seen = {}, [], [], set()
    stack = [(start, (None,) * 4, 'menu')]
    while stack:
        opcode, params, via = stack.pop()
        found = find(opcode)
        if found is None:
            missing.append('%#x from %s' % (opcode, via))
            continue
        if (opcode, params) in seen:
            continue
        seen.add((opcode, params))
        name, nodes = found
        for index, node in enumerate(nodes):
            op = struct.unpack_from('<H', node, 0)[0]
            if op == ANIMATE:
                number, kind = struct.unpack_from('<h', node, 4)[0], struct.unpack_from('<b', node, 8)[0]
                if node[9] & NUMBER_IN_PARAMETER:
                    number = params[0]
                where = '%s node %d' % (name, index)
                if number is None or number < 0:
                    unresolved.append(where)
                elif number:
                    named = clip(kind, number)
                    if named:
                        clips.setdefault(named, []).append(where)
                    else:
                        unresolved.append('%s (kind %d, number %d)' % (where, kind, number))
            elif 0x100 <= op <= SEMI_TO and op != BLANK_NODE:
                stack.append((op, struct.unpack_from('<4h', node, 4), name))
    return clips, unresolved, missing, len({o for o, _ in seen})


def show(disc, row, entry_name):
    rows, sets = Rows(disc), tree_sets(disc)
    start = rows.action_tree(row, entry_name)
    if start is None:
        print('%s has no menu entry %r' % (row, entry_name))
        return 1
    clips, unresolved, missing, trees = walk(rows, sets, row, start)
    print('%s %r: action tree %#x, semi-global row %s, %d trees' % (
        row, entry_name, start, rows.semi(row) or '-', trees))
    for clip in sorted(clips):
        print('  %-40s %s' % (clip, clips[clip][0]))
    for where in unresolved:
        print('  unresolved: %s' % where)
    for call in missing:
        print('  no tree: %s' % call)
    return 0


def check(disc):
    """Eat on a coconut must give the clips bhav.md reads from PS2; Harvest the climb clips."""
    rows, sets = Rows(disc), tree_sets(disc)
    passed = True
    cases = [('resource_fruit_coconut', 'Eat', {'a2o-peelfruit', 'a2o-eatfruit-small'}),
             ('source_tree_coconutpalm', 'Harvest Coconuts', {'a2o-tree-climbup', 'a2o-tree-climbdown'})]
    for row, entry_name, wanted in cases:
        start = rows.action_tree(row, entry_name)
        clips, unresolved, missing, trees = walk(rows, sets, row, start) if start else ({}, [], ['none'], 0)
        ok = start is not None and wanted <= set(clips) and not missing
        print('%-24s %-18s %3d trees, %2d clips, %d unresolved, %d calls with no tree: %s' % (
            row, entry_name, trees, len(clips), len(unresolved), len(missing), 'ok' if ok else 'FAILED'))
        passed &= ok
    print('controls:')
    for row, entry_name, _ in cases:
        start = rows.action_tree(row, entry_name)
        wrong = next(name for name in sorted(rows.at) if name != rows.semi(row) and name in sets
                     and name != row and name.endswith('_sg'))
        _, _, missing, _ = walk(rows, sets, row, start, semi=wrong)
        print('  %s with %s as its semi-global set: %d calls find no tree' % (row, wrong, len(missing)))
        passed &= bool(missing)
    print('failures: %d' % (not passed))
    return 0 if passed else 1


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 5 and argv[1] == 'clips':
        return show(arc.Disc(argv[2]), argv[3], argv[4])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        sys.exit(1)
