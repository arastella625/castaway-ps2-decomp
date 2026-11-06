"""Pad short loops in the compiler's assembly output the way the game's assembler did.

The PS2's processor can leave a loop of a few instructions too early, so assemblers pad
such loops with nops. The game's assembler padded every loop to at least six instructions,
counting the closing branch and its delay slot, with the nops directly before the branch.
That assembler is not available: the SN builds that are pad to seven, or not at all. So
this filter stands in for that one behaviour. It pads the loop itself, and writes the
closing branch as a raw `.word`, which the assembler sees as data and leaves alone.

The length of a loop is measured, not guessed. One line of assembly can become several
instructions, and the assembler adds instructions of its own, so the caller assembles a
probe copy of the file and this module reads each loop's real length out of the result.

Usage, from a build script:

    text = asm_filter.pad_short_loops(text, measure)

where measure(probe_text, markers) assembles probe_text and returns, for each marker symbol,
the instruction words that follow it. tools/verify.py has the implementation.
"""
import re

VERSION = '1'  # part of the build cache key: change it whenever the output can change

LOOP_WORDS = 6                  # the shortest loop in the game's own code
BRANCH_DISTANCE = LOOP_WORDS - 2  # instructions from the top of such a loop to its branch
MARKER = '__short_loop_%d'

LABEL = re.compile(r'^([$.\w]+):')
INSTRUCTION = re.compile(r'^\s+([a-z][\w.]*)\s*([^#]*)')
REGISTER_NAMES = ('zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 '
                  's0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra').split()
REGISTERS = dict({'$%d' % i: i for i in range(32)}, **{'$' + n: i for i, n in enumerate(REGISTER_NAMES)})
REGISTERS['$s8'] = 30

# mnemonic: (opcode bits, number of register operands, how they are placed)
TWO, ONE, NONE = 'rs,rt', 'rs', ''
BRANCHES = {
    'beq': (0x10000000, TWO), 'bne': (0x14000000, TWO),
    'beql': (0x50000000, TWO), 'bnel': (0x54000000, TWO),
    'beqz': (0x10000000, ONE), 'bnez': (0x14000000, ONE),
    'beqzl': (0x50000000, ONE), 'bnezl': (0x54000000, ONE),
    'blez': (0x18000000, ONE), 'bgtz': (0x1C000000, ONE),
    'blezl': (0x58000000, ONE), 'bgtzl': (0x5C000000, ONE),
    'bltz': (0x04000000, ONE), 'bgez': (0x04010000, ONE),
    'bltzl': (0x04020000, ONE), 'bgezl': (0x04030000, ONE),
    'bc1f': (0x45000000, NONE), 'bc1t': (0x45010000, NONE),
    'bc1fl': (0x45020000, NONE), 'bc1tl': (0x45030000, NONE),
    'b': (0x10000000, NONE),
}


class FilterError(Exception):
    """Assembly this filter can't handle safely. The build should stop."""


def branch_bits(mnemonic, operands):
    """The instruction word of a branch without its offset, or None if this isn't a plain one."""
    if mnemonic not in BRANCHES:
        return None
    bits, shape = BRANCHES[mnemonic]
    registers = [REGISTERS.get(o) for o in operands[:-1]]
    if None in registers or len(registers) != (2 if shape == TWO else 1 if shape == ONE else 0):
        return None
    for register, shift in zip(registers, (21, 16)):
        bits |= register << shift
    return bits


def find_candidates(lines):
    """Backward branches close enough to their label that the loop might be short.

    Returns [(label line index, branch line index, bits, label)]. A line of assembly is at
    least one instruction, so a branch more than BRANCH_DISTANCE lines after its label
    can't close a short loop.
    """
    candidates = []
    labels = {}       # label: (line index, instruction lines seen before it)
    seen = 0
    reorder, inline = True, False
    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith('#APP'):
            inline = True
        elif stripped.startswith('#NO_APP'):
            inline = False
        if inline or not stripped or stripped.startswith('#'):
            continue
        label = LABEL.match(line)
        if label:
            labels[label.group(1)] = (index, seen)
            continue
        if stripped.startswith('.'):
            words = stripped.split()
            if words[0] == '.set' and len(words) > 1 and words[1] in ('reorder', 'noreorder'):
                reorder = words[1] == 'reorder'
            elif words[0] in ('.ent', '.end'):
                labels = {}
            continue
        instruction = INSTRUCTION.match(line)
        if not instruction:
            continue
        mnemonic = instruction.group(1)
        operands = [o.strip() for o in instruction.group(2).split(',') if o.strip()]
        if mnemonic[0] == 'b' and operands and operands[-1] in labels:
            head, before = labels[operands[-1]]
            if seen - before <= BRANCH_DISTANCE:
                bits = branch_bits(mnemonic, operands)
                where = 'line %d: %s' % (index + 1, stripped)
                if bits is None:
                    raise FilterError('a short loop ends in a branch this filter cannot encode (%s)' % where)
                if reorder:
                    # The assembler would choose this branch's delay slot itself.
                    raise FilterError('a short loop ends in a branch outside .set noreorder (%s)' % where)
                candidates.append((head, index, bits, operands[-1]))
        seen += 1
    return candidates


