#!/usr/bin/env python3
"""Pair PS2 and Wii vtables that have no named slot, using table order and slot counts.

Tables keep nearly the same order on both platforms. Between two tables already aligned by
names, the sequences of slot counts are aligned with a longest common subsequence, and only
runs of consecutive matches are kept. Named slots then act as a check: a pair is rejected if
its slots contradict a known name, or another pair, in either direction. Multiple-inheritance
classes don't line up by length, so this skips them.

Usage: vtable_order.py <SLUS elf> <ts2c dir> <names.csv> <out.csv> [--holdout]
"""
import sys, os, csv, bisect
from collections import Counter, defaultdict
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vtables import ps2_tables, wii_vtables, SKIP

TRUSTED = ('high', 'medium', 'vtable', 'vtorder', 'propagated', 'order')
MIN_RUN = 3


def anchors(tables, vt, addr, names):
    """(ps2 index, wii index) pairs aligned by names, filtered to one rising sequence."""
    order = sorted(addr, key=addr.get)
    pos = {k: i for i, k in enumerate(order)}
    where = defaultdict(list)
    for k in order:
        for i, s in enumerate(vt[k]):
            where[s].append((k, i))
    pairs = []
    for ti, (_, slots) in enumerate(tables):
        votes = Counter()
        for i, p in enumerate(slots):
            for k, j in where.get(names.get(p), ()):
                if j - i == 2 and len(vt[k]) - 2 == len(slots):
                    votes[k] += 1
        if not votes:
            continue
        top = max(votes.values())
        best = [k for k, v in votes.items() if v == top]
        if len(best) == 1:
            pairs.append((ti, pos[best[0]]))
    # longest increasing subsequence on the Wii index
    tails, prev, idx = [], [None] * len(pairs), []
    for n, (_, w) in enumerate(pairs):
        i = bisect.bisect_left(tails, w)
        if i == len(tails):
            tails.append(w)
            idx.append(n)
        else:
            tails[i] = w
            idx[i] = n
        prev[n] = idx[i - 1] if i else None
    out, n = [], idx[-1] if idx else None
    while n is not None:
        out.append(pairs[n])
        n = prev[n]
    return out[::-1], order


def lcs(a, b):
    """Index pairs of a longest common subsequence of two length lists."""
    t = [[0] * (len(b) + 1) for _ in range(len(a) + 1)]
    for i in range(len(a) - 1, -1, -1):
        for j in range(len(b) - 1, -1, -1):
            t[i][j] = t[i + 1][j + 1] + 1 if a[i] == b[j] else max(t[i + 1][j], t[i][j + 1])
    i = j = 0
    out = []
    while i < len(a) and j < len(b):
        if a[i] == b[j] and t[i][j] == t[i + 1][j + 1] + 1:
            out.append((i, j))
            i += 1
            j += 1
        elif t[i + 1][j] >= t[i][j + 1]:
            i += 1
        else:
            j += 1
    return out


def align(tables, vt, addr, names, min_run):
    """Returns {ps2 function: wii symbol} for slots in newly paired tables."""
    names = dict(names)
    used = set(names.values())
    found = {}
    for _ in range(6):
        pairs, order = anchors(tables, vt, addr, names)
        lens_p = [len(s) for _, s in tables]
        lens_w = [len(vt[k]) - 2 for k in order]
        cand = []
        for (p0, w0), (p1, w1) in zip(pairs, pairs[1:]):
            m = lcs(lens_p[p0 + 1:p1], lens_w[w0 + 1:w1])
            run = []
            for x in m + [None]:
                if run and x and x[0] == run[-1][0] + 1 and x[1] == run[-1][1] + 1:
                    run.append(x)
                    continue
                if len(run) >= min_run:
                    cand += [(p0 + 1 + i, w0 + 1 + j) for i, j in run]
                run = [x] if x else []
        new = {}
        back = {}
        for ti, wi in cand:
            slots, wv = tables[ti][1], vt[order[wi]][2:]
            prop = {}
            ok = True
            for p, s in zip(slots, wv):
                if SKIP.match(s):
                    continue
                if names.get(p, s) != s or (s in used and names.get(p) != s) or new.get(p, s) != s or back.get(s, p) != p \
                        or prop.get(p, s) != s:
                    ok = False
                    break
                prop[p] = s
            if ok:
                new.update(prop)
                back.update({s: p for p, s in prop.items()})
        fresh = {p: s for p, s in new.items() if p not in names}
        if not fresh:
            break
        names.update(fresh)
        used.update(fresh.values())
        found.update(fresh)
    return found


def main():
    elf, ts2c, names_csv, out = sys.argv[1:5]
    tables = sorted(ps2_tables(elf))
    vt = wii_vtables(os.path.join(ts2c, 'asm'))
    addr = {}
    with open(os.path.join(ts2c, 'symbols.txt'), encoding='latin-1') as fh:
        for line in fh:
            p = line.split()
            if len(p) >= 8 and p[-1] in vt and len(vt[p[-1]]) > 2:
                addr[p[-1]] = int(p[1], 16)
    with open(names_csv, encoding='utf8', errors='replace') as fh:
        names = {int(r['ps2_addr'], 16): r['wii_symbol'] for r in csv.DictReader(fh) if r['confidence'] in TRUSTED}
    print('tables', len(tables), 'wii tables', len(addr), 'names', len(names))

    if '--holdout' in sys.argv:
        keys = sorted(names)
        for mr in (2, 3, 4, 6):
            right = wrong = none = 0
            bad = []
            for fold in range(5):
                hidden = set(keys[fold::5])
                got = align(tables, vt, addr, {a: s for a, s in names.items() if a not in hidden}, mr)
                for a in hidden:
                    if a not in got:
                        none += 1
                    elif got[a] == names[a]:
                        right += 1
                    else:
                        wrong += 1
                        bad.append('%08X want %s got %s' % (a, names[a][:44], got[a][:44]))
            print('min_run=%d predicted=%d right=%d wrong=%d no-prediction=%d' % (mr, right + wrong, right, wrong, none))
            for b in bad[:5]:
                print('   ', b)

    got = align(tables, vt, addr, names, MIN_RUN)
    print('min_run=%d names found: %d' % (MIN_RUN, len(got)))
    with open(out, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['ps2_addr', 'wii_symbol', 'evidence', 'tier'])
        for a in sorted(got):
            w.writerow(['%08X' % a, got[a], 'vtable order and slot counts', 'vtorder'])


if __name__ == '__main__':
    main()
