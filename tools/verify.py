#!/usr/bin/env python3
"""Build the decompiled sources with the game's compiler and compare them with the game.

Usage: verify.py --compilers <dir> --slus <SLUS_216.64> [-D NAME] [-v] [source.cpp ...]

Every function compiled from src/ must be listed in config/symbols.txt and must be
byte-identical to the game at its listed address. Exit status is 0 only when that holds.

Run it in WSL. The compiler is an old proprietary Windows binary, so it runs under Wine
inside a network namespace with no interfaces; the script re-executes itself under
`unshare -rn` when it finds a network. It uses the standard library only, so nothing
third-party runs next to the compiler.
"""
import argparse
import hashlib
import os
import socket
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'src'
BUILD = ROOT / 'build'
SYMBOLS = ROOT / 'config' / 'symbols.txt'
TARGET_SHA1 = ROOT / 'pkg' / 'target.sha1'

COMPILER = 'ee-gcc2.95.3-136'
TOOL_DIR = 'lib/gcc-lib/ee/2.95.3'
CPPFLAGS = ['-lang-c++', '-nostdinc', '-Isrc']
# -fno-exceptions: the game has no cleanup code around calls made while a temporary is alive
# (SimBodyPart::GetIconTextureID), which the compiler adds when exceptions are on.
CFLAGS = ['-quiet', '-O2', '-G0', '-fno-rtti', '-fno-exceptions']
ASFLAGS = ['-EL', '-mips3', '-mcpu=r5900', '-G0']
TOOL_TIMEOUT = 120  # seconds; a hung Wine process must not hang the build

SANDBOX_MARK = 'CASTAWAY_SANDBOXED'

SHT_SYMTAB, SHT_RELA, SHT_REL = 2, 4, 9
STT_FUNC = 2
R_MIPS_32, R_MIPS_26 = 2, 4
# Bits of an instruction word that the linker fills in. Anything not listed is a 16-bit field.
RELOC_MASK = {R_MIPS_32: 0xFFFFFFFF, R_MIPS_26: 0x03FFFFFF}
RELOC_MASK_DEFAULT = 0x0000FFFF


class VerifyError(Exception):
    """A problem the user has to fix: bad input, failed build, missing file."""


