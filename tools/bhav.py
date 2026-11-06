#!/usr/bin/env python3
"""Check the BHAV instruction format against every tree on the disc.

Usage: python -I tools/bhav.py <game.iso>

docs/formats/world.md says what each field of a tree means; those statements come from the
decompiled interpreter (src/game/simulator/core2/TreeSim.cpp). This reads all of EDITHTRE and
tests them on the real data: branch targets, tree ID ranges, expression operators and scopes,
and the header bytes. Each rule also runs at a wrong offset, as a negative control: a rule
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


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    disc = arc.Disc(argv[1])
    trees = []
    for entry, data in gamedata.read_entries(disc, 'EDITHTRE'):
        trees.extend(read_trees(data, entry.name))

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

    rules_hold = bad_targets == 0 and bad_expressions == 0 and None not in classes
    controls_fail = bad_control > 0 and bad_expr_control > 0
    print('rules hold' if rules_hold else 'a rule failed', '/',
          'controls fail as they should' if controls_fail else 'a control passed: the test proves nothing')
    return 0 if rules_hold and controls_fail else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
