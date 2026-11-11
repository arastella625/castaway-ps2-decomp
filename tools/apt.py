#!/usr/bin/env python3
"""Read the game's Flash-style UI movies (EA Apt) from FLASHES.ARC.

Usage:
  python -I tools/apt.py check  <game.iso>
  python -I tools/apt.py find   <game.iso> <script name>
  python -I tools/apt.py export <game.iso> <movie> <out_dir>
  python -I tools/apt.py layout <game.iso> <movie> <out_dir> [label]

Each FLASHES entry is an HSLF wrapper round a BIGF archive of <movie>.apt (characters and
timelines), <movie>.const (the constants its scripts use), <movie>.geo (shape triangles and their
texture regions) and 001.tgq on (8-bit texture sheets). check reads all 147 and tests every shape
and placement a layout rests on. find names the movies whose constants hold a script name: the
game calls into a movie by name, such as ActionMenu's imuSetBackdrop. export writes a movie's
sheets as PNG and its header, exports, fonts and strings as JSON. layout writes each export as a
node tree (JSON) and a picture (PNG); with a label such as refresh, an export is drawn at the
frame before it. All of it is game data: keep it out of the repo. docs/formats/media.md has the
format.
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
MENU = 'tsc5_interaction_menu'  # ActionMenu's movie: the check's controls break it
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


GEO_TAG = b'Apt PCGL geometry file\x1a\0'
GEO_TEXTURED, GEO_SOLID, GEO_LINES = 5, 1, 6
GEO_POINTS = {GEO_TEXTURED: 3, GEO_SOLID: 3, GEO_LINES: 2}  # per triangle, or per line
SHAPE, TEXT, SPRITE = 1, 2, 5
ACTION, LABEL, PLACE, REMOVE = 1, 2, 3, 4
# PlaceObject flags, the same bits as SWF's PlaceObject2
MOVE, HAS_CHARACTER, HAS_MATRIX, HAS_COLOUR, HAS_RATIO, HAS_NAME, HAS_CLIP_DEPTH = 1, 2, 4, 8, 0x10, 0x20, 0x40
NO_CLIP = 0xFFFFFFFF


def floats(data, pos, count):
    if not 0 <= pos <= len(data) - 4 * count:
        raise arc.FormatError('Apt: a read past the end at %#x' % pos)
    return struct.unpack_from('<%df' % count, data, pos)


def geometry(geo, name):
    """The .geo: shape id -> draw batches; bitmap id -> sheet number.

    After the tag: shape count and table, bitmap count and table. A shape row is id, batch count,
    pointer to batch pointers. A batch is kind, count, vertex pointer (x, y floats), ARGB colour.
    Kind 1 is solid triangles, 6 line segments (an outline: two points each), 5 textured triangles,
    which add a 4x4 matrix from shape space to bitmap pixels (GL order), the bitmap id, a zero word
    and the bitmap's UV rectangle on its sheet, half a texel in from its edges (bitmap_rect).
    """
    if not geo.startswith(GEO_TAG):
        raise arc.FormatError('%s: not an Apt geometry file' % name)
    shapes, shape_at, bitmaps, bitmap_at = (u32(geo, 24 + 4 * i) for i in range(4))
    sheets = {u32(geo, bitmap_at + 8 * i): u32(geo, bitmap_at + 8 * i + 4) for i in range(bitmaps)}
    out = {}
    for row in range(shapes):
        shape, count, at = (u32(geo, shape_at + 12 * row + 4 * i) for i in range(3))
        batches = []
        for index in range(count):
            record = u32(geo, at + 4 * index)
            kind, number, vertices, colour = (u32(geo, record + 4 * i) for i in range(4))
            if kind not in GEO_POINTS:
                raise arc.FormatError('%s: shape %d has a batch of kind %d' % (name, shape, kind))
            points = floats(geo, vertices, 2 * GEO_POINTS[kind] * number)
            batch = {'kind': 'lines' if kind == GEO_LINES else 'triangles', 'colour': colour,
                     'points': [points[i:i + 2] for i in range(0, len(points), 2)]}
            if kind == GEO_TEXTURED:
                m = floats(geo, record + 16, 16)
                if u32(geo, record + 84):
                    raise arc.FormatError('%s: shape %d has a nonzero word after its bitmap' % (name, shape))
                batch.update(bitmap=u32(geo, record + 80), matrix=(m[0], m[1], m[4], m[5], m[12], m[13]),
                             uv=floats(geo, record + 88, 4))
            batches.append(batch)
        out[shape] = batches
    return out, sheets


def bitmap_rect(batch, width, height):
    """Where a textured batch's bitmap sits on its sheet, in pixels: x, y, width, height."""
    u0, v0, u1, v1 = batch['uv']
    return u0 * width - 0.5, v0 * height - 0.5, (u1 - u0) * width + 1, (v1 - v0) * height + 1


