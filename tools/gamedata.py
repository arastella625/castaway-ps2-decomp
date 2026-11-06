#!/usr/bin/env python3
"""Read the game's world and data files: names, indexes, datasets, lots and quickdata.

Usage:
  python -I tools/gamedata.py check <game.iso>
  python -I tools/gamedata.py dump  <game.iso> tables
  python -I tools/gamedata.py dump  <game.iso> <table> [language]

check proves the name hash on every archive entry and parses every INDEX record, dataset,
lot, model header, Edith tree set and quickdata image to its exact last byte, with a
negative control for each. dump prints a quickdata table as text; "tables" lists them.
A table is a name like ObjDefinition or Sim::NPC, or file/table if a name repeats. Three
more names decode Create-a-Sim fully: "clothing" (every clothing set with its models),
"costumes" and "bodyparts". language is 0 to 20. Output is game data: keep it out of the repo.
"""
import binascii
import collections
import math
import os
import struct
import sys
from dataclasses import dataclass, field

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402

FormatError = arc.FormatError

CRC_POLY = 0xEDB88320
MASK32 = 0xFFFFFFFF
UNDEFINED_NAME = 'undefined'  # EResourceManager::CalcId gives this name the id 0

INDEX_FILES = ('DATA/INDEX.IND', 'DATA/INDEX1.IND')
INDEX_NAME_CHARS = 8  # "AudioStreams" is AUDIOSTR.ARC
INDEX_LONG_HEADER = 20
SAMPLE_INDEX_NAME = 'Samples'
VAG_HEADER_SIZE = 64

DATASET_VERSION = 10
DATASET_TAG = b'TSTD'
DATASET_NAME_SIZE = 64
DATASET_TYPE_SIZE = 32

EDITH_VERSION = 3
EDITH_TAG = b'HTDE'
IFF_BANNER = b'IFF FILE 2.5:TYPE FOLLOWED BY SIZE\0 JAMIE DOORNBOS & MAXIS 1\0'
IFF_HEADER_SIZE = 64
IFF_CHUNK_HEADER = 76
BHAV_HEADER = 12
BHAV_INSTRUCTION = 12

MODEL_VERSION = 69
MODEL_TAG = b'LDOM'
OBJD_GUID = 0x1C
STALE_OBJT_GUIDS = 1  # house 27 lists 0x334945B8, which ObjDefinition doesn't have
CHARACTER_SCALE = 1 / 4096
MODEL_TAIL = 71  # sphere, two boxes and seven flag bytes after the last submodel

OBJECT_END_MARK = 0xB7  # EStream::ReadStructure reads one byte after each object
LEVEL_CLASSES = {binascii.crc32(n.encode()): n for n in
                 ('EInstance', 'EStorable', 'EResource', 'EIStaticSubModel', 'ERLevel')}
LEVEL_VERSION = 7
MAX_LEVEL_ITEMS = 100000

XIMAGE_DIRECTORY = 0x18
QD_TABLE_ENTRY = 20
QD_TABLES_AT = 0x18
QD_ENGLISH = 0  # the first language image; see FORMAT_NOTES

# The 18 vectors of a Sim::SimPartsTSC6 row, from CasSimDescription and the data itself.
SIM_PARTS = ('head', 'facialFeature', 'hair', 'glasses', 'torso', 'lowerBody', 'shoes', 'hat',
             'necklaceEarring', 'armAccessory', 'belt', 'addOnUpperBody', 'addOnLowerBody')
GEOMETRY_SIZE = 0x18
COSTUME_COLUMNS = ('male', 'female')  # Sim flag 0 picks 'am' and the first list
BODY_PART_TYPE_SIZE = 0x18
MODEL_CODE_SIZE = 8
STYLE_SIZE = 8
TORSO_STYLES = ('_ti_', '_ti_', '_slti_', '_slti_')  # model index 2 + style is the sleeve

NGH_FILE = 'DEFAULT.NGH'
NGH_SECTION_ALIGN = 0x800
NGH_HEADER_SIZE = 0x1C
NGH_KIND_THUMBNAIL = 2
NGH_KIND_HOUSE = 3
LZSS_WINDOW = 4096
LZSS_START = 0xFEE


def _crc_table():
    table = []
    for index in range(256):
        value = index
        for _ in range(8):
            value = (value >> 1) ^ CRC_POLY if value & 1 else value >> 1
        table.append(value)
    return table


CRC_TABLE = _crc_table()
DIGIT_START = CRC_TABLE[0xA0] ^ 0x00FFFFFF  # the state after hashing one '_'


def symbol_byte(byte):
    """EChecksum::ComputeSymbol's folding: upper case, and anything else non-alphanumeric is '_'."""
    if 0x61 <= byte <= 0x7A:
        return byte - 0x20
    if 0x41 <= byte <= 0x5A or 0x30 <= byte <= 0x39:
        return byte
    return 0x5F


def name_hash(name):
    """EResourceManager::CalcId: the id of a resource name."""
    if not name or name.lower() == UNDEFINED_NAME:
        return 0
    data = name.encode('latin-1')
    crc = DIGIT_START if 0x30 <= data[0] <= 0x39 else MASK32
    for byte in data:
        crc = (crc >> 8) ^ CRC_TABLE[(crc ^ symbol_byte(byte)) & 0xFF]
    return crc ^ MASK32


