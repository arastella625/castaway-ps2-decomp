#!/usr/bin/env python3
"""Rebuild SLUS_216.64 from a splat split and the decompiled C++, and compare it with the game.

Usage: rebuild.py --split <folder> [--compilers <dir>] [--slus <SLUS_216.64>] [--asm-only] [--allow-kept]

Every function in src/ that can be linked replaces the game's assembly for it. Each C++
function is compiled into its own section, the same address range is cut out of the split
assembly, and a generated linker script puts every piece at its original address. The result
must have the SHA-1 in pkg/target.sha1. Exit status is 0 only when it does.

The game's .rodata is still raw binary, so a string literal or jump table in src/ has no
address of its own. Each reference to one is pointed at the game's copy, after checking that
the game's bytes there equal ours. A function stays as assembly, is listed, and fails the run
(unless --allow-kept) when that check fails or when it refers to a symbol with no address.

Run it in WSL with binutils-mips-linux-gnu installed. The split folder is splat's output
(see rebuild/README.md). It is the game's code, so keep it outside the repo. The compiler
runs under Wine with no network, the same way tools/verify.py runs it.
"""
import argparse
import hashlib
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import verify  # noqa: E402  (shares the compiler flags and the Wine sandbox)

WORK = verify.BUILD / 'rebuild'
UNLINKED = '__unlinked'
SHT_PROGBITS, STT_SECTION = 1, 3
R_MIPS_NONE, R_MIPS_HI16, R_MIPS_LO16 = 0, 5, 6
BINUTILS_TIMEOUT = 600  # seconds; assembling the whole game takes about 40
# A line of split assembly: /* file offset, address, bytes */ mnemonic operands
INSTRUCTION = re.compile(r'\s+/\* [0-9A-F]+ ([0-9A-F]{8}) ([0-9A-F]{8}) \*/')


def binutils(tool, *args):
    try:
        p = subprocess.run(['mips-linux-gnu-' + tool, *map(str, args)], capture_output=True, text=True,
                           timeout=BINUTILS_TIMEOUT)
    except subprocess.TimeoutExpired:
        raise verify.VerifyError('%s timed out after %d s' % (tool, BINUTILS_TIMEOUT))
    except FileNotFoundError:
        raise verify.VerifyError('mips-linux-gnu-%s not found: install binutils-mips-linux-gnu' % tool)
    if p.returncode != 0:
        raise verify.VerifyError('%s failed:\n%s' % (tool, p.stderr.strip()[:3000]))


def rel(path):
    """Path relative to the repo, so Wine never sees a Linux absolute path."""
    return path.relative_to(ROOT).as_posix()


def sext16(value):
    return value - 0x10000 if value & 0x8000 else value


def game_bytes(slus, address, length):
    """The game's bytes at an address, or None when no section holds that range."""
    for s in slus.sections:
        if s['type'] == SHT_PROGBITS and s['addr'] <= address and address + length <= s['addr'] + s['size']:
            start = s['offset'] + address - s['addr']
            return slus.data[start:start + length]
    return None


def same_data(obj, ro_bytes, ro_relocs, symbols, ours, slus, theirs, text_index, base):
    """Does the game hold, at `theirs`, the same thing our .rodata holds at offset `ours`?"""
    if ours in ro_relocs:
        # A jump table: each entry is an address inside the function that uses it.
        while ours in ro_relocs:
            kind, sym = ro_relocs[ours]
            entry = game_bytes(slus, theirs, 4)
            if kind != verify.R_MIPS_32 or symbols[sym][4] != text_index or entry is None:
                return False
            if struct.unpack('<I', entry)[0] != base + struct.unpack_from('<I', ro_bytes, ours)[0]:
                return False
            ours, theirs = ours + 4, theirs + 4
        return True
    end = ro_bytes.find(b'\0', ours)
    if ours < 0 or end < 0:
        return False
    return game_bytes(slus, theirs, end + 1 - ours) == ro_bytes[ours:end + 1]


