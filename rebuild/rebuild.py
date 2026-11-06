#!/usr/bin/env python3
"""Rebuild SLUS_216.64 from a splat split and the decompiled C++, and compare it with the game.

Usage: rebuild.py --split <folder> [--compilers <dir>] [--slus <SLUS_216.64>] [--asm-only]

Every function in src/ that can be linked replaces the game's assembly for it. Each C++
function is compiled into its own section, the same address range is cut out of the split
assembly, and a generated linker script puts every piece at its original address. The result
must have the SHA-1 in pkg/target.sha1. Exit status is 0 only when it does.

A function stays as assembly, and is listed, when it uses data from its own object file
(string literals, jump tables). The game's copy of that data is still raw binary, so those
addresses can't be linked yet.

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


def classify(obj, addresses):
    """Sort an object's functions into linkable {name: size} and not linkable {name: reason}."""
    symbols = obj.symbols()
    linked, kept = {}, {}
    for name, _, _, kind, shndx in symbols:
        if kind != verify.STT_FUNC or not 0 < shndx < len(obj.sections):
            continue
        section = obj.sections[shndx]
        if section['name'] != '.text.' + name:
            continue
        reason = None if name in addresses else 'has no address in config/symbols.txt'
        for _, index in obj.relocations(shndx).values():
            target, _, _, target_kind, target_section = symbols[index]
            if target_section and target_kind != verify.STT_FUNC:
                reason = 'uses data from its own object file'
            elif not target_section and target not in addresses:
                reason = 'refers to %s, which has no address in config/symbols.txt' % target
        if reason:
            kept[name] = reason
        else:
            linked[name] = section['size']
    return linked, kept


def compile_functions(tool_dir, source, addresses):
    """Compile one source with a section per function. Returns (object, linked, kept)."""
    base = WORK / source.relative_to(verify.SRC)
    base.parent.mkdir(parents=True, exist_ok=True)
    ii, asm, raw, obj = (base.with_suffix(e) for e in ('.ii', '.s', '.raw.o', '.o'))
    verify.run_tool(tool_dir, 'cpp.exe', verify.CPPFLAGS + [rel(source), rel(ii)])
    verify.run_tool(tool_dir, 'cc1plus.exe', verify.CFLAGS + ['-ffunction-sections', rel(ii), '-o', rel(asm)])
    assemble = lambda: verify.run_tool(tool_dir, 'as.exe', verify.ASFLAGS + ['-o', rel(raw), rel(asm)])
    assemble()
    linked, kept = classify(verify.Elf32(raw), addresses)
    if kept:
        # Rename the definitions we can't link. Calls to them then go to the game's assembly,
        # and the renamed copies are discarded by the linker script.
        names = '|'.join(map(re.escape, kept))
        text = asm.read_bytes().decode('latin-1')
        text = re.sub(r'(?m)^(\t\.(?:globl|ent|end)\t)(%s)(\r?)$' % names, r'\1\2%s\3' % UNLINKED, text)
        text = re.sub(r'(?m)^(%s)(:\r?)$' % names, r'\1%s\2' % UNLINKED, text)
        asm.write_bytes(text.encode('latin-1'))
        assemble()
    # The old assembler orders the symbol table in a way ld rejects; objcopy rewrites it.
    binutils('objcopy', raw, obj)
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
    args = ap.parse_args()
    try:
        if not args.split or not args.slus or not (args.asm_only or args.compilers):
            raise verify.VerifyError('give --split, --slus and --compilers, or set CASTAWAY_SPLIT, '
                                     'CASTAWAY_SLUS and CASTAWAY_COMPILERS')
        split = Path(args.split).resolve()
        if not (split / 'asm' / 'text.s').is_file():
            raise verify.VerifyError('no asm/text.s in %s: run splat first' % split)
        verify.check_target(args.slus)
        _, text = verify.Elf32(args.slus).section('.text')
        addresses = verify.load_symbols()
        WORK.mkdir(parents=True, exist_ok=True)

        functions, sizes, objects = {}, {}, []
        if not args.asm_only:
            verify.enter_sandbox()
            tool_dir = Path(args.compilers).resolve() / verify.COMPILER / verify.TOOL_DIR
            if not (tool_dir / 'cc1plus.exe').is_file():
                raise verify.VerifyError('no cc1plus.exe in %s' % tool_dir)
            kept_all = {}
            for source in sorted(verify.SRC.rglob('*.cpp')):
                obj, linked, kept = compile_functions(tool_dir, source, addresses)
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
            return 0
        report_difference(rebuilt, Path(args.slus).read_bytes(), text, functions, addresses, sizes)
        return 1
    except verify.VerifyError as e:
        print('rebuild: ' + str(e), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