class Reader:
    """Bounds-checked reads from a byte string."""

    def __init__(self, data, name, pos=0):
        self.data, self.name, self.pos = data, name, pos

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise FormatError('%s: read past the end at %#x' % (self.name, self.pos))
        chunk = self.data[self.pos:self.pos + size]
        self.pos += size
        return chunk

    def unpack(self, form):
        return struct.unpack(form, self.take(struct.calcsize(form)))

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return self.unpack('<H')[0]

    def u32(self):
        return self.unpack('<I')[0]

    def floats(self, count):
        return self.unpack('<%df' % count)

    def count(self, limit=MAX_LEVEL_ITEMS):
        value = self.u32()
        if value > limit:
            raise FormatError('%s: count %d at %#x is too big' % (self.name, value, self.pos))
        return value

    def cstring(self):
        end = self.data.find(b'\0', self.pos)
        if end < 0:
            raise FormatError('%s: unterminated string at %#x' % (self.name, self.pos))
        text = self.data[self.pos:end].decode('latin-1')
        self.pos = end + 1
        return text

    def fixed_string(self, size):
        return self.take(size).split(b'\0')[0].decode('latin-1')

    def at_end(self):
        return self.pos == len(self.data)


def read_data_header(reader, version, tag):
    """EDataHeader: version, tag, -1, name; then the bytes left, which must fit the file."""
    found = reader.unpack('<I4si')
    if found != (version, tag, -1):
        raise FormatError('%s: bad data header %r' % (reader.name, found))
    name = reader.take(reader.count()).rstrip(b'\0').decode('latin-1')
    left = reader.u32()
    if left != len(reader.data) - reader.pos:
        raise FormatError('%s: header says %d bytes left, file has %d'
                          % (reader.name, left, len(reader.data) - reader.pos))
    return name


# --- INDEX.IND and INDEX1.IND -----------------------------------------------------------

@dataclass
class IndexRecord:
    name: str
    hashes: tuple
    places: tuple  # (position, size) per hash
    extras: tuple = ()
    tail: bytes = b''


def read_index(data, label):
    """Return the records of an index file: a list of (name, record) offset pairs."""
    reader = Reader(data, label)
    count = reader.count()
    if count % 2:
        raise FormatError('%s: odd offset count' % label)
    offsets = reader.unpack('<%dI' % (count + 1))
    if offsets[0] != reader.pos or offsets[-1] != len(data) or list(offsets) != sorted(offsets):
        raise FormatError('%s: offsets do not span the file' % label)
    return [read_index_record(data[offsets[i]:offsets[i + 1]].split(b'\0')[0].decode('latin-1'),
                              data[offsets[i + 1]:offsets[i + 2]])
            for i in range(0, count, 2)]


def read_index_record(name, data):
    """The layout EResourceManager::LookupId walks: hashes, then position and size pairs."""
    reader = Reader(data, name)
    first = reader.u32()
    count, start = first, 4
    if first == 0:
        count, size, kind, tail_size = reader.unpack('<4I')
        start = INDEX_LONG_HEADER
        if size != len(data):
            raise FormatError('%s: record says %d bytes, has %d' % (name, size, len(data)))
    reader.pos = start
    hashes = reader.unpack('<%dI' % count)
    flat = reader.unpack('<%dI' % (2 * count))
    places = tuple(zip(flat[0::2], flat[1::2]))
    extras, tail = (), b''
    if first == 0 and kind >> 16:  # Samples: one word more per entry, then VAG headers
        extras = reader.unpack('<%dI' % count)
        tail = reader.take(tail_size)
    if not reader.at_end():
        raise FormatError('%s: %d bytes left over' % (name, len(data) - reader.pos))
    return IndexRecord(name, hashes, places, extras, tail)


def archive_for_index(name):
    return name[:INDEX_NAME_CHARS].upper()


def sample_rates(record):
    """The rates in the VAG headers after the Samples record, in file order."""
    rates = []
    for pos in range(0, len(record.tail), VAG_HEADER_SIZE):
        header = record.tail[pos:pos + VAG_HEADER_SIZE]
        if header[:4] != b'VAGp':
            raise FormatError('Samples: no VAG header at %d' % pos)
        rates.append(struct.unpack_from('<I', header, 16)[0])
    return rates


# --- Datasets ---------------------------------------------------------------------------

@dataclass
class DatasetItem:
    kind: str
    name_hash: int
    offset: int
    size: int


def read_dataset(data, label):
    """ERDataset::Load: named copies of resources in one stream, for one seek per lot."""
    reader = Reader(data, label)
    name = read_data_header(reader, DATASET_VERSION, DATASET_TAG)
    if reader.fixed_string(DATASET_NAME_SIZE) != name:
        raise FormatError('%s: the two names differ' % label)
    items = []
    for _ in range(reader.count()):
        kind = reader.fixed_string(DATASET_TYPE_SIZE)
        item_hash, size, padding = reader.unpack('<3I')
        reader.take(padding)
        items.append(DatasetItem(kind, item_hash, reader.pos, size))
        reader.take(size)
    if not reader.at_end():
        raise FormatError('%s: %d bytes left over' % (label, len(data) - reader.pos))
    return name, items


# --- Edith trees ------------------------------------------------------------------------

def read_edith(data, label):
    """EREdithTreeSet::Load: an IFF file of BHAV chunks. Returns (id, name, sig, opcodes)."""
    reader = Reader(data, label)
    read_data_header(reader, EDITH_VERSION, EDITH_TAG)
    if reader.take(IFF_HEADER_SIZE)[:len(IFF_BANNER)] != IFF_BANNER:
        raise FormatError('%s: no IFF banner' % label)
    trees = []
    while not reader.at_end():
        kind = reader.take(4)
        size, chunk_id, _flags = reader.unpack('>IHH')
        name = reader.fixed_string(IFF_CHUNK_HEADER - 12)
        body = Reader(reader.take(size - IFF_CHUNK_HEADER), label)
        if kind != b'BHAV':
            raise FormatError('%s: unknown chunk %r' % (label, kind))
        signature, count = body.unpack('<HH')
        body.take(BHAV_HEADER - 4)
        opcodes = [struct.unpack('<H', body.take(BHAV_INSTRUCTION)[:2])[0] for _ in range(count)]
        if not body.at_end():
            raise FormatError('%s: BHAV %s has %d bytes over'
                              % (label, name, len(body.data) - body.pos))
        trees.append((chunk_id, name, signature, opcodes))
    return trees


