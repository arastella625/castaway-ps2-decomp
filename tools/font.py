#!/usr/bin/env python3
"""Decode the game's bitmap fonts (FONTS.ARC) from your own disc image.

Usage:
  python -I tools/font.py check  <game.iso>
  python -I tools/font.py export <game.iso> <out_dir> [sample text]

check parses both fonts to their last byte, then tests every glyph against its texture
page. export renders a sample line with each font into <out_dir>/<font>.sample.png, using
the game's own advance rule. The PNGs are game data: keep them out of the repo.
docs/ASSET_FORMATS.md has the format.
"""
import os
import struct
import sys
from dataclasses import dataclass, field

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import texture  # noqa: E402

FONT_ARCHIVE = 'FONTS'
FONT_VERSION = 10
FONT_TAG = b'TNOF'  # "FONT", stored as a little-endian integer
PREFIX_SIZE = 16
TYPE_SIZE, TYPE_PAGE, TYPE_CHARACTER = 0xA81AD9ED, 0x4BD04BA7, 0x598DE3B1
TYPE_UNUSED = 0xF76B628F  # listed first with no objects; which class it names is open
OBJECT_END = 0xB7  # one byte after each object; the loader reads it and ignores it
MAX_OBJECTS = 0x10000
EMPTY_ALPHA = 2  # both pages fill blank space with a palette entry of this alpha, not 0
GLYPH_OVERLAP = 2  # the pen moves back 1 before and 1 after each glyph (ERFont::DoDraw)
GLYPH_LEAD = 1
SAMPLE_TEXT = 'The quick brown fox jumps over the lazy dog. 0123456789 (Sims!)'
BACKGROUND = (32, 40, 56, 255)
MARGIN = 8


class Cursor:
    """Reads little-endian fields, refusing to step past the end of the data."""

    def __init__(self, data, pos=0):
        self.data = data
        self.pos = pos

    def take(self, layout):
        size = struct.calcsize(layout)
        if self.pos + size > len(self.data):
            raise arc.FormatError('font data ends early at %#x' % self.pos)
        values = struct.unpack_from(layout, self.data, self.pos)
        self.pos += size
        return values[0] if len(values) == 1 else values


@dataclass
class Character:
    left: int
    right: int
    row: int
    page: int


@dataclass
class Page:
    texture_hash: int
    width: int
    height: int


@dataclass
class Size:
    point_size: int
    row_height: int
    snap: int
    character_ids: dict = field(default_factory=dict)
    page_ids: list = field(default_factory=list)
    characters: dict = field(default_factory=dict)  # code unit to Character
    pages: list = field(default_factory=list)


@dataclass
class Font:
    name: str
    size: Size
    ascent: int
    descent: int
    em: int
    tracking: int
    line_spacing: int
    space: int
    kerning: dict


def read_table(cursor):
    """Read a saved hash table: a capacity, a count, then key and value pairs."""
    capacity, count = cursor.take('<II')
    if count > MAX_OBJECTS:
        raise arc.FormatError('hash table of %d entries' % count)
    pairs = {}
    for _ in range(count):
        key, value = cursor.take('<Ii')
        pairs[key] = value
    return capacity, pairs


def read_size(cursor):
    size = Size(*cursor.take('<iii'))
    _, size.character_ids = read_table(cursor)
    count = cursor.take('<I')
    if count > MAX_OBJECTS:
        raise arc.FormatError('font lists %d pages' % count)
    size.page_ids = [cursor.take('<I') for _ in range(count)]
    return size


def read_page(cursor):
    return Page(*cursor.take('<Iii'))


def read_character(cursor):
    return Character(*cursor.take('<iiHH'))


OBJECT_READERS = {TYPE_SIZE: read_size, TYPE_PAGE: read_page, TYPE_CHARACTER: read_character}


def read_object_types(cursor, total):
    """Read the type table that EFile::ReadStructure walks until it has `total` objects."""
    kinds = []
    while len(kinds) < total:
        type_hash, _, count = cursor.take('<IHI')
        if count > total - len(kinds):
            raise arc.FormatError('type table lists more than %d objects' % total)
        if count and type_hash not in OBJECT_READERS:
            raise arc.FormatError('objects of unknown type %#x' % type_hash)
        kinds += [type_hash] * count
    return kinds


