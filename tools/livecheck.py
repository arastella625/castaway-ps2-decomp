#!/usr/bin/env python3
"""Compare decoded assets with the game running in PCSX2, reading its memory over PINE.

Usage:
  python -I tools/livecheck.py snapshot <out.bin>
  python -I tools/livecheck.py watch    <out_dir> [limit]
  python -I tools/livecheck.py compare  <game.iso> <snapshot.bin>

PCSX2 needs PINE turned on, on the default slot. snapshot saves the game's main memory. watch
saves one whenever a Sim's composite maps change, until the game has been closed for a while.
compare finds each Sim, works out its parts from the game's own tables, builds its maps by the
rules in docs/ASSET_FORMATS.md, and scores them against the game's maps. This tool only ever
sends PINE's read and status requests. Snapshots are game data: keep them out of the repo.
"""
import collections
import hashlib
import os
import re
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import texture as tex  # noqa: E402

PINE_PORT = 28011
PINE_READ64 = 3
PINE_BATCH = 8000
RAM_BASE = 0x100000
RAM_SIZE = 0x1F00000
CONNECT_TIMEOUT = 20
POLL_SECONDS = 4
RETRY_SECONDS = 5
DEFAULT_LIMIT = 30
GIVE_UP_WAITING = 3 * 3600
STOP_AFTER_CLOSE = 15 * 60

SIM_VTABLE = 0x004ACBF0
SIM_VTABLE_OFFSET = 0x330
SIM_MAGIC = 0xFEED
TEXTURE_VTABLE = 0x005051A0
MAP_MAGIC_SIZES = (128, 256)
CLOSE_ENOUGH = 8
VISIBLE_GAME_ALPHA = 8
VISIBLE_ALPHA = 16
MAP_NAME = re.compile(rb'(a[fm])_c(ul|ur|ll|lr)_([A-Za-z0-9_]{2,40})\0')

# Field offset, quadrant and rectangle in 128ths, from CasSimDescription and SimBodyPart.
PARTS = {'facial': (0x124, 'll', (64, 64, 128, 96)), 'hair': (0x150, 'll', (64, 0, 128, 64)),
         'glasses': (0x17C, 'll', (0, 32, 32, 64)), 'torso': (0x1A0, 'ur', (0, 0, 128, 128)),
         'lower': (0x1C4, 'lr', (0, 0, 128, 128)), 'shoes': (0x1E8, 'ul', (64, 0, 128, 64)),
         'hat': (0x20C, 'll', (0, 64, 64, 128)), 'necklace': (0x230, 'll', (32, 32, 64, 64)),
         'arm': (0x254, 'll', (0, 0, 32, 32)), 'belt': (0x278, 'll', (32, 0, 64, 32)),
         'addon upper': (0x29C, 'll', (64, 96, 128, 128)),
         'addon lower': (0x2C0, 'll', (64, 64, 128, 96)), 'sleeves': (0x2E4, 'ul', (0, 64, 128, 128))}
HEAD_RECT = (0, 0, 64, 64)
EXTRA_COLOURS = ('n', 'texture1', 'texture2')
VARIANTS = ('1', '2')


class Pine:
    """A read-only PINE client."""

    def __init__(self):
        self.sock = socket.create_connection(('127.0.0.1', PINE_PORT), timeout=CONNECT_TIMEOUT)

    def receive(self, count):
        data = bytearray()
        while len(data) < count:
            chunk = self.sock.recv(count - len(data))
            if not chunk:
                raise OSError('PCSX2 closed the connection')
            data += chunk
        return bytes(data)

    def call(self, payload):
        self.sock.sendall(struct.pack('<I', 4 + len(payload)) + payload)
        size = struct.unpack('<I', self.receive(4))[0]
        body = self.receive(size - 4)
        if body[0] != 0:
            raise OSError('PCSX2 refused a read')
        return body[1:]

    def read(self, address, size):
        data = bytearray()
        step = 8 * PINE_BATCH
        for base in range(address, address + size, step):
            count = min(step, address + size - base) // 8
            data += self.call(b''.join(bytes([PINE_READ64]) + struct.pack('<I', base + i * 8)
                                       for i in range(count)))
        return bytes(data)