def resolve_local_data(source, dest, addresses, slus):
    """Point a function's references to its object's own .rodata at the game's copy of it.

    The game's .rodata is still raw binary, so a string literal or jump table in our object
    has no address of its own. The game's instruction says where its copy is. If the bytes
    there equal ours, the address fields are filled in from the game and the relocations are
    retired. The address is taken on trust; the content is checked. Writes the object to
    `dest` and returns the names of the functions resolved this way.
    """
    obj = verify.Elf32(source)
    data = bytearray(obj.data)
    symbols = obj.symbols()
    index_of = {s['name']: i for i, s in enumerate(obj.sections)}
    resolved = set()
    ro_index = index_of.get('.rodata')
    if ro_index is not None:
        ro_bytes = obj.contents(obj.sections[ro_index])
        ro_relocs = obj.relocations(ro_index)
        for rel in obj.sections:
            target = obj.sections[rel['info']] if rel['type'] == verify.SHT_REL else None
            if target is None or not target['name'].startswith('.text.'):
                continue
            name = target['name'][len('.text.'):]
            if name not in addresses:
                continue
            base = addresses[name]
            sites = []  # (position of the relocation entry, offset in the function, type)
            for pos in range(rel['offset'], rel['offset'] + rel['size'], 8):
                r_offset, r_info = struct.unpack_from('<II', data, pos)
                sym = symbols[r_info >> 8]
                if sym[3] == STT_SECTION and sym[4] == ro_index:
                    sites.append((pos, r_offset, r_info & 0xFF))
            if not sites or any(kind not in (R_MIPS_HI16, R_MIPS_LO16) for _, _, kind in sites):
                continue
            ours = lambda offset: struct.unpack_from('<I', data, target['offset'] + offset)[0]
            words = {offset: game_bytes(slus, base + offset, 4) for _, offset, _ in sites}
            if None in words.values():
                continue
            game = {offset: struct.unpack('<I', w)[0] for offset, w in words.items()}
            highs = {game[o] & 0xFFFF for _, o, kind in sites if kind == R_MIPS_HI16}
            targets, ok = set(), True
            for _, offset, kind in sites:
                if kind == R_MIPS_HI16:
                    ok = ok and ours(offset) & 0xFFFF == 0  # our .rodata is far smaller than 64 KB
                    continue
                fits = {t for t in ((h << 16) + sext16(game[offset] & 0xFFFF) for h in highs)
                        if same_data(obj, ro_bytes, ro_relocs, symbols, sext16(ours(offset) & 0xFFFF), slus, t,
                                     rel['info'], base)}
                ok = ok and len(fits) == 1
                targets |= fits
            # every lui must be the top half of an address that was checked
            ok = ok and all(any((t + 0x8000) >> 16 & 0xFFFF == game[o] & 0xFFFF for t in targets)
                            for _, o, kind in sites if kind == R_MIPS_HI16)
            if not ok:
                continue
            for pos, offset, _ in sites:
                struct.pack_into('<I', data, target['offset'] + offset, ours(offset) & 0xFFFF0000 | game[offset] & 0xFFFF)
                struct.pack_into('<I', data, pos + 4, R_MIPS_NONE)
            resolved.add(name)
    Path(dest).write_bytes(data)
    return resolved


