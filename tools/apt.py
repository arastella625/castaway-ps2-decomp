#!/usr/bin/env python3
"""Read the game's Flash-style UI movies (EA Apt) from FLASHES.ARC.

Usage:
  python -I tools/apt.py check  <game.iso>
  python -I tools/apt.py find   <game.iso> <script name>
  python -I tools/apt.py export <game.iso> <movie> <out_dir>

Each FLASHES entry is an HSLF wrapper round a BIGF archive of <movie>.apt (characters and
timelines), <movie>.const (the constants its scripts use), <movie>.geo (shape geometry, layout
open) and 001.tgq on (8-bit texture sheets). check reads all 147. find names the movies whose
constants hold a script name: the game calls into a movie by name, such as ActionMenu's
imuSetBackdrop. export writes a movie's sheets as PNG and its header, exports, fonts and strings
as JSON. All of it is game data: keep it out of the repo. docs/formats/media.md has the format.
"""
import collections
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import texture  # noqa: E402

ARCHIVE = 'FLASHES'
WRAPPER_VERSION, WRAPPER_TAG = 3, b'HSLF'
SIGNATURE = 0x09876543  # every character's second word
MOVIE, FONT = 9, 3
CHARACTER_NAMES = {1: 'shape', 2: 'text', 3: 'font', 4: 'button', 5: 'sprite', 7: 'bitmap', 9: 'movie'}
CONST_TAG = b'Apt constant file\x1a\0\0'
CONST_STRING = 1
WILDCARD = '*.wav'  # an entry at the archive's end with size 0xFFFFFFFF and no data


def u32(data, pos):
    if not 0 <= pos <= len(data) - 4:
        raise arc.FormatError('Apt: a read past the end at %#x' % pos)
    return struct.unpack_from('<I', data, pos)[0]


def cstring(data, pos):
    end = data.find(b'\0', pos)
    if end < 0:
        raise arc.FormatError('Apt: an unterminated string at %#x' % pos)
    return data[pos:end].decode('latin-1')


def unwrap(data, name):
    """The HSLF wrapper: version 3, tag, -1, name size, name, u32 bytes left, then BIGF."""
    version, tag, _, name_size = struct.unpack_from('<I4siI', data, 0)
    if version != WRAPPER_VERSION or tag != WRAPPER_TAG:
        raise arc.FormatError('%s: not an HSLF wrapper' % name)
    base = 16 + name_size + 4
    if u32(data, 16 + name_size) != len(data) - base:
        raise arc.FormatError('%s: the wrapper size does not match' % name)
    return members(data, base, name)


def members(data, base, name):
    """BIGF: u32 LE size, big-endian count and header end, then offset, size and name per member."""
    if data[base:base + 4] != b'BIGF' or u32(data, base + 4) != len(data) - base:
        raise arc.FormatError('%s: not a BIGF archive of the rest of the file' % name)
    count, header_end = struct.unpack_from('>II', data, base + 8)
    pos, entries = base + 16, []
    for _ in range(count):
        offset, size = struct.unpack_from('>II', data, pos)
        end = data.index(b'\0', pos + 8)
        entries.append((data[pos + 8:end].decode('latin-1'), offset, size))
        pos = end + 1
    total, out, at = len(data) - base, {}, header_end
    for member, offset, size in entries:
        if size == 0xFFFFFFFF:
            if member != WILDCARD or offset != total:
                raise arc.FormatError('%s: an empty member that is not the wildcard' % name)
            continue
        if offset % 4 or not 0 <= offset - at <= 3:
            raise arc.FormatError('%s: member %s is not where the last one ended' % (name, member))
        out[member] = data[base + offset:base + offset + size]
        at = offset + size
    if not 0 <= total - at <= 3:
        raise arc.FormatError('%s: members end before the archive does' % name)
    return out


def constants(const, name):
    if not const.startswith(CONST_TAG):
        raise arc.FormatError('%s: not an Apt constant file' % name)
    movie, count, table = u32(const, 20), u32(const, 24), u32(const, 28)
    strings = [cstring(const, u32(const, table + i * 8 + 4)) for i in range(count)
               if u32(const, table + i * 8) == CONST_STRING]
    return movie, strings