class Memory:
    """A snapshot of main memory, read by game address."""

    def __init__(self, data):
        self.data = data

    def valid(self, address, size=4):
        return RAM_BASE <= address and address + size <= RAM_BASE + len(self.data)

    def bytes_at(self, address, size):
        if not self.valid(address, size):
            raise arc.FormatError('an address %#x is outside memory' % address)
        return self.data[address - RAM_BASE:address - RAM_BASE + size]

    def u32(self, address):
        return struct.unpack('<I', self.bytes_at(address, 4))[0]

    def u8(self, address):
        return self.bytes_at(address, 1)[0]

    def text(self, address):
        if not self.valid(address, 1):
            return ''
        start = address - RAM_BASE
        end = self.data.find(b'\0', start, start + 80)
        return self.data[start:end].decode('latin-1') if end >= 0 else ''

    def count(self, array):
        """A game VECTOR keeps its element count just before the first element."""
        return self.u32(array - 4) if array else 0

    def pointers_to(self, target):
        pattern = struct.pack('<I', target)
        pos = self.data.find(pattern)
        while pos >= 0:
            if pos % 4 == 0:
                yield pos + RAM_BASE
            pos = self.data.find(pattern, pos + 1)


def unshuffle_palette(raw):
    """Live composite palettes are in the GS's CSM1 order, unlike textures on the disc."""
    out = bytearray(raw)
    for block in range(0, 256, 32):
        out[(block + 8) * 4:(block + 16) * 4] = raw[(block + 16) * 4:(block + 24) * 4]
        out[(block + 16) * 4:(block + 24) * 4] = raw[(block + 8) * 4:(block + 16) * 4]
    return out


def map_header(memory, name_address):
    """Follow a map's name to its texture's header; return (size, pixels, palette) or None."""
    for reference in memory.pointers_to(name_address):
        if not memory.valid(reference - 12):
            continue
        texture = memory.u32(reference - 12)
        if not memory.valid(texture, 0x18) or memory.u32(texture) != TEXTURE_VTABLE:
            continue
        header = memory.u32(texture + 0x14)
        if not memory.valid(header, 0x30):
            continue
        size = memory.u32(header + 0x10) & 0xFFFF
        if size in MAP_MAGIC_SIZES:
            return size, memory.u32(header + 0x28), memory.u32(header + 0x2C)
    return None


def game_map(memory, name_address):
    found = map_header(memory, name_address)
    if found is None:
        return None
    size, pixels, palette = found
    colours = unshuffle_palette(memory.bytes_at(palette, 1024))
    return size, [tuple(colours[i * 4:i * 4 + 4]) for i in memory.bytes_at(pixels, size * size)]


def find_maps(memory):
    maps = collections.defaultdict(dict)
    for match in MAP_NAME.finditer(memory.data):
        key = (match.group(1).decode('ascii'), match.group(3).decode('ascii'))
        maps[key][match.group(2).decode('ascii')] = match.start() + RAM_BASE
    return maps


def find_sims(memory):
    """Return {FIRST_LAST: address}, keeping the first record of each name."""
    sims = {}
    for reference in memory.pointers_to(SIM_VTABLE):
        sim = reference - SIM_VTABLE_OFFSET
        if not memory.valid(sim, SIM_VTABLE_OFFSET) or memory.u32(sim + 0x84) & 0xFFFF != SIM_MAGIC:
            continue
        first = memory.bytes_at(sim, 64).decode('utf-16-le', 'replace').split('\0')[0]
        last = memory.bytes_at(sim + 0x40, 64).decode('utf-16-le', 'replace').split('\0')[0]
        sims.setdefault(('%s_%s' % (first, last)).upper().replace(' ', '_'), sim)
    return sims


