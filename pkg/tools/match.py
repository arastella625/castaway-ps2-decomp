#!/usr/bin/env python3
"""Match PS2 functions to Wii (ts2c) names using shared string references.
Usage: match.py wii_funcs.json ps2_funcs.json out_prefix [extra_names.csv]
Writes <out>.csv (all matches with confidence) and <out>_ghidra.txt (addr name for import).
"""
import sys, json, re, math, csv
from collections import defaultdict

wii = json.load(open(sys.argv[1])); ps2 = json.load(open(sys.argv[2])); out = sys.argv[3]

def norm(s):
    return s.replace('Wii', '@PLAT@').replace('PS2', '@PLAT@').replace('wii', '@plat@').replace('ps2', '@plat@')

def cw_demangle(n):
    """CodeWarrior/cfront-style -> (namespace list, base name)."""
    m = re.match(r'^(__ct|__dt|__\w+?|[^_].*?|_[^_].*?)__(?=[0-9QF])(.*)$', n)
    if not m: return [], n
    base, rest = m.group(1), m.group(2)
    ns = []
    if rest and rest[0].isdigit():
        k = re.match(r'(\d+)', rest); L = int(k.group(1)); ns = [rest[len(k.group(1)):len(k.group(1)) + L]]
    elif rest.startswith('Q'):
        cnt = int(rest[1]); r = rest[2:]
        for _ in range(cnt):
            k = re.match(r'(\d+)', r)
            if not k: break
            L = int(k.group(1)); ns.append(r[len(k.group(1)):len(k.group(1)) + L]); r = r[len(k.group(1)) + L:]
    if base == '__ct' and ns: base = ns[-1]
    elif base == '__dt' and ns: base = '~' + ns[-1]
    return ns, base

def pretty(n):
    ns, b = cw_demangle(n)
    ns = [x.replace('@unnamed@', 'anon_').replace('@', '_') for x in ns]
    return '::'.join(ns + [b])

w_by = defaultdict(set); p_by = defaultdict(set)
for n, f in wii.items():
    for s in f['strings']: w_by[norm(s)].add(n)
for a, f in ps2.items():
    for s in f['strings']: p_by[norm(s)].add(a)

def weight(s):
    # rarer strings are stronger evidence; very short strings are weak
    k = len(w_by[s]) + len(p_by[s])
    return (1.0 / k) * min(1.0, len(s) / 8.0)

score = defaultdict(lambda: defaultdict(float))
# sorted everywhere: set order changes per process, and ties decide tiers
for s in sorted(set(w_by) & set(p_by)):
    for a in sorted(p_by[s]):
        for n in sorted(w_by[s]):
            score[a][n] += weight(s)

def best(d):
    r = sorted(d.items(), key=lambda kv: (-kv[1], kv[0]))
    return r[0], (r[1][1] if len(r) > 1 else 0.0)

p_best = {a: best(d) for a, d in score.items()}
rev = defaultdict(dict)
for a, d in score.items():
    for n, v in d.items(): rev[n][a] = v
w_best = {n: best(d) for n, d in rev.items()}

rows = []
for a, ((n, sc), second) in p_best.items():
    mutual = w_best[n][0][0] == a
    pset = {norm(s) for s in ps2[a]['strings']}; wset = {norm(s) for s in wii[n]['strings']}
    shared = pset & wset
    unique_anchor = any(len(w_by[s]) == 1 and len(p_by[s]) == 1 for s in shared)
    jacc = len(shared) / len(pset | wset)
    ratio = ps2[a]['size'] / max(4, wii[n]['size'])
    if not (0.33 <= ratio <= 3.0): conf = 'review'
    elif mutual and unique_anchor and sc > 2 * second and jacc >= 0.5: conf = 'high'
    elif mutual and sc > 1.5 * second and (unique_anchor or len(shared) >= 2): conf = 'medium'
    else: conf = 'low'
    rows.append({'ps2_addr': a, 'name': pretty(n), 'wii_symbol': n, 'wii_file': wii[n]['file'],
                 'confidence': conf, 'shared': len(shared), 'jaccard': round(jacc, 2),
                 'score': round(sc, 3), 'runner_up': round(second, 3),
                 'ps2_size': ps2[a]['size'], 'wii_size': wii[n]['size'],
                 'example_strings': ' | '.join(sorted(shared)[:3])})

