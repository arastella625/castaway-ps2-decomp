#!/usr/bin/env python3
"""Name PS2 virtual functions by aligning PS2 vtables with Wii vtables.

PS2 vtable slots are 8 bytes ({delta, index} word, then function pointer), so tables show up
as a pointer every second word. Wii vtables have named slots after a 2-word header. A PS2
table is aligned to a Wii table by voting with functions that already have trusted names;
the remaining slots then get their names by position.

Usage: vtables.py <SLUS elf> <ts2c/asm dir> <names.csv> <out.csv> [--holdout] [--all-tiers]
--holdout hides each fifth of the trusted names in turn and scores predicting them.
"""
import sys, os, re, csv, struct
from collections import Counter, defaultdict
from elftools.elf.elffile import ELFFile

ANCHOR_TIERS = ('high', 'medium')
SKIP = re.compile(r'^(0x|__pure_virtual|__cxa_pure)')


def ps2_tables(path):
    """Runs of {0, .text pointer} entries in .data/.rodata: (address, [slot targets])."""
    with open(path, 'rb') as f:
        elf = ELFFile(f)
        text = elf.get_section_by_name('.text')
        lo, hi = text['sh_addr'], text['sh_addr'] + text['sh_size']
        out = []
        for name in ('.data', '.rodata'):
            s = elf.get_section_by_name(name)
            d = s.data()
            n = len(d) // 4
            w = struct.unpack('<%dI' % n, d[:n * 4])
            entry = lambda i: i + 1 < n and w[i] == 0 and lo <= w[i + 1] < hi and w[i + 1] % 4 == 0
            i = 0
            while i < n:
                if not entry(i):
                    i += 1
                    continue
                j, slots = i, []
                while entry(j):
                    slots.append(w[j + 1])
                    j += 2
                if len(slots) >= 2:
                    out.append((s['sh_addr'] + i * 4, slots))
                i = j
    return out


def wii_vtables(asm):
    """{__vt__Class: [header words and slot symbols]} from the ts2c asm."""
    label = re.compile(r'^"?(__vt__[^":]+)"?:\s*$')
    entry = re.compile(r'^\s+\.4byte\s+(.*?)\s*(;#.*)?$')
    vt = {}
    for root, _, files in os.walk(asm):
        for fn in sorted(files):
            if not fn.endswith('.s'):
                continue
            cur = None
            with open(os.path.join(root, fn), encoding='latin-1') as fh:
                for line in fh:
                    m = label.match(line)
                    if m:
                        cur = m.group(1)
                        vt[cur] = []
                    elif cur and not line.strip().startswith('#'):
                        m = entry.match(line)
                        if m:
                            vt[cur].append(m.group(1).strip('"'))
                        else:
                            cur = None
    return vt


def derive(tables, vt, anchors, min_votes):
    """Grow anchors through vtable alignment until nothing changes. Returns {addr: (sym, why)}."""
    where = defaultdict(list)
    for k in sorted(vt):
        for i, s in enumerate(vt[k]):
            where[s].append((k, i))
    names = dict(anchors)
    found = {}
    for _ in range(8):
        proposals = defaultdict(lambda: defaultdict(list))
        for addr, slots in tables:
            votes = Counter()
            for i, p in enumerate(slots):
                for k, j in where.get(names.get(p), ()):
                    if j - i >= 2:
                        votes[(k, j - i)] += 1
            good = []
            for (k, off), v in votes.items():
                if v < min_votes or off + len(slots) > len(vt[k]):
                    continue
                # a trusted name in the wrong slot rules the alignment out
                if any(p in names and vt[k][off + i] != names[p] for i, p in enumerate(slots)):
                    continue
                # fragments must still touch the start or end of the Wii table
                if off != 2 and off + len(slots) != len(vt[k]):
                    continue
                good.append((v, k, off))
            if not good:
                continue
            top = max(g[0] for g in good)
            tied = [g for g in good if g[0] == top]
            for i, p in enumerate(slots):
                if p in names:
                    continue
                # sibling classes tie; only trust slots they all agree on
                syms = {vt[k][off + i] for _, k, off in tied}
                if len(syms) == 1:
                    (sym,) = syms
                    if not SKIP.match(sym):
                        proposals[p][sym].append('%08X' % addr + ' ' + tied[0][1])
        by_sym = Counter(next(iter(d)) for d in proposals.values() if len(d) == 1)
        taken = set(names.values())
        new = 0
        for p in sorted(proposals):
            d = proposals[p]
            if len(d) != 1:
                continue
            (sym, why), = d.items()
            if by_sym[sym] == 1 and sym not in taken:
                names[p] = sym
                found[p] = (sym, why[0])
                new += 1
        if not new:
            break
    return found


def main():
    elf, asm, names_csv, out = sys.argv[1:5]
    tables, vt = ps2_tables(elf), wii_vtables(asm)
    with open(names_csv, encoding='utf8', errors='replace') as fh:
        rows = list(csv.DictReader(fh))
    # --all-tiers: anchor on every accepted name, for later passes once the set is checked
    tiers = ANCHOR_TIERS + ('vtable', 'vtorder', 'propagated', 'order') if '--all-tiers' in sys.argv else ANCHOR_TIERS
    anchors = {int(r['ps2_addr'], 16): r['wii_symbol'] for r in rows if r['confidence'] in tiers}
    print('tables', len(tables), 'wii vtables', len(vt), 'anchors', len(anchors))
    # every slot is a function entry point, named or not (input for CreateFunctionsAt.java)
    with open(out + '.slots.txt', 'w') as fh:
        fh.writelines('%08X\n' % a for a in sorted({p for _, slots in tables for p in slots}))

    if '--holdout' in sys.argv:
        keys = sorted(anchors)
        for mv in (1, 2, 3):
            right = wrong = silent = 0
            bad = []
            for fold in range(5):
                hidden = set(keys[fold::5])
                got = derive(tables, vt, {a: s for a, s in anchors.items() if a not in hidden}, mv)
                for a in hidden:
                    if a not in got:
                        silent += 1
                    elif got[a][0] == anchors[a]:
                        right += 1
                    else:
                        wrong += 1
                        bad.append('%08X want %s got %s' % (a, anchors[a], got[a][0]))
            print('min_votes=%d predicted=%d right=%d wrong=%d no-prediction=%d' % (mv, right + wrong, right, wrong, silent))
            for b in bad[:6]:
                print('   ', b)

    mv = 2
    got = derive(tables, vt, anchors, mv)
    tier = defaultdict(dict)
    for r in rows:
        tier[int(r['ps2_addr'], 16)][r['wii_symbol']] = r['confidence']
    agree = Counter()
    for a, (s, _) in got.items():
        t = tier.get(a, {})
        agree[('same as ' + t[s]) if s in t else ('differs from ' + '/'.join(sorted(set(t.values())))) if t else 'new'] += 1
    print('min_votes=%d names found: %d' % (mv, len(got)))
    for k, v in sorted(agree.items()):
        print('  ', k, v)
    with open(out, 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['ps2_addr', 'wii_symbol', 'evidence'])
        for a in sorted(got):
            w.writerow(['%08X' % a, got[a][0], got[a][1]])


if __name__ == '__main__':
    main()