def fits(batch, width, height, slack=0.05):
    """True if the batch's matrix maps its points onto exactly its bitmap, 0 to its full size."""
    a, b, c, d, tx, ty = batch['matrix']
    _, _, w, h = bitmap_rect(batch, width, height)
    xs = [a * x + c * y + tx for x, y in batch['points']]
    ys = [b * x + d * y + ty for x, y in batch['points']]
    return (abs(min(xs)) < slack and abs(min(ys)) < slack
            and abs(max(xs) - w) < slack and abs(max(ys) - h) < slack)


def frame_items(apt, frames, at):
    """A timeline: per frame, its items. PlaceObject is 64 bytes: type, flags, depth, character,
    matrix a b c d, tx, ty, colour multiply and add (ARGB), ratio, name pointer, clip depth, actions."""
    out = []
    for frame in range(frames):
        count, items_at = u32(apt, at + 8 * frame), u32(apt, at + 8 * frame + 4)
        items = []
        for index in range(count):
            item = u32(apt, items_at + 4 * index)
            kind = u32(apt, item)
            if kind == PLACE:
                flags, depth, character = (u32(apt, item + 4 * i) for i in (1, 2, 3))
                name_at = u32(apt, item + 52)
                items.append({'type': 'place', 'flags': flags, 'depth': depth,
                              'character': character if flags & HAS_CHARACTER else None,
                              'matrix': floats(apt, item + 16, 6),
                              'multiply': u32(apt, item + 40), 'add': u32(apt, item + 44),
                              'name': cstring(apt, name_at) if name_at else None,
                              'clip_depth': u32(apt, item + 56)})
            elif kind == REMOVE:
                items.append({'type': 'remove', 'depth': u32(apt, item + 4)})
            elif kind == LABEL:
                items.append({'type': 'label', 'name': cstring(apt, u32(apt, item + 4))})
            else:
                items.append({'type': 'action' if kind == ACTION else 'type %d' % kind})
        out.append(items)
    return out


def characters(apt, movie):
    """Characters by id, with what a layout needs: shapes' bounds, texts' fields, sprites' frames."""
    out = {}
    count, character_at = u32(apt, movie + 20), u32(apt, movie + 24)
    for index in range(count):
        at = u32(apt, character_at + index * 4)
        if not at:
            continue
        kind = u32(apt, at)
        entry = {'kind': CHARACTER_NAMES.get(kind, 'type %d' % kind)}
        if kind == SHAPE:
            entry.update(bounds=floats(apt, at + 8, 4), geometry=u32(apt, at + 24))
        elif kind == TEXT:
            text_at, variable_at = u32(apt, at + 52), u32(apt, at + 56)
            entry.update(bounds=floats(apt, at + 8, 4), font=u32(apt, at + 24), align=u32(apt, at + 28),
                         colour=u32(apt, at + 32), size=floats(apt, at + 36, 1)[0],
                         text=cstring(apt, text_at) if text_at else '',
                         variable=cstring(apt, variable_at) if variable_at else '')
        elif kind in (SPRITE, MOVIE):
            entry['frames'] = frame_items(apt, u32(apt, at + 8), u32(apt, at + 12))
        out[index] = entry
    return out


