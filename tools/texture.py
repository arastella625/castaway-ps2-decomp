#!/usr/bin/env python3
"""Decode the game's TXFL textures from your own disc image into PNG files.

Usage:
  python -I tools/texture.py check  <game.iso>
  python -I tools/texture.py export <game.iso> <out_dir> [name_substring]

check decodes every texture and reports anything that doesn't fit the format. The PNGs
that export writes are game data: keep them out of the repo. docs/ASSET_FORMATS.md has
the format.
"""
import collections
import os
import struct
import sys
import zlib
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402

TEXTURE_ARCHIVE = 'TEXTURES'
TEXTURE_VERSION = 9
TEXTURE_TAG = b'LFXT'  # "TXFL", stored as a little-endian integer
PREFIX_SIZE = 16
FIELDS_SIZE = 36  # everything from the end of the name to the first pixel
KIND_PALETTED = 0
KIND_DIRECT = 1
PS2_ALPHA_ONE = 0x80  # the GS treats 0x80 as fully opaque


@dataclass(frozen=True)
class Texture:
    name: str
    width: int
    height: int
    flags: int
    kind: int
    bits: int
    palette_size: int
    mip_hint: int
    pixels: bytes
    palette: bytes


def parse_texture(data, name='texture'):
    """Split one TXFL file into its fields, checking every size against the data."""
    if len(data) < PREFIX_SIZE:
        raise arc.FormatError('%s is too short for a header' % name)
    version, tag, _, name_size = struct.unpack_from('<I4siI', data, 0)
    if version != TEXTURE_VERSION or tag != TEXTURE_TAG:
        raise arc.FormatError('%s is not a version 9 TXFL file' % name)
    body = PREFIX_SIZE + name_size
    if body + FIELDS_SIZE > len(data):
        raise arc.FormatError('%s has a truncated header' % name)
    size = struct.unpack_from('<I', data, body)[0]
    if size != len(data) - body - 4:
        raise arc.FormatError('%s has a size field that does not match the file' % name)
    flags = struct.unpack_from('<I', data, body + 12)[0]
    width, height, palette_size, mip_hint = struct.unpack_from('<HHHH', data, body + 20)
    kind, bits = data[body + 28], data[body + 30]
    pixel_bytes = width * height * bits // 8
    start = body + FIELDS_SIZE
    if start + pixel_bytes + palette_size * 4 != len(data):
        raise arc.FormatError('%s: pixels and palette do not fill the file' % name)
    split = start + pixel_bytes
    return Texture(name, width, height, flags, kind, bits, palette_size, mip_hint,
                   data[start:split], data[split:])


def expand_alpha(value):
    """Map the GS range, where 0x80 is opaque, onto the usual 0-255."""
    return min(255, value * 2)


def palette_rgba(texture):
    raw = texture.palette
    return [(raw[i], raw[i + 1], raw[i + 2], expand_alpha(raw[i + 3]))
            for i in range(0, len(raw), 4)]


def indices(texture):
    if texture.bits == 8:
        return texture.pixels
    if texture.bits == 4:  # the low nibble is the left pixel
        out = bytearray(len(texture.pixels) * 2)
        out[0::2] = bytes(b & 0xF for b in texture.pixels)
        out[1::2] = bytes(b >> 4 for b in texture.pixels)
        return out
    raise arc.FormatError('%s: paletted textures of %d bits are unknown'
                          % (texture.name, texture.bits))


def direct_rgba(texture):
    raw = texture.pixels
    out = bytearray(texture.width * texture.height * 4)
    if texture.bits == 32:
        out[:] = raw
        out[3::4] = bytes(expand_alpha(a) for a in raw[3::4])
    elif texture.bits == 16:  # 5 bits each of red, green, blue, then one of alpha
        for i, (value,) in enumerate(struct.iter_unpack('<H', raw)):
            out[i * 4:i * 4 + 4] = bytes(((value & 31) * 255 // 31,
                                          (value >> 5 & 31) * 255 // 31,
                                          (value >> 10 & 31) * 255 // 31,
                                          255 if value >> 15 else 0))
    else:
        raise arc.FormatError('%s: direct textures of %d bits are unknown'
                              % (texture.name, texture.bits))
    return out


def stored_rgba(texture):
    """Return RGBA bytes in file order, which is bottom row first."""
    if texture.kind == KIND_DIRECT:
        return direct_rgba(texture)
    if texture.kind != KIND_PALETTED:
        raise arc.FormatError('%s has unknown kind %d' % (texture.name, texture.kind))
    palette = [bytes(entry) for entry in palette_rgba(texture)]
    pixels = indices(texture)
    if pixels and max(pixels) >= len(palette):
        raise arc.FormatError('%s indexes past its palette' % texture.name)
    return b''.join(palette[i] for i in pixels)


def rgba(texture):
    """Return the texture as RGBA bytes with the top row first, as PNG wants."""
    stored = stored_rgba(texture)
    stride = texture.width * 4
    return b''.join(stored[y * stride:(y + 1) * stride]
                    for y in reversed(range(texture.height)))


def png_bytes(width, height, pixels):
    def chunk(kind, body):
        return (struct.pack('>I', len(body)) + kind + body
                + struct.pack('>I', zlib.crc32(kind + body)))

    stride = width * 4
    rows = b''.join(b'\0' + bytes(pixels[y * stride:(y + 1) * stride]) for y in range(height))
    header = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header)
            + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))


def each_texture(disc):
    archive = arc.find_archive(disc, TEXTURE_ARCHIVE)
    for entry in sorted(arc.read_toc(disc, archive), key=lambda e: e.offset):
        yield entry, disc.read(archive.lba, entry.offset, entry.size)


def check(disc):
    shapes = collections.Counter()
    alpha_peaks = collections.Counter()
    failures = []
    for entry, data in each_texture(disc):
        try:
            texture = parse_texture(data, entry.name)
            rgba(texture)
        except arc.FormatError as error:
            failures.append(str(error))
            continue
        shapes[(texture.kind, texture.bits, texture.palette_size)] += 1
        source = texture.palette or texture.pixels
        if texture.bits >= 32 or texture.palette:
            alpha_peaks[max(source[3::4], default=0)] += 1
    for (kind, bits, palette_size), count in sorted(shapes.items()):
        print('kind %d, %2d bits, %3d palette entries: %6d textures'
              % (kind, bits, palette_size, count))
    print('peak raw alpha, most common:', [(hex(a), n) for a, n in alpha_peaks.most_common(6)])
    print('peak raw alpha above 0x80:', sum(n for a, n in alpha_peaks.items() if a > PS2_ALPHA_ONE))
    print('failures: %d' % len(failures))
    for failure in failures[:20]:
        print('   ', failure)
    return 1 if failures else 0


def export(disc, out_dir, wanted):
    os.makedirs(out_dir, exist_ok=True)
    written = 0
    for entry, data in each_texture(disc):
        if wanted and wanted not in entry.name:
            continue
        texture = parse_texture(data, entry.name)
        target = '%s.%08x.png' % (arc.safe_file_name(entry.name), entry.name_hash)
        with open(os.path.join(out_dir, target), 'wb') as out:
            out.write(png_bytes(texture.width, texture.height, rgba(texture)))
        written += 1
    print('wrote %d textures to %s' % (written, out_dir))
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) in (4, 5) and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3], argv[4] if len(argv) == 5 else None)
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
