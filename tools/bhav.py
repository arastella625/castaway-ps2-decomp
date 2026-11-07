#!/usr/bin/env python3
"""Check the BHAV instruction format against every tree on the disc.

Usage: python -I tools/bhav.py <game.iso>

docs/formats/world.md says what each field of a tree means; those statements come from the
decompiled interpreter (src/game/simulator/core2/TreeSim.cpp). This reads all of EDITHTRE and
tests them on the real data: branch targets, tree ID ranges, expression operators and scopes,
the header bytes, the link to an object's semi-global set, tree constants and the attribute counts. Each rule also runs at a wrong offset, as a negative control: a rule
that passes there too proves nothing. It prints counts only. Exit status is 0 when every rule
holds and every control fails.
"""
import collections
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import gamedata  # noqa: E402

NODE_SIZE = 12
HEADER_SIZE = 12
ERROR_TARGET, TRUE_TARGET, FALSE_TARGET = 0xFD, 0xFE, 0xFF
RETURN_TARGETS = (ERROR_TARGET, TRUE_TARGET, FALSE_TARGET)
EXPRESSION = 2
MAX_OPERATOR = 0x12
MAX_SCOPE = 0x44
LITERAL_SCOPE = 7
FIRST_GLOBAL, FIRST_PRIVATE, FIRST_SEMI, LAST_SEMI = 0x100, 0x1000, 0x2000, 0x2328
BLANK_NODE = 0x7FFF  # after masking bit 15; a placeholder with all-FF operands
GLOBAL_SET = 'global'
CONSTANT_SCOPE = 0x1A
CONSTANT_BASES = (0x1000, 0x2000, 0x100)  # private, semi-global, global
CONSTANT_INDEX_IN_TEMP = 0x7F
RESFILE_CONSTANTS = 0x20  # VECTOR<BehaviorConstants>
RESFILE_SEMI = 0x34  # pointer to the ResFile row of the object's semi-global set
CONSTANTS_SIZE = 16
CONSTANT_VALUE_SIZE = 8
RESFILE_STRINGS = 0x10  # 12-byte entries: pointer to a VECTOR of strings, tag, ID in the low 16 bits
STRING_SET_SIZE = 12
ATTRIBUTE_SCOPE, SEMI_ATTRIBUTE_SCOPE = 0, 2
DEF_ATTRIBUTES, DEF_ATTRIBUTES_CONTROL = 0x74, 0x72
DEF_GUID = 0x1C
DEF_SLOT_SET = 0x28  # the STR#-like ID of the object's slot set, 0 for none
RESFILE_SLOTS = 0x28  # 8-byte entries: pointer to a VECTOR of slots, set ID
SLOT_SET_SIZE = 8
SLOT_SIZE = 0x48
SEMI_SLOT_SETS = 1000  # set IDs from here live in the semi-global file
DEF_LEAD = 0x10   # the control for +0x16: it marks the interaction leader, not the master
DEF_GROUP = 0x14  # the multi-tile object this is part of, 0 for a single tile
DEF_TILE = 0x16   # the part's place, y << 8 | x, and -1 on the master
DEFAULT_ATTRIBUTES = 8  # what a count of 0 means
SEMI_ATTRIBUTES, SEMI_ATTRIBUTES_CONTROL = 0x100, 0x101  # STR# IDs
SEMI_SET_SUFFIX = '_sg'


def read_trees(data, label):
    """EDITHTRE set -> [(chunk id, name, header bytes, [12-byte nodes])]."""
    reader = gamedata.Reader(data, label)
    gamedata.read_data_header(reader, gamedata.EDITH_VERSION, gamedata.EDITH_TAG)
    reader.take(gamedata.IFF_HEADER_SIZE)
    trees = []
    while not reader.at_end():
        reader.take(4)
        size, chunk_id, _flags = reader.unpack('>IHH')
        name = reader.fixed_string(gamedata.IFF_CHUNK_HEADER - 12)
        body = reader.take(size - gamedata.IFF_CHUNK_HEADER)
        count = struct.unpack_from('<H', body, 2)[0]
        nodes = [body[HEADER_SIZE + i * NODE_SIZE:HEADER_SIZE + (i + 1) * NODE_SIZE] for i in range(count)]
        trees.append((chunk_id, name, body[:HEADER_SIZE], nodes))
    return trees


def target_ok(value, count):
    return value < count or value in RETURN_TARGETS


