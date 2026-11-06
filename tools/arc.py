#!/usr/bin/env python3
"""Read the game's .ARC archives straight from your own disc image.

Usage:
  python -I tools/arc.py survey  <game.iso>
  python -I tools/arc.py list    <game.iso> <ARCHIVE>
  python -I tools/arc.py extract <game.iso> <ARCHIVE> <out_dir> [name_substring]

ARCHIVE is a name like TEXTURES, without the folder or extension. Extracted files are
game data: keep them out of the repo. docs/ASSET_FORMATS.md describes the format.
"""
import collections
import os
import struct
import sys
from dataclasses import dataclass

SECTOR = 2048
PVD_LBA = 16
ARC_HEADER_SIZE = 16
ARC_VERSION = 0x10000
ENTRY_FIXED_SIZE = 12
FILETIME_SIZE = 8
MAX_NAME = 256
MAGIC_SIZE = 4
TOP_MAGICS = 6


class FormatError(Exception):
    """The disc or archive isn't laid out the way this tool expects."""


@dataclass(frozen=True)
class DiscFile:
    path: str
    lba: int
    size: int


@dataclass(frozen=True)
class ArcEntry:
    name: str
    name_hash: int
    offset: int
    size: int
    filetime: int


class Disc:
    """An ISO9660 image, read in place."""

    def __init__(self, path):
        self.handle = open(path, 'rb')
        self.image_size = os.path.getsize(path)
        self.files = self._read_files()

    def read(self, lba, offset, size):
        start = lba * SECTOR + offset
        if start < 0 or size < 0 or start + size > self.image_size:
            raise FormatError('read past the end of the image at sector %d' % lba)
        self.handle.seek(start)
        return self.handle.read(size)

    def _read_files(self):
        descriptor = self.read(PVD_LBA, 0, SECTOR)
        if descriptor[1:6] != b'CD001':
            raise FormatError('no ISO9660 volume descriptor; is this a disc image?')
        root_lba, root_size = _directory_extent(descriptor, 156)
        files = []
        self._walk(root_lba, root_size, '', files, set())
        return files

    def _walk(self, lba, size, prefix, files, seen):
        if lba in seen:
            raise FormatError('directory loop at sector %d' % lba)
        seen.add(lba)
        data = self.read(lba, 0, size)
        pos = 0
        while pos < len(data):
            length = data[pos]
            if length == 0:  # records never cross a sector, so skip to the next one
                pos = (pos // SECTOR + 1) * SECTOR
                continue
            if length < 34 or pos + length > len(data):
                raise FormatError('bad directory record at sector %d' % lba)
            child_lba, child_size = _directory_extent(data, pos)
            flags, name_size = data[pos + 25], data[pos + 32]
            raw = data[pos + 33:pos + 33 + name_size]
            pos += length
            if raw in (b'\0', b'\1'):  # "." and ".."
                continue
            name = raw.decode('latin-1').split(';')[0]
            if flags & 2:
                self._walk(child_lba, child_size, prefix + name + '/', files, seen)
            else:
                files.append(DiscFile(prefix + name, child_lba, child_size))

    def find(self, path):
        for disc_file in self.files:
            if disc_file.path.upper() == path.upper():
                return disc_file
        raise FormatError('%s is not on this disc' % path)

    def archives(self):
        return sorted((f for f in self.files if f.path.upper().endswith('.ARC')),
                      key=lambda f: f.path)


def _directory_extent(data, pos):
    return struct.unpack_from('<I', data, pos + 2)[0], struct.unpack_from('<I', data, pos + 10)[0]


def read_toc(disc, archive):
    """Return an archive's entries in table order; an empty archive has none."""
    if archive.size < ARC_HEADER_SIZE:
        raise FormatError('%s is smaller than an archive header' % archive.path)
    toc_offset, version = struct.unpack('<II', disc.read(archive.lba, 0, 8))
    if version != ARC_VERSION:
        raise FormatError('%s has unknown version %#x' % (archive.path, version))
    if toc_offset == 0:
        return []
    if toc_offset + 4 > archive.size:
        raise FormatError('%s has its table past the end of the file' % archive.path)
    # The table has no stored length, so read to the end of the file; a table at the
    # front would drag in everything after it, so cap that at what the count can need.
    count = struct.unpack('<I', disc.read(archive.lba, toc_offset, 4))[0]
    most = count * (ENTRY_FIXED_SIZE + MAX_NAME + FILETIME_SIZE)
    table = disc.read(archive.lba, toc_offset + 4, min(most, archive.size - toc_offset - 4))
    return _parse_entries(table, count, archive)


def _parse_entries(table, count, archive):
    entries = []
    pos = 0
    for _ in range(count):
        if pos + ENTRY_FIXED_SIZE > len(table):
            raise FormatError('%s has a truncated table' % archive.path)
        name_hash, offset, size = struct.unpack_from('<III', table, pos)
        pos += ENTRY_FIXED_SIZE
        end = table.find(b'\0', pos, pos + MAX_NAME)
        if end < 0 or end + 1 + FILETIME_SIZE > len(table):
            raise FormatError('%s has an unterminated name' % archive.path)
        name = table[pos:end].decode('latin-1')
        filetime = struct.unpack_from('<Q', table, end + 1)[0]
        pos = end + 1 + FILETIME_SIZE
        if offset + size > archive.size:
            raise FormatError('%s: %s runs past the end of the file' % (archive.path, name))
        entries.append(ArcEntry(name, name_hash, offset, size, filetime))
    return entries


def layout_problems(entries):
    """Return overlaps between entries, which would mean the table was misread."""
    problems = []
    previous = None
    for entry in sorted(entries, key=lambda e: e.offset):
        if previous and entry.offset < previous.offset + previous.size:
            problems.append('%s overlaps %s' % (entry.name, previous.name))
        previous = entry
    return problems


def is_sorted_by_hash(entries):
    hashes = [e.name_hash for e in entries]
    return hashes == sorted(hashes)


def magic_label(data):
    if len(data) >= MAGIC_SIZE and all(32 <= c < 127 for c in data[:MAGIC_SIZE]):
        return repr(data[:MAGIC_SIZE].decode('ascii'))
    return data[:MAGIC_SIZE].hex()


def survey(disc):
    for archive in disc.archives():
        entries = read_toc(disc, archive)
        used = sum(e.size for e in entries)
        problems = layout_problems(entries)
        magics = collections.Counter(
            magic_label(disc.read(archive.lba, e.offset, min(MAGIC_SIZE, e.size)))
            for e in entries)
        names = len({e.name for e in entries})
        print('%-22s %6d files %11d bytes (%5.1f%% of file)  unique names %6d  hash-sorted %s'
              % (archive.path, len(entries), used, 100 * used / archive.size, names,
                 is_sorted_by_hash(entries)))
        print('    magics:', ', '.join('%s x%d' % m for m in magics.most_common(TOP_MAGICS)))
        for problem in problems[:3]:
            print('    PROBLEM:', problem)
    return 0


def find_archive(disc, name):
    return disc.find('DATA/%s.ARC' % name.upper())


def list_archive(disc, name):
    for entry in read_toc(disc, find_archive(disc, name)):
        print('%08x %10d %10d %s' % (entry.name_hash, entry.offset, entry.size, entry.name))
    return 0


def safe_file_name(name):
    """Keep a table name from escaping the output folder."""
    cleaned = ''.join(c if c.isalnum() or c in '._-' else '_' for c in name).lstrip('.')
    if not cleaned:
        raise FormatError('entry name %r is unusable' % name)
    return cleaned


def extract(disc, name, out_dir, wanted):
    archive = find_archive(disc, name)
    os.makedirs(out_dir, exist_ok=True)
    written = 0
    for entry in read_toc(disc, archive):
        if wanted and wanted not in entry.name:
            continue
        # Names repeat across kinds of asset, so the hash keeps the outputs apart.
        target = '%s.%08x' % (safe_file_name(entry.name), entry.name_hash)
        with open(os.path.join(out_dir, target), 'wb') as out:
            out.write(disc.read(archive.lba, entry.offset, entry.size))
        written += 1
    print('wrote %d files to %s' % (written, out_dir))
    return 0


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    command, disc = argv[1], Disc(argv[2])
    if command == 'survey':
        return survey(disc)
    if command == 'list' and len(argv) == 4:
        return list_archive(disc, argv[3])
    if command == 'extract' and len(argv) in (5, 6):
        return extract(disc, argv[3], argv[4], argv[5] if len(argv) == 6 else None)
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