# ---- call-graph propagation from accepted matches ----
SKIP = re.compile(r'^(_savegpr|_restgpr|_savefpr|_restfpr|__save|__rest|__ptmf|__construct_array|__destroy_arr)')
assigned = {r['ps2_addr']: r['wii_symbol'] for r in rows if r['confidence'] in ('high', 'medium')}
taken = set(assigned.values())
# names from vtables.py / order_fill.py seed propagation alongside the string matches
if len(sys.argv) > 4:
    for r in csv.DictReader(open(sys.argv[4])):
        a, y = r['ps2_addr'], r['wii_symbol']
        if a in assigned or y in taken or y not in wii: continue
        assigned[a] = y; taken.add(y)
        rows.append({'ps2_addr': a, 'name': pretty(y), 'wii_symbol': y, 'wii_file': wii[y]['file'],
                     'confidence': r.get('tier') or 'vtable', 'shared': 0, 'jaccard': 0, 'score': 0, 'runner_up': 0,
                     'ps2_size': ps2.get(a, {}).get('size', 0), 'wii_size': wii[y]['size'],
                     'example_strings': r['evidence']})
new_total = 0
for rnd in range(6):
    votes = defaultdict(lambda: defaultdict(int))
    for a, n in list(assigned.items()):
        if a not in ps2: continue  # vtable target with no known body yet
        pc = ps2[a]['calls']; wc = [c for c in wii[n].get('calls', []) if not SKIP.match(c)]
        if len(pc) != len(wc) or not pc: continue
        for x, y in zip(pc, wc):
            if x in ps2 and y in wii: votes[x][y] += 1
    new = {}
    for x, d in votes.items():
        if x in assigned or len(d) != 1: continue
        (y, v), = d.items()
        if y in taken: continue
        ratio = ps2[x]['size'] / max(4, wii[y]['size'])
        if not (0.33 <= ratio <= 3.0): continue
        if v < 2 and len(ps2[x]['calls']) == 0 and ps2[x]['size'] < 16: continue
        new[x] = y
    # one wii name per round per ps2 addr; drop wii names claimed twice
    cnt = defaultdict(int)
    for y in new.values(): cnt[y] += 1
    new = {x: y for x, y in new.items() if cnt[y] == 1}
    if not new: break
    for x, y in new.items():
        assigned[x] = y; taken.add(y); new_total += 1
        rows.append({'ps2_addr': x, 'name': pretty(y), 'wii_symbol': y, 'wii_file': wii[y]['file'],
                     'confidence': 'propagated', 'shared': 0, 'jaccard': 0, 'score': 0, 'runner_up': 0,
                     'ps2_size': ps2[x]['size'], 'wii_size': wii[y]['size'], 'example_strings': 'via call graph round %d' % (rnd + 1)})
print('propagated names:', new_total)

order = {'high': 0, 'medium': 1, 'vtable': 2, 'vtorder': 3, 'propagated': 4, 'order': 5, 'review': 6, 'low': 7}
rows.sort(key=lambda r: (order[r['confidence']], r['ps2_addr']))
# a Wii name may be claimed by several PS2 funcs in low tier; only export unique names
with open(out + '.csv', 'w', newline='') as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0])); w.writeheader(); w.writerows(rows)
used = set()
with open(out + '_ghidra.txt', 'w') as fh:
    for r in rows:
        if r['confidence'] in ('low', 'review') or r['wii_symbol'] in used: continue
        used.add(r['wii_symbol'])
        fh.write(f"{r['ps2_addr']} {r['confidence']} {r['name']} {r['wii_symbol']}\n")
from collections import Counter
print(Counter(r['confidence'] for r in rows), 'exported', len(used))