def display_list(frames, frame):
    """What sits at each depth once the timeline reaches frame (counted from 0)."""
    placed = {}
    for items in frames[:frame + 1]:
        for item in items:
            if item['type'] == 'remove':
                placed.pop(item['depth'], None)
            elif item['type'] == 'place':
                old = placed.get(item['depth']) if item['flags'] & MOVE else None
                new = dict(old or {'character': None, 'matrix': (1, 0, 0, 1, 0, 0), 'multiply': NO_CLIP,
                                   'add': 0, 'name': None, 'clip_depth': NO_CLIP})
                for key, bit in (('character', HAS_CHARACTER), ('matrix', HAS_MATRIX),
                                 ('name', HAS_NAME), ('clip_depth', HAS_CLIP_DEPTH)):
                    if item['flags'] & bit:
                        new[key] = item[key]
                if item['flags'] & HAS_COLOUR:
                    new['multiply'], new['add'] = item['multiply'], item['add']
                placed[item['depth']] = new
    return placed


def combine(outer, inner):
    """Matrices are (a, b, c, d, tx, ty): x' = a x + c y + tx, y' = b x + d y + ty."""
    a, b, c, d, tx, ty = outer
    a2, b2, c2, d2, tx2, ty2 = inner
    return (a * a2 + c * b2, b * a2 + d * b2, a * c2 + c * d2, b * c2 + d * d2,
            a * tx2 + c * ty2 + tx, b * tx2 + d * ty2 + ty)


def node(chars, shapes, character, frame=0, depth_path=()):
    """A character as a tree the port can build a scene from: sprites hold children by depth."""
    entry = chars.get(character)
    if entry is None:
        raise arc.FormatError('no character %d' % character)
    out = {'character': character, 'kind': entry['kind']}
    if entry['kind'] == 'shape':
        out['bounds'] = list(entry['bounds'])
        out['batches'] = shapes.get(entry['geometry'], [])
    elif entry['kind'] == 'text':
        out.update({k: entry[k] for k in ('bounds', 'font', 'align', 'colour', 'size', 'text', 'variable')})
    elif entry['kind'] in ('sprite', 'movie'):
        out['frames'] = len(entry['frames'])
        out['labels'] = {item['name']: index for index, items in enumerate(entry['frames'])
                         for item in items if item['type'] == 'label'}
        out['children'] = []
        for depth, placed in sorted(display_list(entry['frames'], frame).items()):
            if placed['character'] is None:
                continue
            if depth_path.count(placed['character']) or len(depth_path) > 32:
                raise arc.FormatError('character %d places itself' % character)
            child = node(chars, shapes, placed['character'], 0, depth_path + (character,))
            child.update(depth=depth, name=placed['name'], matrix=list(placed['matrix']),
                         multiply='%08x' % placed['multiply'], add='%08x' % placed['add'])
            if placed['clip_depth'] != NO_CLIP:
                child['clip_depth'] = placed['clip_depth']
            out['children'].append(child)
    return out


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
    return info, sheets, parts, movie


def read_layout(name, parts, movie, sheets):
    """Characters and geometry, each textured batch given its sheet member and pixel region."""
    chars = characters(parts[name + '.apt'], movie)
    shapes, bitmap_sheets = geometry(parts[name + '.geo'], name)
    for batches in shapes.values():
        for batch in batches:
            if 'bitmap' not in batch:
                continue
            member = '%03d.tgq' % bitmap_sheets.get(batch['bitmap'], -1)
            if member not in sheets:
                raise arc.FormatError('%s: bitmap %d has no sheet' % (name, batch['bitmap']))
            batch['sheet'] = member
            batch['region'] = bitmap_rect(batch, sheets[member].width, sheets[member].height)
    return chars, shapes


def place_problems(item, chars):
    """A PlaceObject's fields must agree with its flags."""
    flags, problems = item['flags'], []
    if flags & HAS_CHARACTER and item['character'] not in chars:
        problems.append('places missing character %d' % item['character'])
    if not flags & (HAS_CHARACTER | MOVE):
        problems.append('neither places nor moves')
    if not flags & HAS_MATRIX and tuple(item['matrix']) != (1, 0, 0, 1, 0, 0):
        problems.append('a matrix without its flag')
    if not flags & HAS_COLOUR and (item['multiply'], item['add']) != (NO_CLIP, 0):
        problems.append('a colour transform without its flag')
    if bool(flags & HAS_NAME) != bool(item['name']):
        problems.append('a name and its flag disagree')
    if not flags & HAS_CLIP_DEPTH and item['clip_depth'] != NO_CLIP:
        problems.append('a clip depth without its flag')
    return problems