# --- Model headers ----------------------------------------------------------------------

@dataclass
class ModelInfo:
    scale: float
    submodels: int
    sphere: tuple
    box: tuple  # stored in model units; multiply by scale for metres
    counts: tuple  # 64-byte records, lattices, dummies, cameras, lights


def skip_named_points(reader, point_size):
    if reader.u32() != 0:
        raise FormatError('%s: unknown node version' % reader.name)
    reader.take(0x44)
    reader.take(reader.count() * point_size)


def read_model_info(data, label):
    """ERModel::LoadFromFile up to the scale, then the fixed tail at the end of the file."""
    reader = Reader(data, label)
    read_data_header(reader, MODEL_VERSION, MODEL_TAG)
    reader.take(4 + 0x30 + 1)
    tables = reader.count()
    reader.take(tables * 64)
    lattices = reader.count()
    for _ in range(lattices):
        if reader.u32() != 0:
            raise FormatError('%s: unknown lattice version' % label)
        reader.take(0x80)
        _first, x, y, z, frames = reader.unpack('<5I')
        if frames > 1:
            reader.take(frames * x * y * z * 12)
    dummies = reader.count()
    for _ in range(dummies):
        skip_named_points(reader, 0x50)
    cameras = reader.count()
    for _ in range(cameras):
        skip_named_points(reader, 0x60)
    lights = reader.count()
    reader.take(lights * 28)
    reader.u8()
    scale = reader.floats(1)[0]
    submodels = reader.count()
    if len(data) - MODEL_TAIL < reader.pos:
        raise FormatError('%s: no room for the tail' % label)
    tail = struct.unpack_from('<4f6f6f', data, len(data) - MODEL_TAIL)
    return ModelInfo(scale, submodels, tail[:4], tail[10:16],
                     (tables, lattices, dummies, cameras, lights))


def is_power_of_two(value):
    return value > 0 and math.frexp(value)[0] == 0.5


# --- DEFAULT.NGH ------------------------------------------------------------------------

def lzss_decode(data, size):
    """LZSSCompressor::Decode: Okumura's LZSS with a 4096-byte window of zeros."""
    window = bytearray(LZSS_WINDOW)
    write, pos, flags = LZSS_START, 0, 0
    out = bytearray()
    while pos < len(data):
        flags >>= 1
        if not flags & 0x100:
            flags = data[pos] | 0xFF00
            pos += 1
        if flags & 1:
            if pos >= len(data):
                break
            copy = [data[pos]]
            pos += 1
        else:
            if pos + 1 >= len(data):
                break
            start = data[pos] | (data[pos + 1] & 0xF0) << 4
            length = (data[pos + 1] & 0xF) + 3
            pos += 2
            copy = None
        for index in range(1 if copy else length):
            byte = copy[0] if copy else window[(start + index) & 0xFFF]  # may read fresh bytes
            out.append(byte)
            window[write] = byte
            write = (write + 1) & 0xFFF
        if len(out) > size:
            break
    if len(out) != size:
        raise FormatError('LZSS gave %d bytes, wanted %d' % (len(out), size))
    return bytes(out)


@dataclass
class NghSection:
    offset: int
    kind: int
    data: bytes
    item_count: int


def read_ngh_sections(data):
    """NghResFile::DecompressSection for every section header on a 2048-byte boundary."""
    sections = []
    for offset in range(0, len(data) - NGH_HEADER_SIZE, NGH_SECTION_ALIGN):
        kind, header, size, packed, _check, _version, count = \
            struct.unpack_from('<7I', data, offset)
        if header != NGH_HEADER_SIZE:
            continue
        if kind == NGH_KIND_THUMBNAIL:  # ThumbnailCompresser, not read here
            sections.append(NghSection(offset, kind, b'', 0))
            continue
        body = data[offset + header:offset + header + packed]
        sections.append(NghSection(offset, kind, lzss_decode(body, size), count))
    return sections


def house_chunks(section):
    """GetTableResArray and friends: types, starts, lengths, ids. Starts count the header."""
    count, data = section.item_count, section.data
    kinds = [data[i * 4:i * 4 + 4][::-1].decode('latin-1') for i in range(count)]
    starts = struct.unpack_from('<%dI' % count, data, count * 4)
    sizes = struct.unpack_from('<%dI' % count, data, count * 8)
    ids = struct.unpack_from('<%dH' % count, data, count * 12)
    chunks = []
    for kind, start, size, chunk_id in zip(kinds, starts, sizes, ids):
        begin = start - NGH_HEADER_SIZE
        if begin < 0 or begin + size > len(data):
            raise FormatError('NGH: chunk %s runs past its section' % kind)
        chunks.append((kind, chunk_id, data[begin:begin + size]))
    if not 0 <= len(data) - (starts[-1] - NGH_HEADER_SIZE + sizes[-1]) < 4:
        raise FormatError('NGH: section at %#x has bytes after its last chunk' % section.offset)
    return chunks