def classify(obj, addresses):
    """Sort an object's functions into linkable {name: size} and not linkable {name: reason}."""
    symbols = obj.symbols()
    linked, kept, calls = {}, {}, {}
    for name, _, _, kind, shndx in symbols:
        if kind != verify.STT_FUNC or not 0 < shndx < len(obj.sections):
            continue
        section = obj.sections[shndx]
        if section['name'] != '.text.' + name:
            continue
        reason = None if name in addresses else 'has no address in config/symbols.txt'
        for kind, index in obj.relocations(shndx).values():
            if kind == R_MIPS_NONE:
                continue  # resolve_local_data settled it
            target, _, _, target_kind, target_section = symbols[index]
            inside = obj.sections[target_section]['name'] if 0 < target_section < len(obj.sections) else ''
            if target_kind == STT_SECTION and inside.startswith('.text.'):
                # The assembler writes a call to a function in the same file as a reference
                # to that function's section.
                calls.setdefault(name, set()).add(inside[len('.text.'):])
            elif target_section and target_kind != verify.STT_FUNC:
                reason = "uses data from its own object file that the game's copy doesn't confirm"
            elif not target_section and target not in addresses:
                reason = 'refers to %s, which has no address in config/symbols.txt' % target
        if reason:
            kept[name] = reason
        else:
            linked[name] = section['size']
    # A kept function's section is discarded, so nothing linked may call into it.
    settled = False
    while not settled:
        settled = True
        for name in sorted(linked):
            missing = sorted(calls.get(name, set()) - set(linked))
            if missing:
                kept[name] = 'calls %s in the same file, which is not linked' % missing[0]
                del linked[name]
                settled = False
    return linked, kept


def compile_functions(tool_dir, source, addresses, slus):
    """Compile one source with a section per function. Returns (object, linked, kept)."""
    base = WORK / source.relative_to(verify.SRC)
    base.parent.mkdir(parents=True, exist_ok=True)
    ii, asm, raw, renamed, relinked, settled, obj, stamp = (
        base.with_suffix(e) for e in ('.ii', '.s', '.raw.o', '.link.s', '.link.o', '.data.o', '.o', '.key'))
    assemble = lambda src, dst: verify.run_tool(tool_dir, 'as.exe', verify.ASFLAGS + ['-o', rel(dst), rel(src)])
    # The compile is reused while the source and its headers are unchanged. Everything after
    # it depends on config/symbols.txt too, so it is redone every time; it is quick.
    key = verify.source_key(source, verify.CPPFLAGS + verify.CFLAGS + verify.ASFLAGS + ['-ffunction-sections'])
    if not (raw.is_file() and stamp.is_file() and stamp.read_text() == key):
        stamp.unlink(missing_ok=True)
        verify.run_tool(tool_dir, 'cpp.exe', verify.CPPFLAGS + [rel(source), rel(ii)])
        verify.run_tool(tool_dir, 'cc1plus.exe', verify.CFLAGS + ['-ffunction-sections', rel(ii), '-o', rel(asm)])
        assemble(asm, raw)
        stamp.write_text(key)
    resolve_local_data(raw, settled, addresses, slus)
    linked, kept = classify(verify.Elf32(settled), addresses)
    if kept:
        # Rename the definitions we can't link. Calls to them then go to the game's assembly,
        # and the renamed copies are discarded by the linker script.
        names = '|'.join(map(re.escape, kept))
        text = asm.read_bytes().decode('latin-1')
        text = re.sub(r'(?m)^(\t\.(?:globl|ent|end)\t)(%s)(\r?)$' % names, r'\1\2%s\3' % UNLINKED, text)
        text = re.sub(r'(?m)^(%s)(:\r?)$' % names, r'\1%s\2' % UNLINKED, text)
        renamed.write_bytes(text.encode('latin-1'))
        assemble(renamed, relinked)
        resolve_local_data(relinked, settled, addresses, slus)
    # The old assembler orders the symbol table in a way ld rejects; objcopy rewrites it.
    binutils('objcopy', settled, obj)
    return obj, linked, kept