def tree_class(opcode):
    """Behavior::GetTreeClass: 0 primitive, 1 global, 2 private, 3 semi-global, None out of range."""
    if opcode == BLANK_NODE:
        return 'blank'
    if opcode < FIRST_GLOBAL:
        return 0
    if opcode < FIRST_PRIVATE:
        return 1
    if opcode < FIRST_SEMI:
        return 2
    if opcode <= LAST_SEMI:
        return 3
    return None


def expression_ok(node, base=4):
    return (node[base + 5] <= MAX_OPERATOR and node[base + 6] <= MAX_SCOPE and node[base + 7] <= MAX_SCOPE)


def read_resfiles(disc):
    """The ResFile quickdata table -> {name: (semi-global row name or None, constant tables)}.

    Constant tables are {table id: (title, tuning, [(value, label)])}. A value is an s16 in the
    first half of an 8-byte entry, and the other half of that word is padding.
    """
    data, table = gamedata.find_table(gamedata.load_quickdata(disc), 'ResFile')
    rows = {table.rows + i * table.stride: data.row_name(table, i) for i in range(table.count)}
    resfiles = {}
    for pos, name in rows.items():
        semi = rows[data.word(pos + RESFILE_SEMI)] if data.word(pos + RESFILE_SEMI) else None
        start, count = data.vector(pos + RESFILE_CONSTANTS)
        tables = {}
        for entry in range(start, start + count * CONSTANTS_SIZE, CONSTANTS_SIZE):
            values_at, values = data.vector(entry + 4)
            tables[data.word(entry) & 0xFFFF] = (
                data.pointer_string(entry + 8), data.word(entry + 12) & 0xFF,
                [(struct.unpack_from('<h', data.image, v)[0], data.pointer_string(v + 4))
                 for v in range(values_at, values_at + values * CONSTANT_VALUE_SIZE, CONSTANT_VALUE_SIZE)])
        resfiles[name] = (semi, tables)
    return resfiles


def split_constant(data):
    """XObjLang::GetConstantsID: scope 0x1A's data -> (kind, table id, index), or None."""
    kind = data >> 13 & 7
    if kind > 2:
        return None
    return kind, CONSTANT_BASES[kind] + (data >> 7 & 0x3F), data & 0x7F


def contexts(name, resfiles, owners):
    """The (private row, semi-global row) pairs a tree in this set can run under."""
    if name == GLOBAL_SET:
        return [(row, resfiles[row][0]) for row in resfiles]
    if name in owners:
        return [(row, name) for row in owners[name]]
    return [(name, resfiles[name][0] or name)]  # a set nothing links to can only see itself


def constant_found(split, context, resfiles):
    kind, table, index = split
    row = (context[0], context[1], GLOBAL_SET)[kind]
    found = resfiles[row][1].get(table) if row else None
    return found is not None and (index == CONSTANT_INDEX_IN_TEMP or index < len(found[2]))


def check_links(disc, sets):
    """Test ResFile +0x34 on every semi-global call, and constants on every scope 0x1A operand."""
    resfiles = read_resfiles(disc)
    owners = collections.defaultdict(list)
    for name, (semi, _) in resfiles.items():
        if semi:
            owners[semi].append(name)
    tree_ids = {name: {tree[0] for tree in trees} for name, trees in sets.items()}
    names = sorted(tree_ids)

    calls = bad_calls = control_calls = 0
    results = collections.Counter()
    controls = control_found = 0
    missing = []
    for name, trees in sets.items():
        runs = contexts(name, resfiles, owners)
        for tree in trees:
            for node in tree[3]:
                opcode = struct.unpack_from('<H', node, 0)[0] & 0x7FFF
                if FIRST_SEMI <= opcode <= LAST_SEMI:
                    calls += 1
                    semis = {run[1] for run in runs if run[1]}
                    bad_calls += not any(opcode in tree_ids[semi] for semi in semis)
                    wrong = {names[(names.index(semi) + 1) % len(names)] for semi in semis}
                    control_calls += any(opcode in tree_ids[semi] for semi in wrong)
                if opcode != EXPRESSION:
                    continue
                for offset, scope in ((4, node[10]), (6, node[11])):
                    if scope != CONSTANT_SCOPE:
                        continue
                    data = struct.unpack_from('<H', node, offset)[0]
                    split = split_constant(data)
                    hits = sum(constant_found(split, run, resfiles) for run in runs) if split else 0
                    results['all' if hits == len(runs) else 'some' if hits else 'none'] += 1
                    if not hits:
                        missing.append((name, tree[1], '%#x' % data))
                    if split and split[0] < 2:  # control: private and semi-global the wrong way round
                        wrong = (1 - split[0], CONSTANT_BASES[1 - split[0]] + (data >> 7 & 0x3F), split[2])
                        controls += 1
                        control_found += any(constant_found(wrong, run, resfiles) for run in runs)
    tables = sum(len(row[1]) for row in resfiles.values())
    values = sum(len(table[2]) for row in resfiles.values() for table in row[1].values())
    print('ResFile rows: %d, with a semi-global link: %d' % (len(resfiles), sum(1 for r in resfiles.values() if r[0])))
    print('semi-global calls: %d, not in the linked set: %d (control, next set along: %d found)'
          % (calls, bad_calls, control_calls))
    print('constant tables: %d, values: %d' % (tables, values))
    print('scope 0x1A operands found in every context: %d, some: %d, none: %d' % (results['all'], results['some'], results['none']))
    print('  control, private and semi-global swapped: %d of %d found' % (control_found, controls))
    print('  sets with operands found nowhere: %s' % ' '.join(sorted({item[0] for item in missing})))
    # bhav.md explains the few that aren't found: objects cut from this game, whose trees stayed.
    operands = sum(results.values())
    return bad_calls == 0 and results['none'] * 50 < operands, \
        control_calls * 2 < calls and control_found * 4 < controls * 3