def channels(argb):
    return [(argb >> shift & 0xFF) / 255 for shift in (16, 8, 0, 24)]  # r, g, b, a


def flatten(tree, matrix=(1, 0, 0, 1, 0, 0), multiply=(1, 1, 1, 1), add=(0, 0, 0, 0)):
    """Yield (shape or text node, matrix to the root, colour multiply, colour add), back to front.
    Mask layers (clip depth) are left out, not applied."""
    if 'matrix' in tree:
        matrix = combine(matrix, tree['matrix'])
        inner_m = channels(int(tree['multiply'], 16))
        inner_a = [v * 255 for v in channels(int(tree['add'], 16))]
        multiply, add = ([m * o for m, o in zip(inner_m, multiply)],
                         [a * o + p for a, o, p in zip(inner_a, multiply, add)])
    if 'clip_depth' in tree:
        return
    if tree['kind'] in ('shape', 'text'):
        yield tree, matrix, multiply, add
    for child in tree.get('children', []):
        yield from flatten(child, matrix, multiply, add)


def corners(item):
    if item['kind'] == 'text':
        left, top, right, bottom = item['bounds']
        return [(left, top), (right, top), (right, bottom), (left, bottom)]
    return [point for batch in item['batches'] for point in batch['points']]


def apply(m, x, y):
    return m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]


def render(tree, sheet_pixels, background=0xFF666666, scale=1.0, margin=4):
    """Draw a node tree to RGBA with nearest texels. A text is drawn as its box only (no font).
    sheet_pixels maps a sheet member to (width, RGBA bytes). Returns (width, height, pixels)."""
    draws = list(flatten(tree))
    points = [apply(m, x, y) for item, m, _, _ in draws for x, y in corners(item)] or [(0, 0)]
    left, top = min(p[0] for p in points), min(p[1] for p in points)
    width = int((max(p[0] for p in points) - left) * scale) + 2 * margin + 1
    height = int((max(p[1] for p in points) - top) * scale) + 2 * margin + 1
    pixels = bytearray(bytes(int(v * 255) for v in channels(background)) * (width * height))
    view = (scale, 0, 0, scale, margin - left * scale, margin - top * scale)

    def blend(x, y, rgba, multiply, add):
        if not (0 <= x < width and 0 <= y < height):
            return
        src = [min(1.0, max(0.0, c * m + a / 255)) for c, m, a in zip(rgba, multiply, add)]
        at = (y * width + x) * 4
        for i in range(3):
            pixels[at + i] = int(src[i] * 255 * src[3] + pixels[at + i] * (1 - src[3]))

    for item, matrix, multiply, add in draws:
        m = combine(view, matrix)
        if item['kind'] == 'text':
            box = [apply(m, x, y) for x, y in corners(item)]
            colour = channels(item['colour'])
            colour[3] *= 0.6
            batches = [('lines', list(zip(box, box[1:] + box[:1])), colour, None)]
        else:
            batches = []
            for batch in item['batches']:
                world = [apply(m, x, y) for x, y in batch['points']]
                step = 2 if batch['kind'] == 'lines' else 3
                batches.append((batch['kind'], [world[i:i + step] for i in range(0, len(world), step)],
                                channels(batch['colour']), batch))
        det = m[0] * m[3] - m[1] * m[2]
        for kind, parts, colour, batch in batches:
            if kind == 'lines':
                for (x0, y0), (x1, y1) in parts:
                    steps = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
                    for i in range(steps + 1):
                        t = i / steps
                        blend(int(x0 + (x1 - x0) * t), int(y0 + (y1 - y0) * t), colour, multiply, add)
                continue
            for (x0, y0), (x1, y1), (x2, y2) in parts:
                area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
                if not area or not det:
                    continue
                for y in range(max(0, int(min(y0, y1, y2))), min(height, int(max(y0, y1, y2)) + 1)):
                    for x in range(max(0, int(min(x0, x1, x2))), min(width, int(max(x0, x1, x2)) + 1)):
                        px, py = x + 0.5, y + 0.5
                        w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
                        w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
                        if w0 < 0 or w1 < 0 or w0 + w1 > 1:
                            continue
                        rgba = colour
                        if batch and 'bitmap' in batch:
                            sx = (m[3] * (px - m[4]) - m[2] * (py - m[5])) / det  # back to shape space
                            sy = (m[0] * (py - m[5]) - m[1] * (px - m[4])) / det
                            bx, by = apply(batch['matrix'], sx, sy)
                            rx, ry, rw, rh = batch['region']
                            bx, by = min(max(bx, 0), rw - 1), min(max(by, 0), rh - 1)
                            sheet_width, texels = sheet_pixels[batch['sheet']]
                            at = (int(ry + by) * sheet_width + int(rx + bx)) * 4
                            rgba = [v / 255 * k for v, k in zip(texels[at:at + 4], colour)]
                        blend(x, y, rgba, multiply, add)
    return width, height, pixels