def read_part(memory, part):
    """Walk the game's tables as GetModelCode, GetStyleCode and GetTextureName do."""
    codes = memory.u32(memory.u32(part + 0xC) + 8)
    code_index = memory.u8(part + 0x19)
    first_code = memory.text(memory.u32(codes))
    code = memory.text(memory.u32(codes + code_index * 8)) if code_index < memory.count(codes) \
        else first_code
    styles = memory.u32(codes + code_index * 8 + 4)
    style_index = memory.u8(part + 0x1B)
    style = memory.text(memory.u32(styles + style_index * 8)) \
        if style_index < memory.count(styles) else ''
    clothing_set = memory.u32(part + 0x10)
    set_name = memory.text(memory.u32(clothing_set)) if clothing_set else ''
    variations = memory.u32(clothing_set + 8) if clothing_set else 0
    index = memory.u8(part + 0x1C)
    colour = memory.text(memory.u32(variations + index * 4)) \
        if index < memory.count(variations) else ''
    return first_code, code, style, set_name, colour, memory.u8(part + 0x1D)


def texture_names(prefix, first_code, style, set_name, colour, ending):
    base = '%s_%s_%s' % (prefix, first_code, set_name)
    stems = [base + '_' + style, base] if style else [base]
    colours = ([colour] if colour else []) + list(EXTRA_COLOURS)
    return ['%s_%s_%s%s' % (stem, c, v, ending) for stem in stems for c in colours for v in VARIANTS]


class Library:
    def __init__(self, disc):
        self.disc = disc
        self.archive = arc.find_archive(disc, 'TEXTURES')
        self.entries = {e.name.lower(): e for e in arc.read_toc(disc, self.archive)}

    def load(self, name):
        """Return (width, height, stored RGBA, name), or None."""
        entry = self.entries.get(name.lower())
        if entry is None:
            return None
        data = self.disc.read(self.archive.lba, entry.offset, entry.size)
        texture = tex.parse_texture(data, entry.name)
        return texture.width, texture.height, tex.stored_rgba(texture), entry.name


def opaque(texture):
    pixels = texture[2]
    return [(pixels[i], pixels[i + 1], pixels[i + 2], 255) for i in range(0, len(pixels), 4)]