def operand_overruns(sets, limit_of, scope):
    """Expression operands of a scope in each set's trees: (within its limit, past it)."""
    within = past = 0
    for name, trees in sets.items():
        limit = limit_of(name)
        if limit is None:
            continue
        for tree in trees:
            for node in tree[3]:
                if struct.unpack_from('<H', node, 0)[0] != EXPRESSION:
                    continue
                for offset, operand_scope in ((4, node[10]), (6, node[11])):
                    if operand_scope == scope:
                        if struct.unpack_from('<h', node, offset)[0] < limit:
                            within += 1
                        else:
                            past += 1
    return within, past


def check_counts(disc, sets):
    """Test ObjDefinition +0x74 on scope 0 operands, and STR# 0x100 on scope 2 operands."""
    quickdata = gamedata.load_quickdata(disc)
    data, table = gamedata.find_table(quickdata, 'ResFile')
    _, definitions = gamedata.find_table(quickdata, 'ObjDefinition')
    rows = {data.row_name(table, i): table.rows + i * table.stride for i in range(table.count)}
    semis = {name: semi for name, (semi, _) in read_resfiles(disc).items()}
    first, last = definitions.rows, definitions.rows + definitions.count * definitions.stride

    def attribute_limit(offset):
        def limit_of(name):
            if name not in rows:
                return None
            start, count = data.vector(rows[name])
            pointers = (data.word(start + 4 * i) for i in range(count))
            values = [struct.unpack_from('<h', data.image, p + offset)[0] for p in pointers if first <= p < last]
            return max(v or DEFAULT_ATTRIBUTES for v in values) if values else None
        return limit_of

    def semi_limit(string_set):
        def limit_of(name):
            semi = name if name.endswith(SEMI_SET_SUFFIX) else semis.get(name)
            if semi not in rows:
                return None
            start, count = data.vector(rows[semi] + RESFILE_STRINGS)
            for entry in range(start, start + count * STRING_SET_SIZE, STRING_SET_SIZE):
                if data.word(entry + 8) & 0xFFFF == string_set and entry in data.pointers:
                    return data.vector(entry)[1]
            return None
        return limit_of

    attributes = operand_overruns(sets, attribute_limit(DEF_ATTRIBUTES), ATTRIBUTE_SCOPE)
    attributes_control = operand_overruns(sets, attribute_limit(DEF_ATTRIBUTES_CONTROL), ATTRIBUTE_SCOPE)
    semi = operand_overruns(sets, semi_limit(SEMI_ATTRIBUTES), SEMI_ATTRIBUTE_SCOPE)
    semi_control = operand_overruns(sets, semi_limit(SEMI_ATTRIBUTES_CONTROL), SEMI_ATTRIBUTE_SCOPE)
    print('scope 0 operands within ObjDefinition +0x74: %d, past it: %d (control +0x72: %d past)'
          % (attributes[0], attributes[1], attributes_control[1]))
    print('scope 2 operands within STR# 0x100: %d, past it: %d (control STR# 0x101: %d past)'
          % (semi[0], semi[1], semi_control[1]))
    return attributes[1] == 0 and semi[1] == 0, attributes_control[1] > 0 and semi_control[1] > 0


