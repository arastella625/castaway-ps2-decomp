#!/usr/bin/env python3
"""Compare a rebuilt executable with the original. Usage: compare.py <rebuilt> <original> <target.sha1>"""
import hashlib, struct, sys

new, old = open(sys.argv[1], 'rb').read(), open(sys.argv[2], 'rb').read()
want = open(sys.argv[3]).read().split()[0]
got = hashlib.sha1(new).hexdigest()
print('size rebuilt %d original %d' % (len(new), len(old)))
print('sha1 rebuilt  ', got)
print('sha1 expected ', want)
if got == want:
    print('MATCH: rebuilt file is byte-identical')
    sys.exit(0)
n = min(len(new), len(old)) // 4 * 4
diffs = [i for i in range(0, n, 4) if new[i:i + 4] != old[i:i + 4]]
print('NO MATCH: %d differing words of %d' % (len(diffs), n // 4))
for i in diffs[:20]:
    a, b = struct.unpack_from('<I', new, i)[0], struct.unpack_from('<I', old, i)[0]
    print('  file %06X vram %08X rebuilt %08X original %08X' % (i, i - 0x80 + 0x100000, a, b))
sys.exit(1)