def place(canvas, texture, rect, size, blend):
    width, height, pixels = texture[:3]
    left, low, right, high = rect
    out = list(canvas)
    for y in range(low, high):
        row = (y - low) * height // (high - low)
        for x in range(left, right):
            at = (row * width + (x - left) * width // (right - left)) * 4
            alpha = pixels[at + 3]
            if blend:
                under = out[y * size + x]
                out[y * size + x] = tuple((pixels[at + k] * alpha + under[k] * (255 - alpha)) // 255
                                          for k in range(3)) + (255,)
            else:
                out[y * size + x] = (pixels[at], pixels[at + 1], pixels[at + 2], alpha)
    return out


def score(mine, theirs, rect, size, visible_only):
    """Return (mean difference, percent close, pixels), or None if no pixel counts."""
    left, low, right, high = rect
    total = close = count = 0
    for y in range(low, high):
        for x in range(left, right):
            a, b = mine[y * size + x], theirs[y * size + x]
            if visible_only and (a[3] < VISIBLE_ALPHA or b[3] < VISIBLE_GAME_ALPHA):
                continue
            differences = [abs(a[k] - b[k]) for k in range(3)]
            total += sum(differences) / 3
            close += max(differences) <= CLOSE_ENOUGH
            count += 1
    return (total / count, 100 * close / count, count) if count else None


def compare_part(library, details, prefix, skin, maps, rect, quadrant, size, ending):
    """Try each name the part could use; return the best (mean, percent, pixels, name)."""
    best = None
    for name in texture_names(prefix, details[0], details[2], details[3], details[4], ending):
        texture = library.load(name)
        if texture is None:
            continue
        if quadrant == 'll':
            mine = place([(0, 0, 0, 0)] * (size * size), texture, rect, size, False)
            result = score(mine, maps['ll'], rect, size, True)
        else:
            mine = place(skin[quadrant], texture, rect, size, True)
            result = score(mine, maps[quadrant], rect, size, False)
        if result and (best is None or result[0] < best[0]):
            best = result + (texture[3],)
    return best


def compare_sim(memory, library, sim, prefix, addresses):
    tones = memory.u32(memory.u32(sim + 0x8C) + 0x44)
    tone = memory.text(memory.u32(tones + memory.u8(sim + 0xFB) * 12))
    number = memory.u8(sim + 0xFC) + 1
    read = {k: game_map(memory, a) for k, a in addresses.items()}
    if not all(read.values()) or len(read) < 4:
        print('    the four maps could not all be read')
        return
    size = read['ur'][0]
    maps = {k: v[1] for k, v in read.items()}
    ending = '_game' if size == 128 else ''
    unit = size // 128
    print('    tone %s %d, body type %d, maps %d pixels' % (tone, number, memory.u8(sim + 0xFA), size))
    skin = {}
    for quadrant in ('ul', 'ur', 'lr'):
        texture = library.load('%s_%s_%s_normal_%d%s' % (prefix, quadrant, tone, number, ending))
        if texture is None:
            print('    no skin map for %s' % quadrant)
            return
        skin[quadrant] = opaque(texture)
    head = score(skin['ul'], maps['ul'], tuple(v * unit for v in HEAD_RECT), size, False)
    print('    %-12s %-30s %6.2f %6.1f%%' % ('head', 'skin alone', head[0], head[1]))
    for name, (offset, quadrant, rect) in PARTS.items():
        details = read_part(memory, sim + offset)
        if details[3] in ('', 'none'):
            continue
        scaled = tuple(v * unit for v in rect)
        label = '/'.join(details[1:5]) + ('/worn %d' % details[5] if details[5] else '')
        best = compare_part(library, details, prefix, skin, maps, scaled, quadrant, size, ending)
        if best:
            print('    %-12s %-30s %6.2f %6.1f%%  %s' % (name, label, best[0], best[1], best[3]))
        else:
            print('    %-12s %-30s no texture found' % (name, label))


def compare(disc, path):
    with open(path, 'rb') as source:
        memory = Memory(source.read())
    library = Library(disc)
    sims = find_sims(memory)
    print('Sim records: %s' % sorted(sims))
    for (prefix, who), addresses in sorted(find_maps(memory).items()):
        print('%s %s' % (prefix, who))
        if who in sims:
            compare_sim(memory, library, sims[who], prefix, addresses)
        else:
            print('    has maps but no Sim record')
    return 0


def map_signature(memory):
    """Hash every composite map's pixels; return (maps found, digest)."""
    digest = hashlib.sha1()
    found = 0
    for (_, who), addresses in sorted(find_maps(memory).items()):
        for quadrant, address in sorted(addresses.items()):
            header = map_header(memory, address)
            if header:
                digest.update((who + quadrant).encode('ascii'))
                digest.update(memory.bytes_at(header[1], header[0] * header[0]))
                found += 1
    return found, digest.hexdigest()


def snapshot(path):
    data = Pine().read(RAM_BASE, RAM_SIZE)
    with open(path, 'wb') as out:
        out.write(data)
    print('saved %s' % path)
    return 0


def watch(out_dir, limit):
    os.makedirs(out_dir, exist_ok=True)
    started = time.time()
    last_seen = None
    seen = set()
    while len(seen) < limit:
        try:
            pine = Pine()
            while len(seen) < limit:
                data = pine.read(RAM_BASE, RAM_SIZE)
                last_seen = time.time()
                found, signature = map_signature(Memory(data))
                if found >= 4 and signature not in seen:
                    seen.add(signature)
                    path = os.path.join(out_dir, 'snap_%02d.bin' % len(seen))
                    with open(path, 'wb') as out:
                        out.write(data)
                    print('%s saved %s' % (time.strftime('%H:%M:%S'), path), flush=True)
                time.sleep(POLL_SECONDS)
        except OSError:
            pass
        waited = time.time() - (last_seen or started)
        if waited > (STOP_AFTER_CLOSE if last_seen else GIVE_UP_WAITING):
            break
        time.sleep(RETRY_SECONDS)
    print('saved %d snapshots' % len(seen))
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'snapshot':
        return snapshot(argv[2])
    if len(argv) in (3, 4) and argv[1] == 'watch':
        return watch(argv[2], int(argv[3]) if len(argv) == 4 else DEFAULT_LIMIT)
    if len(argv) == 4 and argv[1] == 'compare':
        return compare(arc.Disc(argv[2]), argv[3])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that PCSX2 is running with PINE on, or that the snapshot is complete.',
              file=sys.stderr)
        sys.exit(1)
