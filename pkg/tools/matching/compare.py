"""Compare compiler output to game instructions. Usage: compare.py <run log> <targets.txt>"""
import os, re, sys, difflib, collections
R = {'$0': 'zero', '$1': 'at', '$2': 'v0', '$3': 'v1', '$4': 'a0', '$5': 'a1', '$6': 'a2', '$7': 'a3', '$16': 's0', '$17': 's1',
     '$18': 's2', '$19': 's3', '$20': 's4', '$21': 's5', '$22': 's6', '$23': 's7', '$28': 'gp', '$29': 'sp', '$30': 's8', '$31': 'ra', '$sp': 'sp', '$fp': 's8'}
for i in range(8, 16): R['$%d' % i] = 't%d' % (i - 8)
for i in (24, 25): R['$%d' % i] = 't%d' % (i - 16)
BASE = os.path.dirname(os.path.abspath(sys.argv[1])) + os.sep  # paths in the log are relative to it
REGS = set(R.values())


def hexify(s):
    return re.sub(r'(?<![\w.$])(-?)(\d+)(?![\w.])', lambda m: m.group(1) + hex(int(m.group(2))), s)


def norm(line):
    """Mnemonic plus operands, with branch targets dropped."""
    p = line.split(None, 1)
    m = p[0].lstrip('_')
    ops = p[1].replace(' ', '') if len(p) > 1 else ''
    if m == 'j' and ops == 'ra':
        m, ops = 'jr', 'ra'
    # assembler spellings of the same encoded instruction
    q = ops.split(',')
    if m == 'jal' and q[-1] in REGS:  # gcc writes jalr as "jal $31,$2"
        return 'jalr ' + q[-1]
    if m == 'subu' and len(q) == 3 and q[2].startswith('0x'):
        m, q[2] = 'addiu', '-' + q[2]
    elif m == 'addu' and len(q) == 3 and q[2].lstrip('-').startswith('0x'):
        m = 'addiu'
    ops = ','.join(re.sub(r'(?<![\w])(-?)0x0*([0-9a-f]+)', r'\g<1>0x\2', x) for x in q)
    if m in ('jal', 'b') or m.startswith('b') and m not in ('break',):
        ops = ','.join(x for x in ops.split(',')[:-1]) if m != 'jal' else ''
    return m + ' ' + ops


def game(path):
    funcs, cur = [], None
    for l in open(path, encoding='utf8', errors='replace'):
        m = re.match(r'[0-9a-f]{8} (.+)', l)
        if not m or m.group(1).startswith('??'):
            continue
        t = m.group(1).split('   ')[0]
        if cur is None:
            cur = []
            funcs.append(cur)
        cur.append(norm(t))
        if t.lstrip('_').startswith('addiu sp,sp,0x') or (len(cur) > 1 and cur[-2] == 'jr ra'):
            if cur[-2] == 'jr ra':
                cur = None
    return funcs


def asm(path):
    out = collections.OrderedDict()
    cur = None
    for l in open(path, encoding='latin-1'):
        s = l.split('#')[0].strip()
        m = re.match(r'\.ent\s+(\S+)', s)
        if m:
            cur = m.group(1)
            out[cur] = []
            continue
        if s.startswith('.end'):
            cur = None
            continue
        if cur is None or not s or s.startswith('.') or s.endswith(':'):
            continue
        s = re.sub(r'\$\w+', lambda m: R.get(m.group(), m.group()), s)
        p = s.split(None, 1)
        out[cur].append(norm(p[0] + (' ' + hexify(p[1]) if len(p) > 1 else '')))
    return out


targets = {'SetClothingSetIndex': None, 'GetNumPossibleClothingSets': None}
fs = game(sys.argv[2])
big = [f for f in fs if len(f) > 10]
targets['SetClothingSetIndex'], targets['GetNumPossibleClothingSets'] = big[0], big[1]
# whole-range targets: a function with several returns can't be split on "jr ra"
for extra in sys.argv[5:]:
    name, path = extra.split('=')
    targets[name] = [norm(m.group(1).split('   ')[0]) for m in
                     (re.match(r'[0-9a-f]{8} (.+)', l) for l in open(path, encoding='utf8', errors='replace'))
                     if m and not m.group(1).startswith('??')]
print('targets', {k: len(v) for k, v in targets.items()})
rows = []
failed = collections.Counter()
first_error = {}
for line in open(sys.argv[1], encoding='utf8', errors='replace'):
    p = line.strip().split('|')
    if len(p) != 5:
        continue
    b, i, flags, rc, o = p
    err = BASE + o + '.err'
    text = open(err, encoding='latin-1').read().strip()
    if rc != '0' or 'error' in text.lower():
        failed[b] += 1
        first_error.setdefault(b, text.splitlines()[0][:100] if text else 'rc=' + rc)
        continue
    try:
        a = asm(BASE + o)
    except OSError:
        rows.append((0, b, flags, 'no output rc=' + rc))
        continue
    sc, detail = [], []
    for name, t in targets.items():
        k = [x for x in a if x.startswith(name)]
        if not k:
            sc.append(0)
            detail.append('missing')
            continue
        r = difflib.SequenceMatcher(None, a[k[0]], t, autojunk=False).ratio()
        sc.append(r)
        detail.append('%.3f%s' % (r, ' EXACT' if a[k[0]] == t else ''))
    rows.append((sum(sc) / len(sc), b, flags, ' | '.join(detail)))
print('failed compiles', dict(failed), first_error)
rows.sort(key=lambda r: -r[0])
for r in rows[:24]:
    print('%.3f %-20s %-48s %s' % r)
best = collections.defaultdict(float)
for r in rows:
    best[r[1]] = max(best[r[1]], r[0])
print('best per compiler', {k: round(v, 3) for k, v in best.items()})
if len(sys.argv) > 3:
    b, i = sys.argv[3], sys.argv[4]
    a = asm(BASE + 'out/body_%s_%s.s' % (b, i))
    for name, t in targets.items():
        k = [x for x in a if x.startswith(name)][0]
        print('--- diff', name)
        for d in difflib.unified_diff(t, a[k], 'game', b, lineterm='', n=1):
            print('  ', d)