def entries(disc):
    return sorted(arc.read_toc(disc, arc.find_archive(disc, ARCHIVE)), key=lambda e: e.name)


def layout_problems(name, parts, movie, sheets, totals):
    """Everything a layout rests on, for one movie: a list of what does not hold."""
    problems = []
    chars, shapes = read_layout(name, parts, movie, sheets)
    for character, entry in sorted(chars.items()):
        if entry['kind'] == 'shape':
            totals['shapes with geometry'] += entry['geometry'] in shapes
            left, top, right, bottom = entry['bounds']
            for batch in shapes.get(entry['geometry'], []):
                totals['%s batches' % ('textured' if 'bitmap' in batch else batch['kind'])] += 1
                if any(not (left - 0.01 <= x <= right + 0.01 and top - 0.01 <= y <= bottom + 0.01)
                       for x, y in batch['points']):
                    problems.append('shape %d has a point outside its bounds' % character)
                if 'bitmap' in batch:
                    image = sheets[batch['sheet']]
                    if not fits(batch, image.width, image.height):
                        problems.append('shape %d does not fill its bitmap' % character)
        for items in entry.get('frames', []):
            for item in items:
                if item['type'] == 'place':
                    totals['placements'] += 1
                    problems += ['character %d: %s' % (character, p) for p in place_problems(item, chars)]
    for export_name, character in movie_info(parts[name + '.apt'], movie, name)['exports']:
        try:
            node(chars, shapes, character)
            totals['exports laid out'] += 1
        except arc.FormatError as error:
            problems.append('%s: %s' % (export_name, error))
    return problems


