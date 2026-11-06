#!/usr/bin/env python3
"""Extract loads/stores off `this` (r3) from Wii methods in the ts2c asm.
Usage: wii_fields.py <ts2c/asm dir> <ps2_field_access.csv> <out.csv>
Only methods named in the PS2 CSV are scanned. Same columns, minus the PS2 address.
"""
import sys, os, re, csv

asm, ps2_csv, out = sys.argv[1:4]
with open(ps2_csv, encoding='utf8', errors='replace') as fh:
    wanted = {r['wii_symbol'] for r in csv.DictReader(fh)}

WIDTH = {'lbz': 1, 'stb': 1, 'lhz': 2, 'lha': 2, 'sth': 2, 'lwz': 4, 'stw': 4, 'lfs': 4, 'stfs': 4, 'lfd': 8, 'stfd': 8}
label = re.compile(r'^"?([^\s":]+)"?:\s*$')
mem = re.compile(r'\t(\w+)\s+[rf](\d+), (-?0x[0-9a-fA-F]+|-?\d+)\(r(\d+)\)')
copy = re.compile(r'\tmr\s+r(\d+), r(\d+)')
dest = re.compile(r'\t\w+\.?\s+r(\d+),')
call = re.compile(r'\tbl\s+"?([^"\s]+)')

rows = []
for root, _, files in os.walk(asm):
    for fn in sorted(files):
        if not fn.endswith('.s'):
            continue
        cur = None
        self_regs = set()
        with open(os.path.join(root, fn), encoding='latin-1') as fh:
            for line in fh:
                m = label.match(line)
                if m and not m.group(1).startswith('lbl_'):
                    cur = m.group(1) if m.group(1) in wanted else None
                    self_regs = {3}
                    continue
                if not cur or not line.startswith('/*'):
                    continue
                m = mem.search(line)
                if m and m.group(1) in WIDTH and int(m.group(4)) in self_regs:
                    kind = 'L' if m.group(1).startswith('l') else 'S'
                    rows.append((cur, '%x' % int(m.group(3), 0), WIDTH[m.group(1)], kind))
                m = copy.search(line)
                if m and int(m.group(2)) in self_regs:
                    self_regs.add(int(m.group(1)))
                    continue
                m = call.search(line)
                if m:
                    if not m.group(1).startswith(('_savegpr', '_restgpr')):
                        self_regs = {r for r in self_regs if r > 12}
                    continue
                m = dest.search(line)
                if m and not re.search(r'\t(st|cmp)', line):
                    self_regs.discard(int(m.group(1)))

with open(out, 'w', newline='') as fh:
    w = csv.writer(fh)
    w.writerow(['wii_symbol', 'offset', 'width', 'kind'])
    w.writerows(rows)
print('wii field accesses:', len(rows), 'methods:', len({r[0] for r in rows}))