def cut_assembly(source, dest, ranges):
    """Copy the split assembly without the address ranges that C++ replaces.

    Each run of kept code after a cut goes in its own section, so the linker script can place
    it. Returns [(section name, address of its first instruction)]. Alignment directives are
    dropped: every byte of the code, padding included, is an explicit instruction line.
    """
    chunks, pending = [], []
    index, dropping, open_label = 0, False, None
    with open(source, encoding='latin-1') as fin, open(dest, 'w', encoding='latin-1') as out:
        def close(lines):
            nonlocal open_label
            for line in lines:
                if open_label and line.split()[1] == open_label:
                    out.write(line)
                    open_label = None

        for line in fin:
            instruction = INSTRUCTION.match(line)
            if not instruction:
                if not line.startswith(('.align', '.section')):
                    pending.append(line)
                continue
            address, word = int(instruction.group(1), 16), instruction.group(2)
            while index < len(ranges) and address >= ranges[index][1]:
                index += 1
            # Lines since the last instruction: endlabel closes the function before them,
            # everything else (labels, glabel) belongs to this instruction.
            close(l for l in pending if l.startswith('endlabel'))
            opening = [l for l in pending if not l.startswith('endlabel')]
            pending = []
            starts_function = any(l.startswith('glabel') for l in opening)
            if (index < len(ranges) and address >= ranges[index][0]) or (
                    dropping and word == '00000000' and not starts_function):
                # Replaced by C++, or padding after it (the linker fills gaps with zeros).
                if not chunks:
                    raise verify.VerifyError('the first function of the game cannot be replaced')
                if open_label:
                    out.write('endlabel %s\n' % open_label)
                    open_label = None
                dropping = True
                continue
            if dropping or not chunks:
                # Never plain .text: the assembler pads that one to 16 bytes.
                name = '.text.asm_%08X' % address
                out.write('\n.section %s, "ax"\n' % name)
                chunks.append((name, address))
                dropping = False
            for l in opening:
                out.write(l)
                if l.startswith('glabel'):
                    open_label = l.split()[1]
            out.write(line)
        close(pending)
    return chunks


def write_linker_script(path, text_obj, chunks, functions, addresses, text_base):
    """functions: {name: object path} for the C++ functions being linked."""
    lines = ['/* Generated by rebuild.py. */']
    for name, address in sorted(addresses.items(), key=lambda kv: kv[1]):
        if name in functions:
            # Assembly refers to functions by address-based names; point those at the C++.
            lines.append('func_%08X = %s;' % (address, name))
        else:
            lines.append('%s = 0x%08X;' % (name, address))
    pieces = [(address, '"%s"(%s)' % (text_obj, name)) for name, address in chunks]
    pieces += [(addresses[name], '"%s"(.text.%s)' % (obj, name)) for name, obj in functions.items()]
    lines += ['SECTIONS', '{', '    .text 0x%08X :' % text_base, '    {']
    # Setting the position before every piece makes ld fail if the piece before it is too big.
    lines += ['        . = 0x%X; %s' % (address - text_base, spec) for address, spec in sorted(pieces)]
    lines += ['    }', '    /DISCARD/ : { *(*) }', '}']
    path.write_text('\n'.join(lines) + '\n')


