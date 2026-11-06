#!/usr/bin/env python3
"""Name functions by position between two named neighbours.

Unity builds keep functions in nearly the same local order on both platforms. When two
consecutive named PS2 functions map to two Wii functions in the same source file, and the
same number of functions sits between them on each side, the ones between pair up in order.
Hold-out accuracy was 209/210 on the 13,231-function list; misses were close neighbours in
the same class. Run it repeatedly with match.py: new names can make more stretches line up.

Usage: order_fill.py wii_funcs.json ghidra_functions.csv names.csv out.csv
"""
import sys, csv, json, bisect

wii_path, funcs_path, names_path, out_path = sys.argv[1:5]
TRUSTED = ('high', 'medium', 'vtable', 'vtorder', 'propagated', 'order')

wii = json.load(open(wii_path))
wii_addr = {n: int(f['addr'], 16) for n, f in wii.items() if f['addr']}
wii_sorted = sorted(wii_addr.values())
wii_at = {v: k for k, v in wii_addr.items()}
with open(funcs_path, newline='') as fh:
    ps2_sorted = sorted(int(r['start'], 16) for r in csv.DictReader(fh))

with open(names_path, encoding='utf8', errors='replace') as fh:
    named = {int(r['ps2_addr'], 16): r['wii_symbol'] for r in csv.DictReader(fh)
             if r['confidence'] in TRUSTED and r['wii_symbol'] in wii_addr}
taken = set(named.values())

found, claims = {}, {}
keys = sorted(named)
for a1, a2 in zip(keys, keys[1:]):
    n1, n2 = named[a1], named[a2]
    if wii_addr[n2] <= wii_addr[n1] or wii[n1]['file'] != wii[n2]['file']:
        continue
    ps2_mid = ps2_sorted[bisect.bisect_right(ps2_sorted, a1):bisect.bisect_left(ps2_sorted, a2)]
    wii_mid = wii_sorted[bisect.bisect_right(wii_sorted, wii_addr[n1]):bisect.bisect_left(wii_sorted, wii_addr[n2])]
    syms = [wii_at[w] for w in wii_mid]
    # a name already placed elsewhere means this stretch doesn't line up
    if not ps2_mid or len(ps2_mid) != len(syms) or taken.intersection(syms):
        continue
    for p, s in zip(ps2_mid, syms):
        found[p] = (s, '%s .. %s' % (n1, n2))
        claims[s] = claims.get(s, 0) + 1

with open(out_path, 'w', newline='') as fh:
    w = csv.writer(fh)
    w.writerow(['ps2_addr', 'wii_symbol', 'evidence', 'tier'])
    for p in sorted(found):
        s, why = found[p]
        if claims[s] == 1:
            w.writerow(['%08X' % p, s, 'between ' + why, 'order'])
print('order names:', sum(1 for s, _ in found.values() if claims[s] == 1))