def read_objt(data):
    """The object type list of a house: guid, six u16, name padded to even length."""
    reader = Reader(data, 'objt')
    if reader.unpack('<2I4s') != (0, 2, b'tjbo'):
        raise FormatError('objt: bad header')
    types = []
    while True:
        guid = reader.u32()
        if guid == 0:  # the list ends with a zero word
            if not reader.at_end():
                raise FormatError('objt: bytes after the end')
            return types
        fields = reader.unpack('<6H')
        start = reader.pos
        name = reader.cstring()
        if (reader.pos - start) % 2:
            reader.take(1)
        types.append((guid, fields, name))


# --- Lots -------------------------------------------------------------------------------

@dataclass
class SubModelInstance:
    flags: int
    level_ref: int
    tree_ref: int
    draw_flags: int
    box: tuple
    model_hash: int
    submodel: int
    sphere: tuple


@dataclass
class MidLotObject:
    position: tuple
    rotation: tuple  # degrees; quaternion Z * Y * X
    scale: tuple
    character_hash: int  # when set the model's own scale is not applied here
    model_hash: int
    anim_hash: int
    flags: int
    spare: tuple


@dataclass
class BitGrid:
    kind: int
    x: int
    y: int
    width: int
    height: int
    bits: bytes

    def get(self, x, y):
        """BitArray2D::Get, with x and y in tile numbers."""
        col, row = x - self.x, y - self.y
        if not (0 <= col < self.width and 0 <= row < self.height):
            return False
        return bool(self.bits[row * ((self.width + 7) >> 3) + (col >> 3)] >> (col & 7) & 1)


@dataclass
class Level:
    name: str
    version: int
    submodels: list
    lists: tuple
    objects: list
    grids: list


def read_refs(reader):
    return tuple(reader.u32() for _ in range(reader.count()))


def read_class_table(reader):
    """EStream::ReadStructure's first half: class key, version, how many objects of it."""
    wanted = reader.count()
    order, made = [], 0
    while made < wanted:
        key, version, count = reader.unpack('<IHI')
        if key not in LEVEL_CLASSES:
            raise FormatError('%s: unknown class %#x' % (reader.name, key))
        made += count
        order += [(LEVEL_CLASSES[key], version)] * count
    if made != wanted:
        raise FormatError('%s: class table holds %d objects, wanted %d'
                          % (reader.name, made, wanted))
    return order


def read_submodel_instance(reader):
    """EIStaticSubModel::Read after EInstance::Read, at class versions 0 and 1."""
    flags, level_ref, tree_ref, draw_flags = reader.unpack('<4I')
    box = reader.floats(6)
    model_hash, submodel = reader.unpack('<2I')
    return SubModelInstance(flags, level_ref, tree_ref, draw_flags, box, model_hash, submodel,
                            reader.floats(4))


def read_mid_lot_object(reader):
    position, rotation, scale = reader.floats(3), reader.floats(3), reader.floats(3)
    character, model, anim, flags, spare_a, spare_b = reader.unpack('<6I')
    return MidLotObject(position, rotation, scale, character, model, anim, flags,
                        (spare_a, spare_b))


def read_bit_grid(reader):
    kind = reader.u16()
    x, y, width, height = reader.unpack('<4B')
    return BitGrid(kind, x, y, width, height, reader.take(((width + 7) >> 3) * height))


def read_level_object(reader, version):
    """ERLevel::Read for class version 7."""
    if version != LEVEL_VERSION:
        raise FormatError('%s: level version %d' % (reader.name, version))
    name = reader.cstring()
    file_version = reader.u32()
    reader.take(12)
    if reader.count() != 0:
        raise FormatError('%s: instance tree is not empty' % reader.name)
    reader.u32()
    first = read_refs(reader)
    reader.take(8)
    second, third = read_refs(reader), read_refs(reader)
    fourth = read_refs(reader)
    objects = [read_mid_lot_object(reader) for _ in range(reader.count())]
    grids = [read_bit_grid(reader) for _ in range(3)]
    return Level(name, file_version, [], (first, second, third, fourth), objects, grids)


def read_level(data, label):
    """ERLevel::AllocAndLoadLevel: size, object graph, root index, 0."""
    reader = Reader(data, label)
    tail_at = reader.u32()
    order = read_class_table(reader)
    instances, level = [], None
    for class_name, version in order:
        if class_name == 'EIStaticSubModel':
            instances.append(read_submodel_instance(reader))
        elif class_name == 'ERLevel':
            level = read_level_object(reader, version)
        else:
            raise FormatError('%s: no reader for %s' % (label, class_name))
        if reader.u8() != OBJECT_END_MARK:
            raise FormatError('%s: no end mark at %#x' % (label, reader.pos))
    if reader.pos != tail_at:
        raise FormatError('%s: objects end at %#x, not %#x' % (label, reader.pos, tail_at))
    root, last = reader.unpack('<2I')
    if not reader.at_end() or last != 0 or level is None or root != len(order) - 1:
        raise FormatError('%s: bad ending' % label)
    level.submodels = instances
    return level


# --- Quickdata --------------------------------------------------------------------------

def read_fixups(words):
    """applyFixups: the offsets of every pointer in an image."""
    offsets, pos = [], 0
    while pos < len(words):
        word = words[pos]
        pos += 1
        kind = word & 3
        if kind == 0:
            offsets.append(word)
        elif kind == 1:
            if pos >= len(words):
                raise FormatError('fixup run with no start')
            offsets += [words[pos] + 4 * i for i in range(word >> 2)]
            pos += 1
        else:
            offsets += [(word & ~3) + 4 * i for i in range(kind)]
    return offsets


@dataclass
class QdTable:
    name: str
    rows: int
    names: int
    count: int
    stride: int


