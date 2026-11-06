#!/usr/bin/env python3
"""Build a Wii (ts2c) map: function name -> strings it references.
Usage: wii_refs.py <ts2c/asm dir> <out.json>
"""
import os, re, sys, json, subprocess
from collections import defaultdict

ASM = sys.argv[1]
OUT = sys.argv[2]

files = []
for root, _, fs in os.walk(ASM):
    for f in fs:
        if f.endswith('.s'):
            files.append(os.path.join(root, f))

sec_re = re.compile(r'^\.section\s+([\w.]+)')
lab_re = re.compile(r'^("?)([^\s"]+)\1:\s*$')
rom_re = re.compile(r'#\s*ROM:\s*0x([0-9A-Fa-f]+)')
asciz_re = re.compile(r'^\s+\.asciz\s+"(.*)"\s*$')

def unescape(s):
    try:
        return bytes(s, 'latin-1').decode('unicode_escape')
    except Exception:
        return s

def dsize(line):
    m = re.match(r'^\s+\.(4byte|2byte|byte|skip|asciz|float|double)\s*(.*)$', line)
    if not m:
        return 0
    d, arg = m.groups()
    if d == '4byte': return 4 * (arg.count(',') + 1)
    if d == '2byte': return 2 * (arg.count(',') + 1)
    if d == 'byte': return arg.count(',') + 1
    if d == 'float': return 4
    if d == 'double': return 8
    if d == 'skip': return int(arg.split(',')[0], 0)
    if d == 'asciz':
        return len(unescape(arg.strip()[1:-1]).encode('latin-1', 'replace')) + 1
    return 0

# pass 1: labels in data sections with ROM offsets; derive per-section delta from lbl_ names
DATA = {'.data', '.rodata', '.sdata', '.sdata2'}
label_rom = {}      # name -> (section, rom)
strings_raw = []    # (section, base_label, offset, text)
deltas = defaultdict(set)
for fn in files:
    sec = None; cur = None; off = 0
    with open(fn, encoding='latin-1') as fh:
        lines = fh.readlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        m = sec_re.match(line)
        if m:
            sec = m.group(1); cur = None; i += 1; continue
        if sec in DATA:
            m = lab_re.match(line)
            if m:
                name = m.group(2)
                rom = None
                if i + 1 < len(lines):
                    r = rom_re.search(lines[i + 1])
                    if r: rom = int(r.group(1), 16)
                if rom is not None:
                    label_rom[name] = (sec, rom)
                    if name.startswith('lbl_'):
                        deltas[sec].add(int(name[4:], 16) - rom)
                cur = name; off = 0; i += 1; continue
            m = asciz_re.match(line)
            if m and cur:
                strings_raw.append((sec, cur, off, unescape(m.group(1))))
            off += dsize(line)
        i += 1

delta = {}
for s, ds in deltas.items():
    if len(ds) != 1:
        print('warning: section', s, 'deltas', [hex(d) for d in ds])
    delta[s] = max(ds, key=lambda d: d)  # usually exactly one
label_addr = {n: rom + delta[s] for n, (s, rom) in label_rom.items() if s in delta}
str_at = {}
for s, lab, off, txt in strings_raw:
    if lab in label_addr:
        str_at[label_addr[lab] + off] = txt
sorted_str = sorted(str_at)
print('strings located:', len(str_at))

import bisect
def resolve(addr):
    """string containing addr (refs may point into the middle of a label)"""
    if addr in str_at: return str_at[addr]
    return None

# pass 2: functions in code, references
ref_re = re.compile(r'("?)([^\s",()]+?)\1(?:\+(0x[0-9A-Fa-f]+|\d+))?(?:@l|-_SDA2?_BASE_)')
func_re = re.compile(r'^"([^"]+)":\s*$|^([A-Za-z_][\w$@.<>]*):\s*$')
funcs = {}
for fn in files:
    sec = None; cur = None
    rel = os.path.relpath(fn, ASM)
    with open(fn, encoding='latin-1') as fh:
        for line in fh:
            m = sec_re.match(line)
            if m:
                sec = m.group(1); cur = None; continue
            if sec != '.text': continue
            m = func_re.match(line)
            if m:
                name = m.group(1) or m.group(2)
                if not name.startswith('lbl_'):
                    cur = name
                    funcs[cur] = {'file': rel, 'size': 0, 'strings': [], 'addr': None, 'calls': []}
                continue
            if cur and line.startswith('/*'):
                f = funcs[cur]
                if f['addr'] is None:
                    f['addr'] = line[3:11]
                f['size'] += 4
                bm = re.search(r'\tbl\s+"?([^"\s]+)"?\s*$', line)
                if bm: f['calls'].append(bm.group(1))
                for m in ref_re.finditer(line):
                    lab, off = m.group(2), m.group(3)
                    if lab in label_addr:
                        a = label_addr[lab] + (int(off, 0) if off else 0)
                        t = resolve(a)
                        if t is not None and t not in f['strings']:
                            f['strings'].append(t)

# demangle
names = list(funcs)
try:
    p = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True, text=True)
    dem = p.stdout.split('\n')
except FileNotFoundError:  # optional: match.py doesn't read 'demangled'
    dem = names
for n, d in zip(names, dem):
    funcs[n]['demangled'] = d if d and d != n else n

with open(OUT, 'w') as fh:
    json.dump(funcs, fh, indent=0)
print('functions:', len(funcs), ' with string refs:', sum(1 for f in funcs.values() if f['strings']))