def movie_info(apt, movie, name):
    if u32(apt, movie) != MOVIE or u32(apt, movie + 4) != SIGNATURE:
        raise arc.FormatError('%s: no movie character where the constants say' % name)
    frames, characters, character_at = u32(apt, movie + 8), u32(apt, movie + 20), u32(apt, movie + 24)
    exports_count, export_at = u32(apt, movie + 48), u32(apt, movie + 52)
    kinds, fonts = collections.Counter(), []
    for index in range(characters):
        at = u32(apt, character_at + index * 4)
        if not at:
            continue
        if u32(apt, at + 4) != SIGNATURE:
            raise arc.FormatError('%s: character %d has no signature' % (name, index))
        kind = u32(apt, at)
        kinds[CHARACTER_NAMES.get(kind, 'type %d' % kind)] += 1
        if kind == FONT:
            fonts.append(cstring(apt, u32(apt, at + 8)))
    exports = [[cstring(apt, u32(apt, export_at + i * 8)), u32(apt, export_at + i * 8 + 4)]
               for i in range(exports_count)]  # names repeat within a movie, so a list
    return {'width': u32(apt, movie + 28), 'height': u32(apt, movie + 32),
            'frame_ms': u32(apt, movie + 36), 'frames': frames, 'characters': dict(kinds),
            'fonts': sorted(set(fonts)), 'exports': exports}


def sheet(data, name):
    """A .tgq: TXFL, 16 zero bytes, then an archive texture's fields; pixels at 36."""
    if data[:4] != b'TXFL' or any(data[4:20]):
        raise arc.FormatError('%s: not a TXFL sheet' % name)
    width, height, palette, mips = struct.unpack_from('<HHHH', data, 20)
    kind, bits = data[28], data[30]
    split = 36 + width * height * bits // 8
    if split + palette * 4 + 8 != len(data) or struct.unpack_from('<II', data, len(data) - 8) != (width, height):
        raise arc.FormatError('%s: the sheet does not fill its member' % name)
    return texture.Texture(name, width, height, 0, kind, bits, palette, mips, data[36:split], data[split:-8])


def read_movie(disc, entry):
    archive = arc.find_archive(disc, ARCHIVE)
    parts = unwrap(disc.read(archive.lba, entry.offset, entry.size), entry.name)
    movie, strings = constants(parts[entry.name + '.const'], entry.name)
    info = movie_info(parts[entry.name + '.apt'], movie, entry.name)
    info['strings'] = strings
    sheets = {member: sheet(data, member) for member, data in sorted(parts.items()) if member.endswith('.tgq')}
    return info, sheets


def entries(disc):
    return sorted(arc.read_toc(disc, arc.find_archive(disc, ARCHIVE)), key=lambda e: e.name)


def check(disc):
    totals, failures = collections.Counter(), []
    for entry in entries(disc):
        try:
            info, sheets = read_movie(disc, entry)
            totals['movies'] += 1
            totals['sheets'] += len(sheets)
            totals['exports'] += len(info['exports'])
            totals.update(info['characters'])
        except (arc.FormatError, ValueError, KeyError) as error:
            failures.append('%s: %s' % (entry.name, error))
    for key, value in sorted(totals.items()):
        print('%-10s %6d' % (key, value))
    archive = arc.find_archive(disc, ARCHIVE)
    first = entries(disc)[0]
    data = disc.read(archive.lba, first.offset, first.size)
    print('controls:')
    passed = not failures

    def rejects(label, broken):
        try:
            unwrap(broken, first.name)
        except (arc.FormatError, ValueError, struct.error):
            print('  control: %s is rejected' % label)
            return True
        print('  CONTROL FAILED: %s was accepted' % label)
        return False
    passed &= rejects('a movie cut short', data[:-4])
    passed &= rejects('a wrong wrapper tag', data[:4] + b'XXXX' + data[8:])
    print('failures: %d' % len(failures))
    for failure in failures[:10]:
        print('   ', failure)
    return 0 if passed else 1


def find(disc, wanted):
    hits = [e.name for e in entries(disc) if wanted in read_movie(disc, e)[0]['strings']]
    print('%s: %s' % (wanted, ', '.join(hits) or 'in no movie'))
    return 0 if hits else 1


def export(disc, movie, out_dir):
    entry = next((e for e in entries(disc) if e.name == movie), None)
    if entry is None:
        raise arc.FormatError('no movie %s' % movie)
    info, sheets = read_movie(disc, entry)
    os.makedirs(out_dir, exist_ok=True)
    for member, image in sheets.items():
        with open(os.path.join(out_dir, '%s_%s.png' % (movie, member[:-4])), 'wb') as out:
            out.write(texture.png_bytes(image.width, image.height, texture.rgba(image)))
    info['sheets'] = ['%s_%s.png' % (movie, m[:-4]) for m in sheets]
    with open(os.path.join(out_dir, movie + '.json'), 'w', encoding='utf-8') as out:
        json.dump(info, out, indent=1)
    print('wrote %s: %d sheets, %d exports, %d strings' % (out_dir, len(sheets), len(info['exports']),
                                                         len(info['strings'])))
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 4 and argv[1] == 'find':
        return find(arc.Disc(argv[2]), argv[3])
    if len(argv) == 5 and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3], argv[4])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        sys.exit(1)
