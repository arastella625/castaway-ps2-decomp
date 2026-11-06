#!/usr/bin/env python3
"""Decode the game's ANIM animations from your own disc image.

Usage:
  python -I tools/anim.py check <game.iso>
  python -I tools/anim.py list  <game.iso> <name_substring>

check decodes every channel of every animation and reports anything that doesn't fit.
docs/ASSET_FORMATS.md has the format.
"""
import bisect
import collections
import math
import os
import struct
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402

ANIM_ARCHIVE = 'ANIMATIO'
ANIM_VERSION = 22
ANIM_TAG = b'MINA'  # "ANIM", stored as a little-endian integer
PREFIX_SIZE = 16
NODE_FIELDS = 8
ROTATION, SCALE, POSITION, VISIBILITY = 0, 1, 2, 3
EVENT_SIZE = 80
MAX_NOTE = 256
IDENTITY = (0.0, 0.0, 0.0, 1.0)
RAW_BITS = 32  # a width of 0 in a stream means raw 32-bit floats
TWO_PI = 2 * math.pi


class Bits:
    """A bit stream, read from the low bit of each byte upward."""

    def __init__(self, data):
        self.data = data
        self.size = len(data) * 8

    def get(self, pos, count):
        if count == 0:
            return 0
        if pos < 0 or pos + count > self.size:
            raise arc.FormatError('a stream reads past the end of its bits')
        chunk = self.data[pos >> 3:(pos + count + 7) >> 3]
        return (int.from_bytes(chunk, 'little') >> (pos & 7)) & ((1 << count) - 1)

    def signed(self, pos, count):
        value = self.get(pos, count)
        return value - (1 << count) if count and value >> (count - 1) else value

    def real(self, pos):
        return struct.unpack('<f', struct.pack('<I', self.get(pos, 32)))[0]


def signed_scale(bits):
    return 1.0 / ((1 << (bits - 1)) - 1) if bits > 1 else 0.0


def unsigned_scale(bits):
    return 1.0 if bits == RAW_BITS else 1.0 / ((1 << bits) - 1)


def nlerp(a, b, amount):
    """Blend two quaternions in a straight line, then normalise, as the game does."""
    mixed = [a[i] + amount * (b[i] - a[i]) for i in range(4)]
    length = math.sqrt(sum(c * c for c in mixed))
    return tuple(c / length for c in mixed) if length else IDENTITY


def hermite(u):
    """Return the weights for start, start tangent, end and end tangent."""
    u2, u3 = u * u, u * u * u
    return 2 * u3 - 3 * u2 + 1, u3 - 2 * u2 + u, -2 * u3 + 3 * u2, u3 - u2


class Track:
    """Keys at whole frames; subclasses read them and blend between them."""

    def __init__(self, frames, biases):
        if not frames:
            raise arc.FormatError('a stream has no keys')
        self.frames = frames
        self.biases = biases
        self.count = len(frames)

    def segment(self, frame):
        """Return the key before a frame, and how far through the gap it is."""
        index = bisect.bisect_right(self.frames, frame) - 1
        index = min(max(index, 0), self.count - 2)
        span = self.frames[index + 1] - self.frames[index]
        return index, min(max((frame - self.frames[index]) / span, 0.0), 1.0)

    def timing(self, which, index):
        """Return a tangent's share of the time around a key."""
        before = self.frames[index] - self.frames[index - 1]
        after = self.frames[index + 1] - self.frames[index]
        return (after if which else before) / (before + after)