def read_masters(disc):
    """{definition's GUID: its master's GUID}, the way ObjectFolder::GetMasterSelector finds it.

    Definitions of one file with the same non-zero +0x14 are one multi-tile object, and the master
    is the one whose +0x16 is -1. A single-tile definition, or one with no master, is its own.
    Also returns the groups by their number of masters, and the same count for the control word.
    """
    quickdata = gamedata.load_quickdata(disc)
    data, table = gamedata.find_table(quickdata, 'ResFile')
    half = lambda position: struct.unpack_from('<h', data.image, position)[0]
    masters, counts, control = {}, {}, {}
    for row in range(table.count):
        start, count = data.vector(table.rows + row * table.stride)
        groups = {}
        for position in (data.word(start + 4 * i) for i in range(count)):
            guid = data.word(position + DEF_GUID) & 0xFFFFFFFF
            masters[guid] = guid
            if half(position + DEF_GROUP):
                groups.setdefault(half(position + DEF_GROUP), []).append(position)
        for parts in groups.values():
            found = [p for p in parts if half(p + DEF_TILE) == -1]
            counts[len(found)] = counts.get(len(found), 0) + 1
            wrong = sum(half(p + DEF_LEAD) == -1 for p in parts)
            control[wrong] = control.get(wrong, 0) + 1
            if len(found) == 1:
                for part in parts:
                    masters[data.word(part + DEF_GUID) & 0xFFFFFFFF] = data.word(found[0] + DEF_GUID) & 0xFFFFFFFF
    return masters, counts, control


def check_masters(disc):
    """Every multi-tile object should have exactly one master, and the wrong word shouldn't."""
    masters, counts, control = read_masters(disc)
    print('multi-tile objects by masters: %s (control +0x10: %s), %d definitions point elsewhere'
          % (sorted(counts.items()), sorted(control.items()), sum(k != v for k, v in masters.items())))
    return set(counts) == {1}, set(control) != {1}


def read_slots(disc):
    """{ResFile row name: {slot set ID: [slot dictionaries]}}, as QuickDataSlotDescList reads them."""
    quickdata = gamedata.load_quickdata(disc)
    data, table = gamedata.find_table(quickdata, 'ResFile')
    half = lambda position: struct.unpack_from('<h', data.image, position)[0]
    word = lambda position: struct.unpack_from('<i', data.image, position)[0]
    real = lambda position: struct.unpack_from('<f', data.image, position)[0]
    files = {}
    for row in range(table.count):
        start, count = data.vector(table.rows + row * table.stride + RESFILE_SLOTS)
        sets = files.setdefault(data.row_name(table, row), {})
        for entry in range(start, start + count * SLOT_SET_SIZE, SLOT_SET_SIZE):
            first, slots = data.vector(entry)
            sets[word(entry + 4)] = [{
                'type': half(p), 'offset': (real(p + 4), real(p + 8), real(p + 12)),
                'multipliers': (word(p + 0x1C), word(p + 0x20), word(p + 0x24)),
                'flags': word(p + 0x28), 'snap_target': word(p + 0x2C),
                'min': word(p + 0x30), 'max': word(p + 0x34), 'optimal': word(p + 0x38),
                'gradient': real(p + 0x3C), 'facing': word(p + 0x40), 'resolution': word(p + 0x44),
            } for p in range(first, first + slots * SLOT_SIZE, SLOT_SIZE)]
    return files


