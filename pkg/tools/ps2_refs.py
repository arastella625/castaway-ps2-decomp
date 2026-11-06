#!/usr/bin/env python3
"""PS2 EE ELF: find functions and the strings each references.
Usage: ps2_refs.py <SLUS elf> <out.json> [ghidra_functions.csv]
With the CSV, Ghidra's function boundaries replace the jal/jr-ra heuristic.
"""
import sys, json, re, csv, bisect
from elftools.elf.elffile import ELFFile
import capstone
from capstone import mips as M

ELF, OUT = sys.argv[1], sys.argv[2]
FUNCS = sys.argv[3] if len(sys.argv) > 3 else None
f = open(ELF, 'rb'); elf = ELFFile(f)
secs = {s.name: s for s in elf.iter_sections()}
text = secs['.text']; tbase = text['sh_addr']; tdata = text.data()

# strings in data-ish sections
str_at = {}
for name in ('.rodata', '.data'):
    s = secs[name]; base = s['sh_addr']; d = s.data()
    for m in re.finditer(rb'[\x09\x0a\x20-\x7e]{4,}\x00', d):
        str_at[base + m.start()] = m.group()[:-1].decode('latin-1')
print('ps2 strings:', len(str_at))

md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS64 + capstone.CS_MODE_LITTLE_ENDIAN)
md.detail = True
md.skipdata = True

# decode every word (EE-only opcodes come back as data; that's fine)
insns = {}
for ins in md.disasm(tdata, tbase):
    insns[ins.address] = ins
addrs = sorted(insns)
print('instructions:', len(addrs))

def imm(ins):
    for op in ins.operands:
        if op.type == M.MIPS_OP_IMM: return op.imm
    return None

# function starts: jal targets + after "jr ra; <delay>" (skipping nops)
starts = {tbase}
for a in addrs:
    ins = insns[a]
    if ins.id and ins.mnemonic == 'jal':
        t = imm(ins)
        if t is not None and tbase <= t < tbase + len(tdata): starts.add(t)
for i, a in enumerate(addrs):
    ins = insns[a]
    if ins.id and ins.mnemonic == 'jr' and ins.op_str.strip() in ('$ra', 'ra'):
        j = i + 2
        while j < len(addrs) and insns[addrs[j]].id and insns[addrs[j]].mnemonic == 'nop': j += 1
        if j < len(addrs): starts.add(addrs[j])
starts = sorted(starts)
print('function starts:', len(starts))

MIN_GAP = 16

def gap_function_starts(gaps, heuristic_starts):
    """Pick extra function starts inside code Ghidra left outside any function.
    gaps: sorted (start, end) pairs. heuristic_starts: sorted jal/jr-ra starts.
    Returns addresses; each becomes a function running to the next pick or the gap's end.
    """
    picks = []
    for g0, g1 in gaps:
        if g1 - g0 < MIN_GAP: continue  # alignment padding, not code worth scanning
        lo, hi = bisect.bisect_left(heuristic_starts, g0), bisect.bisect_left(heuristic_starts, g1)
        picks += sorted({g0, *heuristic_starts[lo:hi]})
    return picks

# Optional Ghidra boundaries (ExportGhidraFunctions.java): start -> (end, body size)
spans = None
if FUNCS:
    with open(FUNCS, newline='') as fh:
        spans = {int(r['start'], 16): (int(r['end'], 16), int(r['size'])) for r in csv.DictReader(fh)}
    order = sorted(spans)
    gaps = [(spans[a][0], b) for a, b in zip(order, order[1:] + [tbase + len(tdata)]) if spans[a][0] < b]
    picks = sorted(gap_function_starts(gaps, starts))
    for g0, g1 in gaps:
        inside = [p for p in picks if g0 <= p < g1]
        for p, e in zip(inside, inside[1:] + [g1]):
            spans[p] = (e, e - p)
    starts = sorted(spans)
    print('ghidra functions: %d, gaps: %d (%d bytes), gap functions added: %d'
          % (len(order), len(gaps), sum(b - a for a, b in gaps), len(picks)))

def reg(op):
    return op.reg if op.type == M.MIPS_OP_REG else None

funcs = {}
for k, st in enumerate(starts):
    end = starts[k + 1] if k + 1 < len(starts) else tbase + len(tdata)
    size = end - st
    if spans: end, size = spans[st]
    hi = {}; refs = []; calls = []
    a = st
    while a < end:
        ins = insns.get(a); a += 4
        if ins is None or ins.id == 0: continue
        mn = ins.mnemonic; ops = ins.operands
        try:
            if mn == 'lui':
                hi[reg(ops[0])] = (imm(ins) & 0xffff) << 16
            elif mn in ('addiu', 'daddiu', 'ori') and len(ops) == 3 and reg(ops[1]) in hi:
                lo = ops[2].imm
                if mn == 'ori': v = hi[reg(ops[1])] | (lo & 0xffff)
                else: v = hi[reg(ops[1])] + (lo - 0x10000 if 0x8000 <= lo <= 0xffff else lo)
                v &= 0xffffffff
                if v in str_at and str_at[v] not in refs: refs.append(str_at[v])
            elif mn == 'jal':
                calls.append(imm(ins))
        except Exception:
            pass
    funcs['%08X' % st] = {'size': size, 'strings': refs, 'calls': ['%08X' % c for c in calls if c is not None]}

json.dump(funcs, open(OUT, 'w'))
print('with string refs:', sum(1 for v in funcs.values() if v['strings']))