def report_difference(rebuilt, original, text, functions, addresses, sizes):
    ranges = sorted((addresses[n], addresses[n] + sizes[n], n) for n in functions)
    count = min(len(rebuilt), len(original)) // 4 * 4
    diffs = [i for i in range(0, count, 4) if rebuilt[i:i + 4] != original[i:i + 4]]
    print('NO MATCH: %d words differ (sizes %d and %d)' % (len(diffs), len(rebuilt), len(original)))
    for i in diffs[:20]:
        ours, theirs = struct.unpack_from('<I', rebuilt, i)[0], struct.unpack_from('<I', original, i)[0]
        address = i - text['offset'] + text['addr']
        where = next((n for start, end, n in ranges if start <= address < end), 'not in a C++ function')
        print('  0x%08X  rebuilt %08X  game %08X  %s' % (address, ours, theirs, where))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--split', default=os.environ.get('CASTAWAY_SPLIT'),
                    help="folder holding splat's output (or set CASTAWAY_SPLIT)")
    ap.add_argument('--compilers', default=os.environ.get('CASTAWAY_COMPILERS'),
                    help='folder holding %s (or set CASTAWAY_COMPILERS)' % verify.COMPILER)
    ap.add_argument('--slus', default=os.environ.get('CASTAWAY_SLUS'),
                    help='path to SLUS_216.64 (or set CASTAWAY_SLUS)')
    ap.add_argument('--asm-only', action='store_true', help='link no C++, to test the split by itself')
    ap.add_argument('--allow-kept', action='store_true',
                    help='succeed even when some functions had to stay as assembly')
    args = ap.parse_args()
    try:
        if not args.split or not args.slus or not (args.asm_only or args.compilers):
            raise verify.VerifyError('give --split, --slus and --compilers, or set CASTAWAY_SPLIT, '
                                     'CASTAWAY_SLUS and CASTAWAY_COMPILERS')
        split = Path(args.split).resolve()
        if not (split / 'asm' / 'text.s').is_file():
            raise verify.VerifyError('no asm/text.s in %s: run splat first' % split)
        verify.check_target(args.slus)
        slus = verify.Elf32(args.slus)
        _, text = slus.section('.text')
        addresses = verify.load_symbols()
        WORK.mkdir(parents=True, exist_ok=True)

        functions, sizes, objects, kept_all = {}, {}, [], {}
        if not args.asm_only:
            verify.enter_sandbox()
            tool_dir = Path(args.compilers).resolve() / verify.COMPILER / verify.TOOL_DIR
            if not (tool_dir / 'cc1plus.exe').is_file():
                raise verify.VerifyError('no cc1plus.exe in %s' % tool_dir)
            sources = sorted(verify.SRC.rglob('*.cpp'))
            results = verify.build_all(lambda source: compile_functions(tool_dir, source, addresses, slus), sources)
            for obj, linked, kept in results:
                objects.append(obj)
                functions.update((name, obj) for name in linked)
                sizes.update(linked)
                kept_all.update(kept)
            print('linking %d C++ functions (%d bytes); %d kept as assembly'
                  % (len(functions), sum(sizes.values()), len(kept_all)))
            for name, reason in sorted(kept_all.items()):
                print('  kept: %s %s' % (name, reason))

        ranges = sorted((addresses[n], addresses[n] + sizes[n]) for n in functions)
        cut, text_obj, script, elf, binary = (WORK / n for n in
                                              ('text.s', 'text.o', 'link.ld', 'out.elf', 'text.bin'))
        chunks = cut_assembly(split / 'asm' / 'text.s', cut, ranges)
        # -mabi=eabi matches the game's e_flags; -mabi=32 expands every ld and sd into two.
        binutils('as', '-EL', '-march=r5900', '-mabi=eabi', '-G0', '-I', split / 'include', '-o', text_obj, cut)
        write_linker_script(script, text_obj, chunks, functions, addresses, text['addr'])
        binutils('ld', '-EL', '--no-check-sections', '-T', script, '-T', split / 'undefined_syms_auto.txt',
                 '-T', split / 'undefined_funcs_auto.txt', '-o', elf, text_obj, *objects)
        binutils('objcopy', '-O', 'binary', '-j', '.text', elf, binary)

        # The assembler pads .text; the game's own padding is at the start of after_text.bin.
        rebuilt = ((split / 'assets' / 'header.bin').read_bytes() + binary.read_bytes()[:text['size']]
                   + (split / 'assets' / 'after_text.bin').read_bytes())
        want = verify.TARGET_SHA1.read_text().split()[0]
        got = hashlib.sha1(rebuilt).hexdigest()
        print('sha1 rebuilt  %s\nsha1 expected %s' % (got, want))
        if got == want:
            print('MATCH: rebuilt file is byte-identical, %d assembly pieces and %d C++ functions'
                  % (len(chunks), len(functions)))
            if kept_all and not args.allow_kept:
                print('but %d functions could not be linked (listed above); fix them or pass --allow-kept'
                      % len(kept_all))
                return 1
            return 0
        report_difference(rebuilt, Path(args.slus).read_bytes(), text, functions, addresses, sizes)
        return 1
    except verify.VerifyError as e:
        print('rebuild: ' + str(e), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