def read_objects(cursor):
    total = cursor.take('<I')
    if not 0 < total <= MAX_OBJECTS:
        raise arc.FormatError('font holds %d objects' % total)
    objects = []
    for kind in read_object_types(cursor, total):
        objects.append(OBJECT_READERS[kind](cursor))
        if cursor.take('<B') != OBJECT_END:
            raise arc.FormatError('no object end mark at %#x' % (cursor.pos - 1))
    root = cursor.take('<I')
    if root >= total or not isinstance(objects[root], Size):
        raise arc.FormatError('font root %d is not a size' % root)
    return objects, objects[root]


def object_by_id(objects, number, kind):
    """Saved pointers count objects from 1; 0 would be a null pointer."""
    if not 0 < number <= len(objects) or not isinstance(objects[number - 1], kind):
        raise arc.FormatError('pointer %d is not a %s' % (number, kind.__name__))
    return objects[number - 1]


def link_size(size, objects):
    size.characters = {code: object_by_id(objects, number, Character)
                       for code, number in size.character_ids.items()}
    size.pages = [object_by_id(objects, number, Page) for number in size.page_ids]


def parse_font(data, name='font'):
    """Parse one FONT file; it must end on its exact last byte."""
    cursor = Cursor(data)
    version, tag, _, name_size = cursor.take('<I4siI')
    if version != FONT_VERSION or tag != FONT_TAG:
        raise arc.FormatError('%s is not a version 10 FONT file' % name)
    cursor.pos += name_size
    if cursor.take('<I') != len(data) - cursor.pos:
        raise arc.FormatError('%s has a size field that does not match the file' % name)
    cursor.take('<HH')  # saved class versions of EFontData and its base
    if cursor.take('<I') != 1:
        raise arc.FormatError('%s has several sizes; only one is expected' % name)
    objects, size = read_objects(cursor)
    link_size(size, objects)
    ascent, descent, em, tracking, line_spacing, space = cursor.take('<6i')
    _, kerning = read_table(cursor)
    if cursor.pos != len(data):
        raise arc.FormatError('%s has %d bytes left over' % (name, len(data) - cursor.pos))
    return Font(name, size, ascent, descent, em, tracking, line_spacing, space, kerning)


def each_font(disc):
    archive = arc.find_archive(disc, FONT_ARCHIVE)
    for entry in sorted(arc.read_toc(disc, archive), key=lambda e: e.name):
        yield entry, disc.read(archive.lba, entry.offset, entry.size)


def load_pages(disc, font):
    """Return each page's decoded texture, found by the name hash the page stores."""
    archive = arc.find_archive(disc, texture.TEXTURE_ARCHIVE)
    entries = {e.name_hash: e for e in arc.read_toc(disc, archive)}
    pages = []
    for page in font.size.pages:
        entry = entries.get(page.texture_hash)
        if entry is None:
            raise arc.FormatError('%s: no texture has hash %#x' % (font.name, page.texture_hash))
        parsed = texture.parse_texture(disc.read(archive.lba, entry.offset, entry.size),
                                       entry.name)
        if (parsed.width, parsed.height) != (page.width, page.height):
            raise arc.FormatError('%s: page and texture sizes differ' % entry.name)
        pages.append(parsed)
    return pages


def glyph_rect(font, character, page_height):
    """Return (left, top, right, bottom) in a top-down image, right and bottom exclusive.

    Rows count from the first row as stored, which is the bottom of the image.
    """
    bottom = page_height - character.row * font.size.row_height
    return character.left, bottom - font.size.row_height, character.right, bottom


def inked(pixels, width, rect):
    left, top, right, bottom = rect
    return any(pixels[(y * width + x) * 4 + 3] > EMPTY_ALPHA
               for y in range(top, bottom) for x in range(left, right))


def audit(font, pages):
    """Test glyph rectangles against the pages; return a list of problems."""
    problems = []
    images = [(texture.rgba(page), page.width, page.height) for page in pages]
    covered = [bytearray(width * height) for _, width, height in images]
    for code, character in sorted(font.size.characters.items()):
        if character.page >= len(images):
            problems.append('U+%04X is on missing page %d' % (code, character.page))
            continue
        pixels, width, height = images[character.page]
        rect = glyph_rect(font, character, height)
        if not (0 <= rect[0] < rect[2] <= width and rect[1] >= 0):
            problems.append('U+%04X falls outside its page' % code)
            continue
        if not inked(pixels, width, rect):
            problems.append('U+%04X has no ink' % code)
        for y in range(rect[1], rect[3]):
            for x in range(rect[0], rect[2]):
                if covered[character.page][y * width + x]:
                    problems.append('U+%04X overlaps another glyph' % code)
                covered[character.page][y * width + x] = 1
    for index, (pixels, width, height) in enumerate(images):
        stray = sum(1 for i in range(width * height)
                    if pixels[i * 4 + 3] > EMPTY_ALPHA and not covered[index][i])
        if stray:
            problems.append('page %d has %d inked pixels outside every glyph' % (index, stray))
    return problems


