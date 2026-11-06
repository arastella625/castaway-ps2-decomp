#!/usr/bin/env python3
"""Read the game's CHRC skeletons from your own disc image.

Usage:
  python -I tools/skeleton.py check <game.iso>
  python -I tools/skeleton.py list  <game.iso> <character_name>

check reads every skeleton, then tests each model's bone references against its rig. list
prints one skeleton's bones. docs/ASSET_FORMATS.md has the format.
"""
import collections
import os
import struct
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import model as mdl  # noqa: E402

CHARACTER_ARCHIVE = 'CHARACTE'
CHARACTER_VERSION = 9
CHARACTER_TAG = b'CRHC'  # "CHRC", stored as a little-endian integer
PREFIX_SIZE = 16
MATRIX_SIZE = 64
MAX_NAME = 256
TAIL_SIZE = 20
INVERSE_TOLERANCE = 1e-4
ADULT_RIGS = {'af_': 'female_adult', 'am_': 'Male_adult'}


@dataclass(frozen=True)
class Bone:
    name: str
    parent: int
    children: tuple
    position: tuple  # world space, in metres
    rotation: tuple  # x, y, z, w
    identity: bool  # the file's flag: matrix and inverse are the identity
    matrix: tuple
    inverse: tuple


@dataclass(frozen=True)
class Character:
    name: str
    bones: tuple
    centre: tuple
    radius: float


def need(data, pos, size, name):
    if size < 0 or pos + size > len(data):
        raise arc.FormatError('%s is truncated' % name)


def read_bone(data, pos, name):
    need(data, pos, 8, name)
    parent, child_count = struct.unpack_from('<iI', data, pos)
    pos += 8
    need(data, pos, child_count * 4, name)
    children = struct.unpack_from('<%di' % child_count, data, pos)
    pos += child_count * 4
    need(data, pos, 12 + 16 + 1 + 2 * MATRIX_SIZE, name)
    position = struct.unpack_from('<3f', data, pos)
    rotation = struct.unpack_from('<4f', data, pos + 12)
    identity = data[pos + 28] != 0
    pos += 29
    matrix = struct.unpack_from('<16f', data, pos)
    inverse = struct.unpack_from('<16f', data, pos + MATRIX_SIZE)
    pos += 2 * MATRIX_SIZE
    end = data.find(b'\0', pos, pos + MAX_NAME)
    if end < 0:
        raise arc.FormatError('%s has an unterminated bone name' % name)
    bone = Bone(data[pos:end].decode('latin-1'), parent, children, position, rotation,
                identity, matrix, inverse)
    return bone, end + 1


def check_links(bones, name):
    """Parents and children must agree, and a parent must come before its child."""
    for index, bone in enumerate(bones):
        if bone.parent >= index or bone.parent < -1:
            raise arc.FormatError('%s: %s has a bad parent' % (name, bone.name))
        for child in bone.children:
            if not 0 <= child < len(bones) or bones[child].parent != index:
                raise arc.FormatError('%s: %s has a bad child' % (name, bone.name))


def parse_character(data, name='character'):
    """Parse one CHRC file; every byte must be accounted for."""
    need(data, 0, PREFIX_SIZE, name)
    version, tag, _, name_size = struct.unpack_from('<I4siI', data, 0)
    if version != CHARACTER_VERSION or tag != CHARACTER_TAG:
        raise arc.FormatError('%s is not a version 9 CHRC file' % name)
    pos = PREFIX_SIZE + name_size
    need(data, pos, 8, name)
    size, count = struct.unpack_from('<II', data, pos)
    if size != len(data) - pos - 4:
        raise arc.FormatError('%s has a size field that does not match the file' % name)
    pos += 8
    bones = []
    for _ in range(count):
        bone, pos = read_bone(data, pos, name)
        bones.append(bone)
    if pos + TAIL_SIZE != len(data):
        raise arc.FormatError('%s has %d bytes left over' % (name, len(data) - pos))
    check_links(bones, name)
    centre = struct.unpack_from('<3f', data, pos)
    radius = struct.unpack_from('<f', data, pos + 12)[0]
    return Character(name, tuple(bones), centre, radius)


def inverse_error(bone):
    a, b = bone.matrix, bone.inverse
    worst = 0.0
    for row in range(4):
        for col in range(4):
            value = sum(a[row * 4 + k] * b[k * 4 + col] for k in range(4))
            worst = max(worst, abs(value - (row == col)))
    return worst


def read_characters(disc):
    archive = arc.find_archive(disc, CHARACTER_ARCHIVE)
    return {e.name.lower(): parse_character(disc.read(archive.lba, e.offset, e.size), e.name)
            for e in arc.read_toc(disc, archive)}


def rig_for(model_name, characters):
    """Return a model's rig: its same-named file, or the adult rig its prefix names."""
    own = characters.get(model_name.lower())
    if own:
        return own
    adult = ADULT_RIGS.get(model_name[:3])
    return characters.get(adult.lower()) if adult else None


def check(disc):
    characters = read_characters(disc)
    bones = [b for c in characters.values() for b in c.bones]
    print('skeletons %d, bones %d' % (len(characters), len(bones)))
    print('largest error in matrix x inverse: %.1e' % max(inverse_error(b) for b in bones))
    bad_inverse = sum(inverse_error(b) > INVERSE_TOLERANCE for b in bones)
    totals = collections.Counter()
    failures = []
    _, shaders = mdl.shader_table(disc)
    for entry, data in mdl.each_model(disc, shaders):
        meshes = mdl.parse_model(data, shaders, entry.name)
        batches = [b for m in meshes for b in m.batches]
        used = [bone for b in batches for bone in mdl.bones_used(b)]
        if not used:
            totals['models with no bone references'] += 1
            continue
        rig = rig_for(entry.name, characters)
        if rig is None:
            totals['models with no known rig'] += 1
        elif max(used) >= len(rig.bones):
            failures.append('%s uses a bone past the end of %s' % (entry.name, rig.name))
        else:
            totals['models tied to a rig'] += 1
    for key, value in sorted(totals.items()):
        print('%-32s %6d' % (key, value))
    print('bad inverses: %d' % bad_inverse)
    print('failures: %d' % len(failures))
    for failure in failures[:20]:
        print('   ', failure)
    return 1 if failures or bad_inverse else 0


def list_bones(disc, name):
    character = read_characters(disc).get(name.lower())
    if character is None:
        raise arc.FormatError('%s is not in %s' % (name, CHARACTER_ARCHIVE))
    for index, bone in enumerate(character.bones):
        print('%3d parent %3d  %7.3f %7.3f %7.3f  %s' % ((index, bone.parent) + bone.position
                                                         + (bone.name,)))
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 4 and argv[1] == 'list':
        return list_bones(arc.Disc(argv[2]), argv[3])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