class RotationTrack(Track):
    """Full rotations. Curved segments are Bezier curves through four quaternions."""

    def __init__(self, bits, pos):
        count, time_bits = bits.get(pos, 20), bits.get(pos + 20, 5)
        bias_bits = bits.get(pos + 25, 5)
        value_bits = bits.get(pos + 30, 5) or RAW_BITS
        stride = time_bits + bias_bits + 3 * value_bits + 2
        start = pos + 35
        frames, biases, self.flags, self.values = [], [], [], []
        frame = 0
        self.total = 0
        for index in range(count):
            key = start + index * stride
            delta = bits.get(key, time_bits)
            self.total += delta + 1
            frame += delta + 1 if index else 0
            frames.append(frame)
            biases.append(bits.signed(key + time_bits, bias_bits) * signed_scale(bias_bits))
            self.flags.append(bits.get(key + time_bits + bias_bits, 1))
            self.values.append(read_quaternion(bits, key + time_bits + bias_bits + 1, value_bits))
        super().__init__(frames, biases)

    def control(self, which, i):
        """Return the inner control point: which is 1 leaving a key, 0 arriving at it."""
        q, bias = self.values[i], self.biases[i]
        if i == 0:
            return nlerp(q, self.values[1], 0.5 * (1 + bias))
        if i == self.count - 1:
            return nlerp(q, self.values[i - 1], 0.5 * (1 - bias))
        before = nlerp(q, self.values[i - 1], -0.5 * (1 + bias))
        after = nlerp(q, self.values[i + 1], 0.5 * (1 - bias))
        direction = 1.0 if which else -1.0
        return nlerp(q, nlerp(before, after, 0.5), 2 * direction * self.timing(which, i))

    def sample(self, frame):
        if self.count == 1:
            return self.values[0]
        i, u = self.segment(frame)
        p0, p3 = self.values[i], self.values[i + 1]
        if not (self.flags[i] or self.flags[i + 1]):
            return nlerp(p0, p3, u)
        p1, p2 = self.control(1, i), self.control(0, i + 1)
        lerp = lambda a, b: [a[k] + u * (b[k] - a[k]) for k in range(4)]  # noqa: E731
        a, b, c = lerp(p0, p1), lerp(p1, p2), lerp(p2, p3)
        return nlerp(lerp(a, b), lerp(b, c), u)


def read_quaternion(bits, pos, value_bits):
    """Read x, y and z, then rebuild w from unit length; one bit holds its sign."""
    if value_bits == RAW_BITS:
        xyz = [bits.real(pos + 32 * i) for i in range(3)]
    else:
        scale = signed_scale(value_bits)
        xyz = [bits.signed(pos + value_bits * i, value_bits) * scale for i in range(3)]
    remainder = 1 - sum(c * c for c in xyz)
    w = math.sqrt(remainder) if remainder > 0 else 0.0
    return (xyz[0], xyz[1], xyz[2], -w if bits.get(pos + 3 * value_bits, 1) else w)


def near(angle, target):
    """Move an angle by whole turns to within half a turn of a target."""
    if not math.isfinite(angle) or not math.isfinite(target):
        raise arc.FormatError('a stream holds an angle that is not a number')
    return target + math.remainder(angle - target, TWO_PI)


class AxisTrack(Track):
    """Rotation about one fixed axis: one angle per key, on a Hermite curve."""

    def __init__(self, bits, pos):
        self.axis = [bits.real(pos + 32 * i) for i in range(3)]
        count, time_bits = bits.get(pos + 96, 20), bits.get(pos + 116, 5)
        bias_bits = bits.get(pos + 121, 5)
        value_bits = bits.get(pos + 126, 5) or RAW_BITS
        scale = bits.real(pos + 131) * unsigned_scale(value_bits)
        base = bits.real(pos + 163)
        if value_bits == RAW_BITS:
            scale, base = 1.0, 0.0
        stride = time_bits + bias_bits + value_bits
        start = pos + 195
        frames, biases, self.values = [], [], []
        frame = 0
        self.total = 0
        for index in range(count):
            key = start + index * stride
            delta = bits.get(key, time_bits)
            self.total += delta + 1
            frame += delta + 1 if index else 0
            frames.append(frame)
            biases.append(bits.signed(key + time_bits, bias_bits) * signed_scale(bias_bits))
            value = key + time_bits + bias_bits
            self.values.append(bits.real(value) if value_bits == RAW_BITS
                               else base + bits.get(value, value_bits) * scale)
        super().__init__(frames, biases)

    def tangent(self, which, i):
        a = self.values[i]
        if i == 0 or i == self.count - 1:
            other = 1 if i == 0 else i - 1
            slope = near(self.values[other], a) - a if i == 0 else a - near(self.values[other], a)
            if self.count == 2:
                return slope
            return 1.5 * slope - 0.5 * self.tangent(0 if i == 0 else 1, other)
        before = a - near(self.values[i - 1], a)
        after = near(self.values[i + 1], a) - a
        bias = self.biases[i]
        return self.timing(which, i) * ((1 + bias) * before + (1 - bias) * after)

    def sample(self, frame):
        if self.count == 1:
            angle = self.values[0]
        else:
            i, u = self.segment(frame)
            start = near(self.values[i], 0.0)
            end = near(near(self.values[i + 1], 0.0), start)
            leaving, arriving = self.tangent(1, i), self.tangent(0, i + 1)
            if abs(leaving - arriving) == math.pi:  # the game's own special case
                leaving = arriving
            h0, h1, h2, h3 = hermite(u)
            angle = (h0 * start + h1 * near(leaving, start) + h2 * end
                     + h3 * near(arriving, start))
        half = 0.5 * angle
        s = math.sin(half)
        return (self.axis[0] * s, self.axis[1] * s, self.axis[2] * s, math.cos(half))


