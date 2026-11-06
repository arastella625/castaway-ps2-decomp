"""Byte-compare assembled functions with the game. Usage: bytes.py <object.o> <SLUS elf>"""
import struct, sys
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

TARGETS = {'SetClothingSetIndex__11SimBodyPartUc': 0x00147958,
           'GetNumPossibleClothingSets__11SimBodyPart': 0x00147A20,
           'IsGeneric__11SimBodyPart': 0x001380B8}
g = ELFFile(open(sys.argv[2], 'rb'))
t = g.get_section_by_name('.text')
gd, gb = t.data(), t['sh_addr']

o = ELFFile(open(sys.argv[1], 'rb'))
text = o.get_section_by_name('.text')
od = text.data()
sym = o.get_section_by_name('.symtab')
rel = set()
for s in o.iter_sections():
    if isinstance(s, RelocationSection) and s.name in ('.rel.text', '.rela.text'):
        rel |= {r['r_offset'] for r in s.iter_relocations()}
print('object .text', len(od), 'bytes, relocations', len(rel))
funcs = sorted((x['st_value'], x['st_size'], x.name) for x in sym.iter_symbols() if x['st_info']['type'] == 'STT_FUNC' or x.name in TARGETS)
print('symbols', [(hex(a), n[:36]) for a, _, n in funcs][:8])
starts = [a for a, _, _ in funcs] + [len(od)]
total_same = total = 0
for a, size, name in funcs:
    addr = next((v for k, v in TARGETS.items() if name.startswith(k)), None)
    if addr is None:
        continue
    end = a + size if size else starts[starts.index(a) + 1]
    same = masked = n = 0
    diffs = []
    for off in range(a, end, 4):
        w1 = struct.unpack_from('<I', od, off)[0]
        w2 = struct.unpack_from('<I', gd, addr - gb + off - a)[0]
        n += 1
        if off in rel:
            masked += 1
            continue
        if w1 == w2:
            same += 1
        else:
            diffs.append('+%x ours %08X game %08X' % (off - a, w1, w2))
    print('%-46s %3d words: %3d identical, %d relocated, %d different' % (name[:46], n, same, masked, len(diffs)))
    for d in diffs[:8]:
        print('   ', d)
    total_same += same
    total += n - masked
print('total identical %d of %d comparable words' % (total_same, total))