def check_slots(disc):
    """Definitions' +0x28 should name slot sets, in their own file or, from 1000, the semi-global one.

    59 definitions name a set that isn't there, mostly test objects: SlotLoader then loads nothing.
    The control, +0x2A, finds none at all.
    """
    files = read_slots(disc)
    semis = {name: semi for name, (semi, _) in read_resfiles(disc).items()}
    quickdata = gamedata.load_quickdata(disc)
    data, table = gamedata.find_table(quickdata, 'ResFile')

    def found(offset):
        hits = misses = 0
        for row in range(table.count):
            name = data.row_name(table, row)
            start, count = data.vector(table.rows + row * table.stride)
            for position in (data.word(start + 4 * i) for i in range(count)):
                wanted = struct.unpack_from('<h', data.image, position + offset)[0]
                if wanted:
                    owner = semis.get(name) if wanted >= SEMI_SLOT_SETS else name
                    if wanted in files.get(owner, {}):
                        hits += 1
                    else:
                        misses += 1
        return hits, misses

    types = {}
    for sets in files.values():
        for slots in sets.values():
            for slot in slots:
                types[slot['type']] = types.get(slot['type'], 0) + 1
    hits, misses = found(DEF_SLOT_SET)
    control = found(DEF_SLOT_SET + 2)
    print('slot types: %s; definitions with a set found: %d, not found: %d (control +0x2A: %d, %d)'
          % (sorted(types.items()), hits, misses, control[0], control[1]))
    return hits > misses * 10 and set(types) <= {0, 1, 3}, control[0] == 0


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    disc = arc.Disc(argv[1])
    trees = []
    sets = {}
    for entry, data in gamedata.read_entries(disc, 'EDITHTRE'):
        sets[entry.name] = read_trees(data, entry.name)
        trees.extend(sets[entry.name])

    nodes = [(tree, node) for tree in trees for node in tree[3]]
    bad_targets = sum(not (target_ok(n[2], len(t[3])) and target_ok(n[3], len(t[3]))) for t, n in nodes)
    # Control: the same test one byte late, where the bytes mean something else.
    bad_control = sum(not (target_ok(n[3], len(t[3])) and target_ok(n[4], len(t[3]))) for t, n in nodes)

    opcodes = collections.Counter(struct.unpack_from('<H', n, 0)[0] for _, n in nodes)
    high_bit = sum(v for k, v in opcodes.items() if k & 0x8000)
    classes = collections.Counter(tree_class(k & 0x7FFF) for k in opcodes.elements())
    primitives = sorted(k for k in opcodes if k < FIRST_GLOBAL)

    expressions = [n for _, n in nodes if struct.unpack_from('<H', n, 0)[0] == EXPRESSION]
    bad_expressions = sum(not expression_ok(n) for n in expressions)
    bad_expr_control = sum(not expression_ok(n, 3) for n in expressions)
    operators = collections.Counter(n[4 + 5] for n in expressions)
    scopes = collections.Counter(n[4 + 6] for n in expressions) + collections.Counter(n[4 + 7] for n in expressions)
    flags = collections.Counter(n[4 + 4] for n in expressions)

    calls = [(t, n) for t, n in nodes if struct.unpack_from('<H', n, 0)[0] & 0x7FFF >= FIRST_GLOBAL]
    default_params = sum(n[4:12] == b'\xff' * 8 for _, n in calls)

    print('%d trees, %d instructions, %d expressions, %d calls' % (len(trees), len(nodes), len(expressions), len(calls)))
    print('branch targets out of range: %d (control one byte late: %d)' % (bad_targets, bad_control))
    print('opcodes with bit 15 set: %d' % high_bit)
    print('tree classes:', sorted(classes.items(), key=str))
    print('primitive opcodes used:', ' '.join('%02X' % k for k in primitives))
    print('expressions out of range: %d (control one byte early: %d)' % (bad_expressions, bad_expr_control))
    print('operators:', ' '.join('%02X:%d' % kv for kv in sorted(operators.items())))
    print('scopes:', ' '.join('%02X:%d' % kv for kv in sorted(scopes.items())))
    print('expression byte +4:', ' '.join('%02X:%d' % kv for kv in sorted(flags.items())))
    print('calls with default parameters: %d of %d' % (default_params, len(calls)))
    for name, offset in (('type', 4), ('params', 5), ('locals', 6), ('byte 7', 7), ('byte 8', 8)):
        values = collections.Counter(t[2][offset] for t in trees)
        print('header %s:' % name, ' '.join('%02X:%d' % kv for kv in sorted(values.items())[:24]))
    print('primitive use:', ' '.join('%02X:%d' % (k, opcodes[k]) for k in primitives))

    links_hold, link_control_fails = check_links(disc, sets)
    counts_hold, count_control_fails = check_counts(disc, sets)
    masters_hold, master_control_fails = check_masters(disc)
    slots_hold, slot_control_fails = check_slots(disc)

    rules_hold = bad_targets == 0 and bad_expressions == 0 and None not in classes and links_hold and counts_hold and masters_hold and slots_hold
    controls_fail = bad_control > 0 and bad_expr_control > 0 and link_control_fails and count_control_fails and master_control_fails and slot_control_fails
    print('rules hold' if rules_hold else 'a rule failed', '/',
          'controls fail as they should' if controls_fail else 'a control passed: the test proves nothing')
    return 0 if rules_hold and controls_fail else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