class VectorTrack(Track):
    """Three values per key, with a range per axis; linear or Hermite segments."""

    def __init__(self, bits, pos):
        count, time_bits = bits.get(pos, 20), bits.get(pos + 20, 5)
        bias_bits = bits.get(pos + 25, 5)
        value_bits = bits.get(pos + 30, 5) or RAW_BITS
        scale = [bits.real(pos + 35 + 64 * i) * unsigned_scale(value_bits) for i in range(3)]
        low = [bits.real(pos + 67 + 64 * i) for i in range(3)]
        stride = time_bits + bias_bits + 3 * value_bits + 1
        start = pos + 227
        frames, biases, self.flags, self.values = [], [], [], []
        frame = 0
        self.total = 0
        for index in range(count):
            key = start + index * stride
            delta = bits.get(key, time_bits)
            self.total += delta + 1
            frame += delta + 1 if index else 0
            frames.append(frame)
            biases.append(bits.signed(key + time_bits, bias_bits) * signed_scale(bias_bits))
            self.flags.append(bits.get(key + time_bits + bias_bits, 1))
            value = key + time_bits + bias_bits + 1
            if value_bits == RAW_BITS:
                self.values.append(tuple(bits.real(value + 32 * k) for k in range(3)))
            else:
                self.values.append(tuple(low[k] + scale[k] * bits.get(value + value_bits * k,
                                                                      value_bits)
                                         for k in range(3)))
        super().__init__(frames, biases)

    def tangent(self, which, i):
        v = self.values[i]
        if i == 0 or i == self.count - 1:
            other = 1 if i == 0 else i - 1
            slope = [(self.values[other][k] - v[k]) if i == 0 else (v[k] - self.values[other][k])
                     for k in range(3)]
            if self.count == 2:
                return slope
            neighbour = self.tangent(0 if i == 0 else 1, other)
            return [1.5 * slope[k] - 0.5 * neighbour[k] for k in range(3)]
        share = self.timing(which, i)
        bias = self.biases[i]
        return [share * ((1 + bias) * (v[k] - self.values[i - 1][k])
                         + (1 - bias) * (self.values[i + 1][k] - v[k])) for k in range(3)]

    def sample(self, frame):
        if self.count == 1:
            return self.values[0]
        i, u = self.segment(frame)
        start, end = self.values[i], self.values[i + 1]
        if not (self.flags[i] or self.flags[i + 1]):
            return tuple(start[k] + u * (end[k] - start[k]) for k in range(3))
        t0, t1 = self.tangent(1, i), self.tangent(0, i + 1)
        h0, h1, h2, h3 = hermite(u)
        return tuple(h0 * start[k] + h1 * t0[k] + h2 * end[k] + h3 * t1[k] for k in range(3))


@dataclass
class Animation:
    name: str
    frames: int
    nodes: list
    floats: tuple
    bits: Bits

    def constant(self, index, size):
        if index + size > len(self.floats):
            raise arc.FormatError('%s: a constant is past its float table' % self.name)
        return self.floats[index:index + size]

    def rotation_track(self, node):
        """Return a sampler for a node's rotation: a track, a constant tuple, or None."""
        value = self.nodes[node][ROTATION]
        if value == 0:
            return None
        if value > 0:
            return self.constant(value, 4)
        kind = self.bits.get(-value, 1)
        return (AxisTrack if kind else RotationTrack)(self.bits, -value + 1)

    def vector_track(self, node, field):
        value = self.nodes[node][field]
        if value == 0:
            return None
        if value > 0:
            return self.constant(value, 3)
        return VectorTrack(self.bits, -value)


def read_counted(data, pos, code, size, name):
    if pos + 4 > len(data):
        raise arc.FormatError('%s is truncated' % name)
    count = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    if pos + count * size > len(data):
        raise arc.FormatError('%s has a table past its end' % name)
    return struct.unpack_from('<%d%s' % (count, code), data, pos), pos + count * size


def skip_tail(data, pos, name):
    """Step over the settings, notes and events; return where the file should end."""
    pos += 12 + 1 + 16 + 2
    if pos + 4 > len(data):
        raise arc.FormatError('%s is truncated' % name)
    notes = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    for _ in range(notes):
        end = data.find(b'\0', pos + 8, pos + 8 + MAX_NOTE)
        if end < 0:
            raise arc.FormatError('%s has an unterminated note' % name)
        pos = end + 1
    if pos + 4 > len(data):
        raise arc.FormatError('%s is truncated' % name)
    events = struct.unpack_from('<I', data, pos)[0]
    return pos + 4 + events * EVENT_SIZE