class Elf32:
    """The few parts of a little-endian ELF32 file this script needs."""

    def __init__(self, path):
        self.data = Path(path).read_bytes()
        if self.data[:6] != b'\x7fELF\x01\x01':
            raise VerifyError('%s is not a little-endian ELF32 file' % path)
        shoff, = struct.unpack_from('<I', self.data, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from('<HHH', self.data, 0x2E)
        fields = ('name', 'type', 'flags', 'addr', 'offset', 'size', 'link', 'info', 'align', 'entsize')
        self.sections = [dict(zip(fields, struct.unpack_from('<10I', self.data, shoff + i * shentsize)))
                         for i in range(shnum)]
        names = self.sections[shstrndx]
        for s in self.sections:
            s['name'] = self._string(names, s['name'])

    def _string(self, table, offset):
        start = table['offset'] + offset
        return self.data[start:self.data.index(b'\0', start)].decode('latin-1')

    def section(self, name):
        for i, s in enumerate(self.sections):
            if s['name'] == name:
                return i, s
        raise VerifyError('no %s section' % name)

    def contents(self, s):
        return self.data[s['offset']:s['offset'] + s['size']]

    def symbols(self):
        """[(name, value, size, type, section index)] in symbol-table order."""
        out = []
        for s in self.sections:
            if s['type'] != SHT_SYMTAB:
                continue
            strings = self.sections[s['link']]
            for off in range(s['offset'], s['offset'] + s['size'], 16):
                name, value, size, info, _, shndx = struct.unpack_from('<IIIBBH', self.data, off)
                out.append((self._string(strings, name), value, size, info & 0xF, shndx))
        return out

    def relocations(self, target_index):
        """{offset: (type, symbol index)} for the section with this index."""
        out = {}
        for s in self.sections:
            if s['type'] not in (SHT_REL, SHT_RELA) or s['info'] != target_index:
                continue
            step = 8 if s['type'] == SHT_REL else 12
            for off in range(s['offset'], s['offset'] + s['size'], step):
                r_offset, r_info = struct.unpack_from('<II', self.data, off)
                out[r_offset] = (r_info & 0xFF, r_info >> 8)
        return out


def enter_sandbox():
    """Make sure there is no network before any proprietary binary runs."""
    # Ask the kernel, not /sys/class/net: sysfs keeps showing the namespace it was mounted in.
    interfaces = {name for _, name in socket.if_nameindex()} - {'lo'}
    if not interfaces:
        return
    if os.environ.get(SANDBOX_MARK):
        raise VerifyError('still see network interfaces %s inside the namespace' % sorted(interfaces))
    os.environ[SANDBOX_MARK] = '1'
    os.execvp('unshare', ['unshare', '-rn', sys.executable, *sys.argv])


def check_target(slus):
    want = TARGET_SHA1.read_text().split()[0]
    got = hashlib.sha1(Path(slus).read_bytes()).hexdigest()
    if got != want:
        raise VerifyError('%s has SHA-1 %s, expected %s: wrong build of the game' % (slus, got, want))


def load_symbols():
    """config/symbols.txt -> {mangled symbol: address}."""
    out = {}
    for n, line in enumerate(SYMBOLS.read_text().splitlines(), 1):
        parts = line.split('#')[0].split()
        if not parts:
            continue
        if len(parts) != 2:
            raise VerifyError('%s:%d: expected "<address> <symbol>"' % (SYMBOLS.name, n))
        address, symbol = int(parts[0], 16), parts[1]
        if symbol in out:
            raise VerifyError('%s:%d: %s is listed twice' % (SYMBOLS.name, n, symbol))
        out[symbol] = address
    return out


def run_tool(tool_dir, exe, args):
    env = dict(os.environ, WINEPREFIX=str(Path.home() / '.wine-ps2'), WINEDEBUG='-all', WINEARCH='win32')
    cmd = ['wine', str(tool_dir / exe), *args]
    try:
        p = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True,
                           errors='replace', timeout=TOOL_TIMEOUT)
    except subprocess.TimeoutExpired:
        raise VerifyError('%s timed out after %d s' % (exe, TOOL_TIMEOUT))
    if p.returncode != 0 or 'error' in p.stderr.lower():
        raise VerifyError('%s failed (exit %d):\n%s' % (exe, p.returncode, p.stderr.strip()))
    if p.stderr.strip():
        print(p.stderr.strip())


def build(tool_dir, source, defines):
    """Preprocess, compile and assemble one source file. Returns the object path."""
    rel = source.relative_to(ROOT)
    out = BUILD / rel.relative_to('src')
    out.parent.mkdir(parents=True, exist_ok=True)
    ii, asm, obj = (out.with_suffix(e) for e in ('.ii', '.s', '.o'))
    # Paths are relative to ROOT, with forward slashes, so Wine never sees a Linux absolute path.
    r = lambda p: p.relative_to(ROOT).as_posix()
    run_tool(tool_dir, 'cpp.exe', CPPFLAGS + ['-D' + d for d in defines] + [rel.as_posix(), r(ii)])
    run_tool(tool_dir, 'cc1plus.exe', CFLAGS + [r(ii), '-o', r(asm)])
    run_tool(tool_dir, 'as.exe', ASFLAGS + ['-o', r(obj), r(asm)])
    return obj