class Quickdata:
    """ReloadXImage: an image with pointers as offsets, and one small image per language."""

    def __init__(self, data, label, language=QD_ENGLISH):
        self.label = label
        directory = self._directory(data, 0)
        languages = (directory[1] - XIMAGE_DIRECTORY) // XIMAGE_DIRECTORY
        self.language_count = languages
        self.main_size = directory[2]
        self.main_end = (directory[2] + 3) & ~3  # language images load after this
        last = directory[3] + directory[4]
        extras = []
        for index in range(languages):
            extra = self._directory(data, XIMAGE_DIRECTORY * (index + 1))
            extras.append(extra)
            last = max(last, extra[3] + extra[4])
        if last != len(data):
            raise FormatError('%s: images end at %#x, file at %#x' % (label, last, len(data)))
        self.pointers = set(self._fixups(data, directory))
        image = data[directory[1]:directory[1] + directory[2]]
        image += b'\0' * (self.main_end - len(image))
        if extras:
            chosen = extras[language]
            image += data[chosen[1]:chosen[1] + chosen[2]]
            self.pointers |= {self.main_end + o for o in self._fixups(data, chosen)}
        self.image = image
        for offset in self.pointers:
            if offset + 4 > len(image):
                raise FormatError('%s: pointer word %#x is outside the image' % (label, offset))
        self.tables = self._read_tables()

    def _directory(self, data, pos):
        if pos + XIMAGE_DIRECTORY > len(data):
            raise FormatError('%s: truncated directory' % self.label)
        entry = struct.unpack_from('<6I', data, pos)
        if entry[0] != XIMAGE_DIRECTORY or entry[5] != 0:
            raise FormatError('%s: bad directory at %#x' % (self.label, pos))
        if entry[1] + entry[2] > len(data) or entry[3] + entry[4] > len(data) or entry[4] % 4:
            raise FormatError('%s: directory runs past the file' % self.label)
        return entry

    @staticmethod
    def _fixups(data, directory):
        return read_fixups(struct.unpack_from('<%dI' % (directory[4] // 4), data, directory[3]))

    def _read_tables(self):
        _version, _name, count = struct.unpack_from('<3I', self.image, 0)
        tables = []
        for index in range(count):
            fields = struct.unpack_from('<5I', self.image, QD_TABLES_AT + QD_TABLE_ENTRY * index)
            tables.append(QdTable(self.string(fields[0]), *fields[1:]))
        if [t.name for t in tables] != sorted(t.name for t in tables):
            raise FormatError('%s: tables are not sorted' % self.label)
        return tables

    def word(self, pos):
        if pos < 0 or pos + 4 > len(self.image):
            raise FormatError('%s: read outside the image at %#x' % (self.label, pos))
        return struct.unpack_from('<I', self.image, pos)[0]

    def string(self, pos):
        end = self.image.find(b'\0', pos)
        if pos >= len(self.image) or end < 0:
            raise FormatError('%s: bad string at %#x' % (self.label, pos))
        return self.image[pos:end].decode('latin-1')

    def pointer_string(self, pos):
        """The string at the pointer in this word, or None if it's null."""
        return self.string(self.word(pos)) if pos in self.pointers else None

    def vector(self, pos):
        """A VECTOR<T>: returns (first element, count); the count sits just before the data."""
        start = self.word(pos)
        return (start, self.word(start - 4)) if start else (0, 0)

    def looks_like_string(self, pos):
        end = self.image.find(b'\0', pos)
        return 0 < end - pos < 96 and all(32 <= c < 127 for c in self.image[pos:end])

    def looks_like_wide(self, pos):
        text = self.wide_string(pos)
        return bool(text) and all(ord(c) < 0x3000 for c in text)

    def wide_string(self, pos):
        chars = []
        while pos + 2 <= len(self.image) and len(chars) < 256:
            value = struct.unpack_from('<H', self.image, pos)[0]
            if value == 0:
                return ''.join(chars)
            chars.append(chr(value))
            pos += 2
        return ''

    def row_name(self, table, index):
        return self.string(self.word(table.names + 4 * index)) if table.names else ''

    def describe_word(self, pos):
        value = self.word(pos)
        if pos in self.pointers:
            if self.looks_like_string(value):
                return repr(self.string(value))
            if value in self.pointers and self.word(value) >= self.main_end:
                return 'L' + repr(self.wide_string(self.word(value)))
            return '@%x' % value
        if value > 0xFFFF:
            number = struct.unpack_from('<f', self.image, pos)[0]
            if 1e-4 < abs(number) < 1e7:
                return '%.6g' % number
            return '%#x' % value
        return str(value)

    def dump_table(self, table, names):
        print('# %s: %d rows of %d bytes' % (table.name, table.count, table.stride))
        for index in range(table.count):
            pos = table.rows + index * table.stride
            words = []
            for offset in range(0, table.stride - table.stride % 4, 4):
                text = self.describe_word(pos + offset)
                value = self.word(pos + offset)
                words.append(names.get(value, text) if pos + offset not in self.pointers else text)
            print('%d\t%s\t%s' % (index, self.row_name(table, index), ' '.join(words)))


def load_quickdata(disc, language=QD_ENGLISH):
    archive = arc.find_archive(disc, 'QUICKDAT')
    return {entry.name: Quickdata(disc.read(archive.lba, entry.offset, entry.size), entry.name,
                                  language)
            for entry in arc.read_toc(disc, archive)}


def resource_names(disc):
    names = {}
    for archive in disc.archives():
        for entry in arc.read_toc(disc, archive):
            names.setdefault(entry.name_hash, entry.name)
    return names


def find_table(quickdata, wanted):
    file_name, _, table_name = wanted.rpartition('/')
    found = [(data, table) for name, data in sorted(quickdata.items())
             for table in data.tables
             if table.name == table_name and file_name in ('', name)]
    if len(found) != 1:
        raise FormatError('%d tables match %s; "dump <iso> tables" lists them'
                          % (len(found), wanted))
    return found[0]


def geometry_sets(data, vector_pos, names):
    """Rows of a VECTOR<Sim::BodyPartGeometryTSC6>."""
    start, count = data.vector(vector_pos)
    sets = []
    for index in range(count):
        pos = start + index * GEOMETRY_SIZE
        models_at, model_count = data.vector(pos + 4)
        textures_at, texture_count = data.vector(pos + 8)
        armour_at, armour_count = data.vector(pos + 0x14)
        models = [data.word(models_at + 4 * i) for i in range(model_count)]
        sets.append({
            'name': data.pointer_string(pos),
            'models': [names.get(m, '%#x' % m) if m else '-' for m in models],
            'textures': [data.pointer_string(textures_at + 4 * i) for i in range(texture_count)],
            'flags': data.word(pos + 0xC),
            'hit_points': data.word(pos + 0x10),
            'armour': list(data.image[armour_at:armour_at + armour_count]),
        })
    return sets


def dump_clothing(quickdata, names):
    data = quickdata['CreateASim']
    table = next(t for t in data.tables if t.name == 'Sim::SimPartsTSC6')
    for row in range(table.count):
        base = table.rows + row * table.stride
        for slot, part in enumerate(SIM_PARTS):
            for index, item in enumerate(geometry_sets(data, base + 4 * slot, names)):
                print('%s\t%s\t%d\t%s\tflags=%#x\thp=%d\tarmour=%s\ttextures=%s\tmodels=%s'
                      % (data.row_name(table, row), part, index, item['name'], item['flags'],
                         item['hit_points'], item['armour'], ','.join(item['textures']),
                         ','.join(item['models'])))


def dump_costumes(quickdata):
    """Sim::CostumesTSC6: per gender, a clothing set name for each of the 13 parts."""
    data = quickdata['CreateASim']
    table = next(t for t in data.tables if t.name == 'Sim::CostumesTSC6')
    for row in range(table.count):
        pos = table.rows + row * table.stride
        for gender, label in enumerate(COSTUME_COLUMNS):
            start, count = data.vector(pos + 4 + 4 * gender)
            sets = [data.pointer_string(start + 4 * i) for i in range(count)]
            print('%s	%s	%s' % (data.string(data.word(pos)), label,
                                  ' '.join('%s=%s' % (part, name)
                                           for part, name in zip(SIM_PARTS, sets) if name)))


def dump_body_parts(quickdata):
    """Sim::BodyPartTypeInfoTableTSC6: each body part, its model codes and their styles."""
    data = quickdata['CreateASim']
    table = next(t for t in data.tables if t.name == 'Sim::BodyPartTypeInfoTableTSC6')
    start, count = data.vector(table.rows)
    for index in range(count):
        pos = start + index * BODY_PART_TYPE_SIZE
        codes_at, code_count = data.vector(pos + 8)
        codes = []
        for code in range(code_count):
            styles_at, style_count = data.vector(codes_at + code * MODEL_CODE_SIZE + 4)
            styles = ['%s:%d' % (data.pointer_string(styles_at + i * STYLE_SIZE),
                                 data.image[styles_at + i * STYLE_SIZE + 4])
                      for i in range(style_count)]
            codes.append('%s(%s)' % (data.pointer_string(codes_at + code * MODEL_CODE_SIZE),
                                     ' '.join(styles)))
        defaults, nudes = data.word(pos + 0x10), data.word(pos + 0x14)
        print('%d\t%s\tdefaults=%s\tnude=%s\t%s'
              % (index, data.pointer_string(pos), list(data.image[defaults:defaults + 2]),
                 list(data.image[nudes:nudes + 2]), ' '.join(codes)))


def dump(disc, wanted, language):
    quickdata = load_quickdata(disc, language)
    if wanted == 'tables':
        for name, data in sorted(quickdata.items()):
            for table in data.tables:
                print('%s/%s\t%d rows\t%d bytes' % (name, table.name, table.count, table.stride))
        return 0
    names = resource_names(disc)
    if wanted == 'clothing':
        dump_clothing(quickdata, names)
    elif wanted == 'bodyparts':
        dump_body_parts(quickdata)
    elif wanted == 'costumes':
        dump_costumes(quickdata)
    else:
        data, table = find_table(quickdata, wanted)
        data.dump_table(table, names)
    return 0


# --- check ------------------------------------------------------------------------------

def report(label, passed, total, note=''):
    print('%-34s %7d of %7d pass  %s' % (label, passed, total, note))
    return passed == total


def expect_failure(label, action):
    """A negative control: this deliberately wrong input must raise FormatError."""
    try:
        action()
    except (FormatError, struct.error):
        print('  control: %s is rejected' % label)
        return True
    print('  CONTROL FAILED: %s was accepted' % label)
    return False


def plain_crc(name):
    return binascii.crc32(name.encode('latin-1'))


def no_digit_rule(name):
    crc = MASK32
    for byte in name.encode('latin-1'):
        crc = (crc >> 8) ^ CRC_TABLE[(crc ^ symbol_byte(byte)) & 0xFF]
    return crc ^ MASK32


def no_symbol_rule(name):
    data = name.upper().encode('latin-1')
    crc = DIGIT_START if 0x30 <= data[0] <= 0x39 else MASK32
    for byte in data:
        crc = (crc >> 8) ^ CRC_TABLE[(crc ^ byte) & 0xFF]
    return crc ^ MASK32


def all_entries(disc):
    return [(archive, arc.read_toc(disc, archive)) for archive in disc.archives()]


def check_hash(tocs):
    entries = [e for _, toc in tocs for e in toc]
    good = report('name hash', sum(name_hash(e.name) == e.name_hash for e in entries),
                  len(entries))
    for label, function in (('plain CRC-32', plain_crc), ('no digit rule', no_digit_rule),
                            ('no symbol rule', no_symbol_rule)):
        wrong = sum(function(e.name) != e.name_hash for e in entries)
        print('  control: %s misses %d' % (label, wrong))
        good = good and wrong > 0
    return good


def check_index(disc, tocs):
    by_name = {a.path[5:-4]: toc for a, toc in tocs}
    good = passed = total = 0
    seen = set()
    records = []
    for path in INDEX_FILES:
        index_file = disc.find(path)
        records += read_index(disc.read(index_file.lba, 0, index_file.size), path)
    for record in records:
        toc = by_name.get(archive_for_index(record.name))
        total += 1
        if toc is None:
            continue
        seen.add(archive_for_index(record.name))
        wanted = tuple((e.name_hash, e.offset, e.size) for e in toc)
        found = tuple((h, p, s) for h, (p, s) in zip(record.hashes, record.places))
        passed += wanted == found
    good = report('index records equal archive tables', passed, total,
                  'archives without a record: %s' % sorted(set(by_name) - seen))
    samples = next(r for r in records if r.name == SAMPLE_INDEX_NAME)
    rates = sample_rates(samples)
    toc = {e.name_hash: e for e in by_name['SAMPLES']}
    fit = sum(toc[h].size - (x & 0xFFFFFF) in (64, 80, 96, 112) and (x >> 24 & 0x7F) < len(rates)
              for h, x in zip(samples.hashes, samples.extras))
    good = report('sample words', fit, len(samples.hashes), 'rates %s' % rates) and good
    broken = bytearray(disc.read(disc.find(INDEX_FILES[1]).lba, 0, disc.find(INDEX_FILES[1]).size))
    broken[4] ^= 1
    return expect_failure('an index with a bad offset',
                          lambda: read_index(bytes(broken), 'broken')) and good


def read_entries(disc, archive_name):
    archive = arc.find_archive(disc, archive_name)
    for entry in arc.read_toc(disc, archive):
        yield entry, disc.read(archive.lba, entry.offset, entry.size)


def check_datasets(disc, tocs):
    names = {a.path[5:-4]: {e.name_hash: e for e in toc} for a, toc in tocs}
    archives = {a.path[5:-4]: a for a, _ in tocs}
    passed = total = same = items = different = absent = 0
    kinds = collections.Counter()
    sample = None
    for entry, data in read_entries(disc, 'DATASETS'):
        total += 1
        _name, contents = read_dataset(data, entry.name)
        passed += 1
        sample = sample or data
        for item in contents:
            items += 1
            kinds[item.kind] += 1
            source = names.get(archive_for_index(item.kind), {}).get(item.name_hash)
            if source is None:
                absent += 1
            elif source.size == item.size and disc.read(
                    archives[archive_for_index(item.kind)].lba, source.offset, source.size) \
                    == data[item.offset:item.offset + item.size]:
                same += 1
            else:
                different += 1
    good = report('datasets to the last byte', passed, total, '%d items' % items)
    print('  copies equal to the archive %d, different %d, not in one %d'
          % (same, different, absent))
    print('  kinds: %s' % dict(kinds.most_common()))
    broken = bytearray(sample)
    count_at = 16 + sample[12] + 4 + DATASET_NAME_SIZE
    broken[count_at] += 1
    return expect_failure('a dataset with a wrong count',
                          lambda: read_dataset(bytes(broken), 'broken')) and good


def check_edith(disc):
    passed = total = trees = instructions = 0
    signatures = collections.Counter()
    sample = None
    for entry, data in read_entries(disc, 'EDITHTRE'):
        total += 1
        found = read_edith(data, entry.name)
        passed += 1
        sample = sample or data
        trees += len(found)
        instructions += sum(len(tree[3]) for tree in found)
        signatures.update(tree[2] for tree in found)
    good = report('Edith tree sets to the last byte', passed, total,
                  '%d BHAVs, %d instructions, signatures %s'
                  % (trees, instructions, {hex(k): v for k, v in signatures.items()}))
    broken = sample[:-1]
    return expect_failure('a tree set one byte short',
                          lambda: read_edith(broken, 'broken')) and good


def check_models(disc):
    passed = total = 0
    scales = collections.Counter()
    infos = {}
    for entry, data in read_entries(disc, 'MODELS'):
        total += 1
        info = read_model_info(data, entry.name)
        infos[entry.name_hash] = info
        if is_power_of_two(info.scale):
            passed += 1
            scales[math.frexp(info.scale)[1] - 1] += 1
    print('  model scale exponents: %s' % sorted(scales.items()))
    return report('models with a power-of-two scale', passed, total), infos


def union(boxes):
    return tuple(min(b[i] for b in boxes) for i in range(3)) + \
        tuple(max(b[i] for b in boxes) for i in range(3, 6))


def box_error(level, info, factor):
    """How far the lot's own bounds are from the model's box times its scale, in metres."""
    own = union([s.box for s in level.submodels])
    return max(abs(a - b * info.scale * factor) for a, b in zip(own, info.box))


def check_levels(disc, models, names):
    passed = total = placed = resolved = fits = 0
    wrong_fits = 0
    sample = None
    for entry, data in read_entries(disc, 'LEVELS'):
        total += 1
        level = read_level(data, entry.name)
        passed += level.name == entry.name
        sample = sample or data
        placed += len(level.objects)
        resolved += sum(o.model_hash in names for o in level.objects)
        info = models[level.submodels[0].model_hash]
        fits += box_error(level, info, 1) < 0.01
        wrong_fits += box_error(level, info, 2) < 0.01
    good = report('lots to the last byte', passed, total, '%d placed objects' % placed)
    good = report('placed objects that name a resource', resolved, placed) and good
    good = report('lot bounds equal model box * scale', fits, total) and good
    print('  control: with twice the scale, %d fit' % wrong_fits)
    broken = bytearray(sample)
    broken[len(broken) - 9] ^= 1
    return expect_failure('a lot with a bad end mark',
                          lambda: read_level(bytes(broken), 'broken')) and good and wrong_fits == 0


def check_quickdata(disc):
    archive = arc.find_archive(disc, 'QUICKDAT')
    passed = total = 0
    sample = None
    for entry in arc.read_toc(disc, archive):
        data = disc.read(archive.lba, entry.offset, entry.size)
        sample = sample or data
        total += 1
        first = Quickdata(data, entry.name, 0)
        for language in range(first.language_count):
            image = Quickdata(data, entry.name, language)
            for table in image.tables:
                for index in range(table.count if table.names else 0):
                    image.row_name(table, index)
        print('  %-12s %d tables, %d pointers, %d languages'
              % (entry.name, len(first.tables), len(first.pointers), first.language_count))
        passed += 1
    good = report('quickdata images', passed, total)
    broken = bytearray(sample)
    struct.pack_into('<I', broken, struct.unpack_from('<I', broken, 12)[0], 0x7FFFFFF0)
    return expect_failure('quickdata with a wild pointer',
                          lambda: Quickdata(bytes(broken), 'broken')) and good


def obj_definition_guids(data):
    table = next(t for t in data.tables if t.name == 'ObjDefinition')
    return {data.word(table.rows + i * table.stride + OBJD_GUID) for i in range(table.count)}


def check_ngh(disc, quickdata):
    disc_file = disc.find(NGH_FILE)
    sections = read_ngh_sections(disc.read(disc_file.lba, 0, disc_file.size))
    houses = [s for s in sections if s.kind == NGH_KIND_HOUSE]
    guids = obj_definition_guids(quickdata['SimsObjects'])
    types = known = wrong = 0
    names = collections.Counter()
    for house in houses:
        for kind, _chunk_id, data in house_chunks(house):
            if kind == 'objt':
                for guid, _fields, name in read_objt(data):
                    types += 1
                    known += guid in guids
                    wrong += (guid ^ 1) in guids
                    names[name] += 1
    good = report('NGH house sections', len(houses), len(houses),
                  '%d sections in all' % len(sections))
    report('objt GUIDs found in ObjDefinition', known, types, '%d names' % len(names))
    good = good and types - known <= STALE_OBJT_GUIDS
    print('  control: with one GUID bit flipped, %d are found' % wrong)
    return expect_failure('LZSS asked for one byte more',
                          lambda: lzss_decode(b'\xff' + b'a' * 8, 9)) and good


def check_clothing(quickdata, names, texture_names):
    data = quickdata['CreateASim']
    table = next(t for t in data.tables if t.name == 'Sim::SimPartsTSC6')
    models = resolved = sleeves = sleeve_total = textures = texture_total = 0
    for row in range(table.count):
        base = table.rows + row * table.stride
        for slot, part in enumerate(SIM_PARTS):
            for item in geometry_sets(data, base + 4 * slot, names):
                used = [m for m in item['models'] if m != '-']
                models += len(used)
                resolved += sum(not m.startswith('0x') for m in used)
                if part == 'torso':
                    sleeve_total += len(used)
                    sleeves += sum(code in m for m, code in zip(item['models'], TORSO_STYLES)
                                   if m != '-')
                if part in ('torso', 'lowerBody'):
                    for model in used:
                        for variation in item['textures']:
                            texture_total += 1
                            textures += '%s_%s_1' % (model, variation) in texture_names
    good = report('clothing model ids that resolve', resolved, models)
    good = report('torso model order ti ti slti slti', sleeves, sleeve_total) and good
    report('torso and leg textures, state 1', textures, texture_total, '(a survey)')
    return good


def model_sizes(models):
    """How many models fill the 16-bit range, which is what a chosen scale would do."""
    filled = total = 0
    for info in models.values():
        widest = max(abs(v) for v in info.box)
        if widest and info.scale != CHARACTER_SCALE:
            total += 1
            filled += 16384 <= widest <= 32768
    print('  models not at 1/4096 whose largest coordinate is 16384 to 32768: %d of %d'
          % (filled, total))


def check(disc):
    tocs = all_entries(disc)
    names = {e.name_hash: e.name for _, toc in tocs for e in toc}
    results = [check_hash(tocs), check_index(disc, tocs), check_datasets(disc, tocs),
               check_edith(disc)]
    models_good, models = check_models(disc)
    model_sizes(models)
    results += [models_good, check_levels(disc, models, names), check_quickdata(disc)]
    quickdata = load_quickdata(disc)
    textures = {e.name for a, toc in tocs if a.path.endswith('TEXTURES.ARC') for e in toc}
    results += [check_clothing(quickdata, names, textures), check_ngh(disc, quickdata)]
    print('all checks pass' if all(results) else 'SOME CHECKS FAILED')
    return 0 if all(results) else 1


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) in (4, 5) and argv[1] == 'dump':
        language = int(argv[4]) if len(argv) == 5 else QD_ENGLISH
        return dump(arc.Disc(argv[2]), argv[3], language)
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (FormatError, OSError, struct.error, IndexError, ValueError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