def parse_animation(data, name='animation'):
    """Parse one ANIM file; every byte must be accounted for."""
    if len(data) < PREFIX_SIZE:
        raise arc.FormatError('%s is too short for a header' % name)
    version, tag, _, name_size = struct.unpack_from('<I4siI', data, 0)
    if version != ANIM_VERSION or tag != ANIM_TAG:
        raise arc.FormatError('%s is not a version 22 ANIM file' % name)
    pos = PREFIX_SIZE + name_size
    if pos + 36 > len(data):
        raise arc.FormatError('%s is truncated' % name)
    size, frames = struct.unpack_from('<II', data, pos)
    if size != len(data) - pos - 4:
        raise arc.FormatError('%s has a size field that does not match the file' % name)
    pos += 8 + 24
    nodes, pos = read_nodes(data, pos, name)
    _, pos = read_counted(data, pos, 'i', 4, name)  # morph channels
    pos += 4
    floats, pos = read_counted(data, pos, 'f', 4, name)
    if pos + 4 > len(data):
        raise arc.FormatError('%s is truncated' % name)
    bit_count = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    word_bytes = (bit_count + 31) // 32 * 4
    if pos + word_bytes > len(data):
        raise arc.FormatError('%s has bits past its end' % name)
    bits = Bits(data[pos:pos + word_bytes])
    if skip_tail(data, pos + word_bytes, name) != len(data):
        raise arc.FormatError('%s does not end where its tables say' % name)
    return Animation(name, frames, nodes, floats, bits)


def read_nodes(data, pos, name):
    if pos + 4 > len(data):
        raise arc.FormatError('%s is truncated' % name)
    count = struct.unpack_from('<I', data, pos)[0]
    pos += 4
    size = 4 * NODE_FIELDS
    if pos + count * size > len(data):
        raise arc.FormatError('%s has nodes past its end' % name)
    nodes = [struct.unpack_from('<%di' % NODE_FIELDS, data, pos + i * size) for i in range(count)]
    return nodes, pos + count * size


def each_animation(disc):
    archive = arc.find_archive(disc, ANIM_ARCHIVE)
    for entry in sorted(arc.read_toc(disc, archive), key=lambda e: e.offset):
        yield entry, disc.read(archive.lba, entry.offset, entry.size)


def worst_step(track, frames):
    """Return the largest change between neighbouring half frames."""
    last = track.sample(0.0)
    worst = 0.0
    for step in range(1, 2 * frames - 1):
        value = track.sample(step / 2)
        if len(value) == 4 and sum(a * b for a, b in zip(value, last)) < 0:
            value = tuple(-c for c in value)
        worst = max(worst, math.dist(value, last))
        last = value
    return worst


def check_track(track, animation, totals):
    if track.total != animation.frames:
        raise arc.FormatError('%s: a stream has the wrong length' % animation.name)
    totals['keys'] += track.count


def check(disc):
    totals = collections.Counter()
    steps = collections.Counter()
    failures = []
    for entry, data in each_animation(disc):
        try:
            animation = parse_animation(data, entry.name)
            totals['animations'] += 1
            totals['frames'] += animation.frames
            for node in range(len(animation.nodes)):
                rotation = animation.rotation_track(node)
                if isinstance(rotation, Track):
                    check_track(rotation, animation, totals)
                    totals[type(rotation).__name__] += 1
                    if totals['animations'] % 25 == 0:
                        steps[round(worst_step(rotation, animation.frames), 1)] += 1
                for field in (POSITION, SCALE):
                    vector = animation.vector_track(node, field)
                    if isinstance(vector, Track):
                        check_track(vector, animation, totals)
                        totals['VectorTrack'] += 1
        except arc.FormatError as error:
            failures.append(str(error))
    for key, value in sorted(totals.items()):
        print('%-16s %9d' % (key, value))
    print('largest half-frame rotation steps, sampled:', sorted(steps.items())[-6:])
    print('failures: %d' % len(failures))
    for failure in failures[:20]:
        print('   ', failure)
    return 1 if failures else 0


def list_animations(disc, wanted):
    for entry, data in each_animation(disc):
        if wanted in entry.name:
            animation = parse_animation(data, entry.name)
            print('%5d frames %3d nodes  %s' % (animation.frames, len(animation.nodes),
                                                entry.name))
    return 0


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 4 and argv[1] == 'list':
        return list_animations(arc.Disc(argv[2]), argv[3])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