def compare(obj_path, game, game_base, addresses, verbose):
    """Compare each function in the object with the game. Returns (bytes, functions, problems)."""
    obj = Elf32(obj_path)
    text_index, text = obj.section('.text')
    code = obj.contents(text)
    relocs = obj.relocations(text_index)
    symbols = obj.symbols()
    funcs = sorted((value, size, name) for name, value, size, kind, shndx in symbols
                   if shndx == text_index and kind == STT_FUNC)
    starts = [value for value, _, _ in funcs] + [len(code)]
    matched, count, problems = 0, 0, []
    for i, (start, size, name) in enumerate(funcs):
        end = start + size if size else starts[i + 1]
        address = addresses.get(name)
        if address is None:
            problems.append('%s: not in %s' % (name, SYMBOLS.name))
            continue
        offset = address - game_base
        if offset < 0 or offset + (end - start) > len(game):
            problems.append('%s: address 0x%08X is outside the game .text' % (name, address))
            continue
        diffs, unchecked = [], 0
        for pos in range(start, end, 4):
            ours, = struct.unpack_from('<I', code, pos)
            theirs, = struct.unpack_from('<I', game, offset + pos - start)
            mask = 0
            if pos in relocs:
                kind, sym = relocs[pos]
                mask = RELOC_MASK.get(kind, RELOC_MASK_DEFAULT)
                target = addresses.get(symbols[sym][0])
                if kind == R_MIPS_26 and target is not None:
                    # A call to a function we know the address of: check where the game's call goes.
                    ours, mask = (ours & ~mask) | (target >> 2 & mask), 0
                else:
                    unchecked += 1
            if (ours ^ theirs) & ~mask & 0xFFFFFFFF:
                diffs.append('    +0x%03x  ours %08X  game %08X' % (pos - start, ours, theirs))
        note = ', %d linker fields unchecked' % unchecked if unchecked else ''
        # Functions are aligned to 8 bytes, so only padding may follow ours. Anything else
        # means the game's function is longer than the one we compiled.
        tail = game[offset + end - start:offset + ((end - start + 7) & ~7)]
        if not diffs and any(tail):
            problems.append('%s: matches as far as it goes, but the game has more code after 0x%08X'
                            % (name, address + end - start))
            continue
        if diffs:
            problems.append('%s: %d of %d words differ at 0x%08X\n%s'
                            % (name, len(diffs), (end - start) // 4, address, '\n'.join(diffs[:12])))
        else:
            matched += end - start
            count += 1
            if verbose:
                print('  match 0x%08X %4d bytes  %s%s' % (address, end - start, name, note))
    return matched, count, problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--compilers', default=os.environ.get('CASTAWAY_COMPILERS'),
                    help='folder holding %s (or set CASTAWAY_COMPILERS)' % COMPILER)
    ap.add_argument('--slus', default=os.environ.get('CASTAWAY_SLUS'),
                    help='path to SLUS_216.64 (or set CASTAWAY_SLUS)')
    ap.add_argument('-D', dest='defines', action='append', default=[], metavar='NAME',
                    help='preprocessor define, e.g. NON_MATCHING')
    ap.add_argument('-v', '--verbose', action='store_true', help='list every matching function')
    ap.add_argument('sources', nargs='*', type=Path, help='default: every .cpp under src/')
    args = ap.parse_args()
    try:
        if not args.compilers or not args.slus:
            raise VerifyError('give --compilers and --slus, or set CASTAWAY_COMPILERS and CASTAWAY_SLUS')
        enter_sandbox()
        tool_dir = Path(args.compilers).resolve() / COMPILER / TOOL_DIR
        if not (tool_dir / 'cc1plus.exe').is_file():
            raise VerifyError('no cc1plus.exe in %s' % tool_dir)
        check_target(args.slus)
        slus = Elf32(args.slus)
        _, text = slus.section('.text')
        game, game_base = slus.contents(text), text['addr']
        addresses = load_symbols()
        sources = [p.resolve() for p in args.sources] or sorted(SRC.rglob('*.cpp'))
        total, count, failed = 0, 0, 0
        for source in sources:
            print(source.relative_to(ROOT).as_posix())
            matched, functions, problems = compare(build(tool_dir, source, args.defines), game,
                                                   game_base, addresses, args.verbose)
            total += matched
            count += functions
            failed += len(problems)
            for p in problems:
                print('  FAIL ' + p)
        print('%d functions match: %d bytes, %.3f%% of the game code; %d problems'
              % (count, total, 100.0 * total / len(game), failed))
        return 1 if failed else 0
    except VerifyError as e:
        print('verify: ' + str(e), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