def check(disc):
    totals, failures = collections.Counter(), []
    menu = None
    for entry in entries(disc):
        try:
            info, sheets, parts, movie = read_movie(disc, entry)
            totals['movies'] += 1
            totals['sheets'] += len(sheets)
            totals['exports'] += len(info['exports'])
            totals.update(info['characters'])
            failures += ['%s: %s' % (entry.name, p) for p in layout_problems(entry.name, parts, movie, sheets, totals)]
            if entry.name == MENU:
                menu = entry.name, parts, movie, sheets
        except (arc.FormatError, ValueError, KeyError, struct.error) as error:
            failures.append('%s: %s' % (entry.name, error))
    for key, value in sorted(totals.items()):
        print('%-22s %6d' % (key, value))
    archive = arc.find_archive(disc, ARCHIVE)
    first = entries(disc)[0]
    data = disc.read(archive.lba, first.offset, first.size)
    print('controls:')
    passed = not failures and menu is not None

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
    if menu:
        name, parts, movie, sheets = menu

        def caught(label, member, change):
            broken = dict(parts)
            broken[member] = change(bytearray(parts[member]))
            found = layout_problems(name, broken, movie, sheets, collections.Counter())
            print('  %s: %s' % ('control' if found else 'CONTROL FAILED', label),
                  '-> %s' % found[0] if found else 'went unnoticed')
            return bool(found)

        def shift_uv(geo):  # the first textured batch's UV rectangle, one texel right
            shapes_at = u32(geo, 28)
            record = u32(geo, u32(geo, shapes_at + 8))
            u0, v0, u1, v1 = floats(geo, record + 88, 4)
            struct.pack_into('<2f', geo, record + 88, u0 + 1 / 256, v0)
            return bytes(geo)

        def drop_matrix_flag(apt):  # a PlaceObject that scales its child, with its matrix bit cleared
            chars = characters(bytes(apt), movie)
            count, character_at = u32(apt, movie + 20), u32(apt, movie + 24)
            for index in range(count):
                at = u32(apt, character_at + index * 4)
                if not at or u32(apt, at) != SPRITE:
                    continue
                frames, frame_at = u32(apt, at + 8), u32(apt, at + 12)
                for frame in range(frames):
                    for k in range(u32(apt, frame_at + 8 * frame)):
                        item = u32(apt, u32(apt, frame_at + 8 * frame + 4) + 4 * k)
                        if u32(apt, item) == PLACE and floats(apt, item + 16, 1)[0] != 1:
                            struct.pack_into('<I', apt, item + 4, u32(apt, item + 4) & ~HAS_MATRIX)
                            return bytes(apt)
            raise arc.FormatError('no scaled placement in %s (%d characters)' % (name, len(chars)))
        passed &= caught('a UV rectangle moved one texel', name + '.geo', shift_uv)
        passed &= caught('a scaled placement without its matrix flag', name + '.apt', drop_matrix_flag)
    print('failures: %d' % len(failures))
    for failure in failures[:10]:
        print('   ', failure)
    return 0 if passed else 1


def find(disc, wanted):
    hits = [e.name for e in entries(disc) if wanted in read_movie(disc, e)[0]['strings']]
    print('%s: %s' % (wanted, ', '.join(hits) or 'in no movie'))
    return 0 if hits else 1


def movie_entry(disc, movie):
    entry = next((e for e in entries(disc) if e.name == movie), None)
    if entry is None:
        raise arc.FormatError('no movie %s' % movie)
    return entry


def export(disc, movie, out_dir):
    info, sheets, _, _ = read_movie(disc, movie_entry(disc, movie))
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


def layout(disc, movie, out_dir, label=None, scale=2.0):
    """Each export as a node tree (JSON) and a picture (PNG), every sprite at its frame 0. With a
    label, an export that has it is drawn at the frame before it instead (such as the end of a
    show animation, before 'refresh'), and that frame number goes in the JSON."""
    info, sheets, parts, at = read_movie(disc, movie_entry(disc, movie))
    chars, shapes = read_layout(movie, parts, at, sheets)
    # UVs count rows as stored (v 0 is the first row); texture.rgba flips them for PNG
    pixels = {member: (image.width, texture.stored_rgba(image)) for member, image in sheets.items()}
    os.makedirs(out_dir, exist_ok=True)
    trees = []
    for export_name, character in [['_root', 0]] + info['exports']:  # the root: what frame 0 places
        tree = node(chars, shapes, character)
        frame = tree.get('labels', {}).get(label, 0) - 1 if label else -1
        if frame >= 0:
            tree = node(chars, shapes, character, frame)
            tree['shown_frame'] = frame
        tree['export'] = export_name
        trees.append(tree)
        width, height, rgba = render(tree, pixels, scale=scale)
        with open(os.path.join(out_dir, '%s_%s_%d.png' % (movie, export_name, character)), 'wb') as out:
            out.write(texture.png_bytes(width, height, rgba))
    with open(os.path.join(out_dir, movie + '_layout.json'), 'w', encoding='utf-8') as out:
        json.dump({'movie': movie, 'width': info['width'], 'height': info['height'],
                   'sheets': sorted(sheets), 'exports': trees}, out, indent=1)
    print('wrote %s: %d exports laid out' % (out_dir, len(trees)))
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 4 and argv[1] == 'find':
        return find(arc.Disc(argv[2]), argv[3])
    if len(argv) == 5 and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3], argv[4])
    if len(argv) in (5, 6) and argv[1] == 'layout':
        return layout(arc.Disc(argv[2]), argv[3], argv[4], *argv[5:])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        sys.exit(1)