def raw_branch(bits, label, comment):
    return '\t.word\t0x%08X | (((%s - . - 4) >> 2) & 0xFFFF)\t# %s' % (bits, label, comment)


def measured_distance(words, bits, taken):
    """How many instructions after the loop's top its branch sits, or None if more than a short loop allows."""
    for distance, word in enumerate(words[:BRANCH_DISTANCE + 1]):
        if word == bits | (-(distance + 1) & 0xFFFF) and distance not in taken:
            taken.add(distance)
            return distance
    return None


def pad_short_loops(text, measure):
    """Return `text` with every short loop padded to LOOP_WORDS instructions."""
    lines = text.splitlines(keepends=True)
    candidates = find_candidates(lines)
    if not candidates:
        return text
    ending = '\r\n' if lines[0].endswith('\r\n') else '\n'

    # The probe: every candidate branch hidden, and a marker symbol at the top of its loop.
    probe = list(lines)
    for number, (head, branch, bits, label) in enumerate(candidates):
        marker = MARKER % number
        probe[head] += '\t.globl\t%s%s%s:%s' % (marker, ending, marker, ending)
        probe[branch] = raw_branch(bits, label, 'probe') + ending
    words = measure(''.join(probe), [MARKER % n for n in range(len(candidates))])

    out = list(lines)
    padding = {}  # branch line index: nops put before it
    taken = {}    # label line index: distances already matched to a candidate
    for number, (head, branch, bits, label) in enumerate(candidates):
        distance = measured_distance(words[MARKER % number], bits, taken.setdefault(head, set()))
        if distance is None:
            continue
        # Padding an inner loop makes the loop around it longer.
        distance += sum(count for line, count in padding.items() if head < line < branch)
        if distance > BRANCH_DISTANCE:
            continue
        nops = BRANCH_DISTANCE - distance
        padding[branch] = nops
        out[branch] = ('\tnop' + ending) * nops + raw_branch(bits, label, lines[branch].strip()) + ending
    return ''.join(out)


def _count_measure(probe_text, markers):
    """A stand-in for the assembler in the self-test: one instruction per line of assembly."""
    words = {}     # marker: the words after it
    loop_of = {}   # marker: the label at the top of its loop
    last_label = None
    for line in probe_text.splitlines():
        label = LABEL.match(line)
        raw = re.match(r'\s+\.word\s+(0x[0-9A-F]+) \| \(\(\((\S+) - ', line)
        if label and label.group(1) in markers:
            words[label.group(1)] = []
            loop_of[label.group(1)] = last_label
        elif label:
            last_label = label.group(1)
        elif raw or INSTRUCTION.match(line):
            for marker, seen in words.items():
                offset = -(len(seen) + 1) & 0xFFFF
                seen.append(int(raw.group(1), 16) | offset if raw and raw.group(2) == loop_of[marker] else 0)
    return words


def self_test():
    """Check the padding rules without an assembler. Exits non-zero on a failure."""
    def loop(body, branch='bne\t$2,$0,$L1', mode='noreorder'):
        return ('f:\n$L1:\n' + ''.join('\t%s\n' % b for b in body)
                + '\t.set\t%s\n\t%s\n\taddu\t$6,$6,8\n\t.set\treorder\n' % (mode, branch))

    def nops_and_raw(text):
        return text.count('\tnop\n'), text.count('.word')

    one = pad_short_loops(loop(['sltu\t$2,$6,$7']), _count_measure)
    assert nops_and_raw(one) == (3, 1), one                   # 1 + 3 nops + branch + delay slot = 6
    assert '0x14400000 | ((($L1 - . - 4) >> 2) & 0xFFFF)' in one, one
    four = pad_short_loops(loop(['lw\t$4,0($19)', 'move\t$5,$17', 'addu\t$19,$19,4', 'addu\t$20,$20,-1'],
                                'bgezl\t$20,$L1'), _count_measure)
    assert nops_and_raw(four) == (0, 1), four                 # already 6: hidden, not padded
    assert '0x06830000' in four, four
    five = loop(['nop2'] * 5)
    assert pad_short_loops(five, _count_measure) == five      # 7 instructions: left alone
    forward = 'f:\n\t.set\tnoreorder\n\tbne\t$2,$0,$L9\n\tnop\n$L9:\n'
    assert pad_short_loops(forward, _count_measure) == forward
    for bad in (loop(['sltu\t$2,$6,$7'], mode='reorder'), loop(['sltu\t$2,$6,$7'], 'bgt\t$2,5,$L1')):
        try:
            pad_short_loops(bad, _count_measure)
        except FilterError:
            continue
        raise AssertionError('accepted:\n' + bad)
    assert branch_bits('bc1t', ['$L1']) == 0x45010000 and branch_bits('b', ['$L1']) == 0x10000000
    assert branch_bits('beq', ['$sp', '$31', '$L1']) == 0x10000000 | 29 << 21 | 31 << 16
    print('asm_filter: self-test passed')


if __name__ == '__main__':
    import sys
    if sys.argv[1:] != ['--self-test']:
        sys.exit('usage: asm_filter.py --self-test\n(the filter itself is called by tools/verify.py)')
    self_test()
