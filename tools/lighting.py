#!/usr/bin/env python3
"""Read the lights the game sets through the day, from QuickData's world/LevelLightingSequence.

Usage:
  python -I tools/lighting.py check  <game.iso>
  python -I tools/lighting.py export <game.iso> <out.json>

Each sequence (DarkCave, Default, TempleInterior) is a VECTOR of Level::LightingEntry, keyed by
the hour. ERLevel::OutsideLightsUpdate builds the sun's direction from the entry's angles, and
CalcSkyLights turns the entry's sky lights into the one directional light and ambient that
EIStaticSubModel::Draw lights level geometry with. The export is game data: keep it out of the
repo. docs/formats/world.md has what's known.
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402
import gamedata as gd  # noqa: E402

TABLE = 'world/LevelLightingSequence'
ENTRY_SIZE = 0x50
COLOURS = {'ambient': 0x04, 'fog': 0x10, 'sun': 0x24, 'moon': 0x34, 'sky': 0x3C, 'sky_ambient': 0x44}
FLOATS = {'hour': 0x00, 'ambient_intensity': 0x08, 'sun_elevation': 0x1C, 'sun_yaw': 0x20,
          'sun_intensity': 0x28, 'moon_elevation': 0x2C, 'moon_yaw': 0x30, 'moon_intensity': 0x38,
          'sky_intensity': 0x40, 'sky_ambient_intensity': 0x48}


def read_entry(data, pos):
    def number(offset):
        return struct.unpack('<f', struct.pack('<I', data.word(pos + offset)))[0]

    def colour(offset):
        if pos + offset not in data.pointers:
            raise arc.FormatError('lighting: no colour at +%#x of an entry' % offset)
        return [number_at(data, data.word(pos + offset) + 4 * k) for k in range(3)]

    entry = {key: number(offset) for key, offset in FLOATS.items()}
    entry.update({key: colour(offset) for key, offset in COLOURS.items()})
    return entry


def number_at(data, pos):
    return struct.unpack('<f', struct.pack('<I', data.word(pos)))[0]


def read_sequences(disc, entry_size=ENTRY_SIZE):
    """Return {name: [entry]}, each entry the floats and RGB colours of a Level::LightingEntry."""
    data, table = gd.find_table(gd.load_quickdata(disc), TABLE)
    sequences = {}
    for index in range(table.count):
        start, count = data.vector(table.rows + index * table.stride)
        entries = [read_entry(data, start + item * entry_size) for item in range(count)]
        hours = [entry['hour'] for entry in entries]
        if hours != sorted(hours) or not all(0.0 <= hour < 24.0 for hour in hours):
            raise arc.FormatError('lighting: hours out of order in %s' % data.row_name(table, index))
        sequences[data.row_name(table, index)] = entries
    return sequences


def check(disc):
    sequences = read_sequences(disc)
    for name, entries in sequences.items():
        print('%-15s %2d entries, hours %s' % (name, len(entries), ' '.join('%g' % e['hour'] for e in entries)))
    noon = next(e for e in sequences['Default'] if e['hour'] == 12.0)
    print('Default at 12:00: sun elevation %g, yaw %g; sky %s x %g; sky ambient %s x %g' % (
        noon['sun_elevation'], noon['sun_yaw'], noon['sky'], noon['sky_intensity'],
        noon['sky_ambient'], noon['sky_ambient_intensity']))
    print('controls:')
    passed = gd.expect_failure('entries read 4 bytes short', lambda: read_sequences(disc, ENTRY_SIZE - 4))
    passed &= gd.expect_failure('entries read 4 bytes long', lambda: read_sequences(disc, ENTRY_SIZE + 4))
    print('failures: %d' % (not passed))
    return 0 if passed else 1


def export(disc, out_path):
    with open(out_path, 'w') as out:
        json.dump(read_sequences(disc), out, indent=1)
    print('wrote %s' % out_path)
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 4 and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        sys.exit(1)