def code_ranges(codes):
    """Summarise sorted code units as runs, for the report."""
    runs = []
    for code in codes:
        if runs and runs[-1][1] + 1 == code:
            runs[-1][1] = code
        else:
            runs.append([code, code])
    return ' '.join('%04X-%04X' % (a, b) if a != b else '%04X' % a for a, b in runs)


def check(disc):
    failures = []
    for entry, data in each_font(disc):
        try:
            font = parse_font(data, entry.name)
            problems = audit(font, load_pages(disc, font))
        except arc.FormatError as error:
            failures.append(str(error))
            continue
        size = font.size
        print('%-11s %5d bytes  size %d, row height %d, snap %d, %d glyphs, %d page(s) %s'
              % (entry.name, entry.size, size.point_size, size.row_height, size.snap,
                 len(size.characters), len(size.pages),
                 ['%dx%d' % (p.width, p.height) for p in size.pages]))
        print('    ascent %d descent %d em %d tracking %d line %d space %d kerning pairs %d'
              % (font.ascent, font.descent, font.em, font.tracking, font.line_spacing,
                 font.space, len(font.kerning)))
        print('    codes:', code_ranges(sorted(size.characters)))
        failures += ['%s: %s' % (entry.name, problem) for problem in problems]
    print('failures: %d' % len(failures))
    for failure in failures[:20]:
        print('   ', failure)
    return 1 if failures else 0


def round_away(value):
    return int(value + 0.5) if value >= 0 else int(value - 0.5)


def layout(font, text):
    """Place text at the font's own size; return ([(character or None, pen x)], width).

    This follows ERFont::DoDraw at scale 1. A code with no glyph, such as the space, moves
    the pen by `space`. Every pair then adds its kerning, or `tracking` if it has none.
    """
    unit = font.size.point_size / font.em  # em units to pixels
    pen = float(GLYPH_LEAD)
    placed = []
    codes = [ord(c) for c in text]
    for index, code in enumerate(codes):
        character = font.size.characters.get(code)
        if character:
            placed.append((character, pen - GLYPH_LEAD))
            pen += character.right - character.left - GLYPH_OVERLAP
        else:
            pen += font.space * unit
        if index + 1 < len(codes):
            pair = font.kerning.get(codes[index + 1] | code << 16, font.tracking)
            pen += (round_away(pair) if font.size.snap == 1 else pair) * unit
    return placed, int(pen) + GLYPH_LEAD + 1


def blend(canvas, offset, source, alpha):
    keep = 255 - alpha
    for channel in range(3):
        kept = canvas[offset + channel] * keep
        canvas[offset + channel] = (source[channel] * alpha + kept) // 255


def render(font, pages, text):
    """Return (width, height, RGBA bytes) of one line of text over a plain background."""
    placed, line_width = layout(font, text)
    width = line_width + 2 * MARGIN
    height = font.size.row_height + 2 * MARGIN
    canvas = bytearray(bytes(BACKGROUND) * (width * height))
    images = [texture.rgba(page) for page in pages]
    for character, pen in placed:
        left, top, right, bottom = glyph_rect(font, character, pages[character.page].height)
        stride = pages[character.page].width
        image = images[character.page]
        origin = MARGIN + round_away(pen)
        for y in range(top, bottom):
            for x in range(left, right):
                source = (y * stride + x) * 4
                target_x = origin + x - left
                if image[source + 3] and 0 <= target_x < width:
                    offset = ((MARGIN + y - top) * width + target_x) * 4
                    blend(canvas, offset, image[source:source + 3], image[source + 3])
    return width, height, canvas


def export(disc, out_dir, text):
    os.makedirs(out_dir, exist_ok=True)
    for entry, data in each_font(disc):
        font = parse_font(data, entry.name)
        width, height, canvas = render(font, load_pages(disc, font), text)
        target = '%s.sample.png' % arc.safe_file_name(entry.name)
        with open(os.path.join(out_dir, target), 'wb') as out:
            out.write(texture.png_bytes(width, height, canvas))
    print('wrote samples to %s' % out_dir)
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) in (4, 5) and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3], argv[4] if len(argv) == 5 else SAMPLE_TEXT)
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
