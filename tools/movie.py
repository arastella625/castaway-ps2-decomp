#!/usr/bin/env python3
"""Demux the game's PSS movies (MOVIES.ARC) from your own disc image.

Usage:
  python -I tools/movie.py check  <game.iso>
  python -I tools/movie.py export <game.iso> <out_dir> <name_substring>

check walks every MPEG program stream packet of every movie, reports each stream and tests
that they agree with each other. export writes <name>.m2v, the MPEG-2 video elementary
stream, and <name>.wav, the PCM audio. Both are game data: keep them out of the repo.
docs/ASSET_FORMATS.md has the format.
"""
import collections
import hashlib
import os
import re
import struct
import sys
import wave
from dataclasses import dataclass, field

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402

MOVIE_ARCHIVE = 'MOVIES'
PACK_START, SYSTEM_HEADER, PROGRAM_END = 0xBA, 0xBB, 0xB9
PRIVATE_1, PADDING, VIDEO = 0xBD, 0xBE, 0xE0
PACK_FIXED_SIZE = 14
PACK_SPACING = 0x4000  # every pack in the game's movies starts on a 16 KiB boundary
PTS_CLOCK = 90000
SUBSTREAM_HEADER = bytes.fromhex('ffa00000')  # audio sits behind these 4 bytes in each packet
AUDIO_HEADER_SIZE = 40  # SShd, 24 bytes of fields, SSbd, the data size
AUDIO_CODEC_PCM16 = 1
SEQUENCE_HEADER, EXTENSION, GOP, SEQUENCE_END, USER_DATA = 0xB3, 0xB5, 0xB8, 0xB7, 0xB2
SLICE_FIRST, SLICE_LAST = 0x01, 0xAF
FRAME_RATES = {1: 24000 / 1001, 2: 24.0, 3: 25.0, 4: 30000 / 1001, 5: 30.0, 6: 50.0,
               7: 60000 / 1001, 8: 60.0}
PICTURE_TYPES = {1: 'I', 2: 'P', 3: 'B'}
MACROBLOCK = 16
AUDIO_TAIL_LIMIT = 1.0  # seconds; audio is padded to whole seconds with silence after the video
AUDIO_SHORT_LIMIT = 0.1  # seconds; audio may end this much before the video
PTS_DRIFT_LIMIT = 0.02  # seconds; an audio packet's PTS against its byte position
START_CODE = re.compile(b'\0\0\1(.)', re.DOTALL)


@dataclass
class AudioFormat:
    codec: int
    rate: int
    channels: int
    interleave: int
    loop_start: int
    loop_end: int
    data_size: int


@dataclass
class VideoInfo:
    width: int = 0
    height: int = 0
    aspect: int = 0
    rate_code: int = 0
    bit_rate: int = 0
    mpeg2: bool = False
    progressive: bool = False
    pictures: int = 0
    gops: int = 0
    sequences: int = 0
    types: collections.Counter = field(default_factory=collections.Counter)

    @property
    def frame_rate(self):
        return FRAME_RATES[self.rate_code]

    @property
    def seconds(self):
        return self.pictures / self.frame_rate


@dataclass
class Movie:
    name: str
    packets: collections.Counter = field(default_factory=collections.Counter)
    packs: int = 0
    video: bytearray = field(default_factory=bytearray)
    audio: bytearray = field(default_factory=bytearray)
    video_pts: list = field(default_factory=list)
    audio_pts: list = field(default_factory=list)  # (pts, audio bytes before this packet)


def read_pts(data, pos):
    """Decode the 33-bit timestamp in the 5 bytes at pos, checking its marker bits."""
    b = data[pos:pos + 5]
    if len(b) < 5 or not b[0] & 1 or not b[2] & 1 or not b[4] & 1:
        raise arc.FormatError('bad timestamp at %#x' % pos)
    return (b[0] >> 1 & 7) << 30 | b[1] << 22 | (b[2] >> 1) << 15 | b[3] << 7 | b[4] >> 1


def pes_payload(data, pos, end):
    """Return (pts or None, payload start) for the PES packet whose header is at pos."""
    if pos + 9 > end or data[pos + 6] & 0xC0 != 0x80:
        raise arc.FormatError('not an MPEG-2 PES header at %#x' % pos)
    flags, header_size = data[pos + 7], data[pos + 8]
    start = pos + 9 + header_size
    if start > end:
        raise arc.FormatError('PES header overruns its packet at %#x' % pos)
    pts = read_pts(data, pos + 9) if flags & 0x80 else None
    return pts, start


def take_private(movie, data, pos, end):
    """Add one private stream 1 packet, which holds audio behind a short sub-header."""
    pts, start = pes_payload(data, pos, end)
    head = data[start:min(end, start + len(SUBSTREAM_HEADER))]
    if head != SUBSTREAM_HEADER:
        raise arc.FormatError('unknown private stream %s at %#x' % (head.hex(), pos))
    if pts is not None:
        movie.audio_pts.append((pts, len(movie.audio)))
    movie.audio += data[start + len(SUBSTREAM_HEADER):end]


def take_video(movie, data, pos, end):
    pts, start = pes_payload(data, pos, end)
    if pts is not None:
        movie.video_pts.append(pts)
    movie.video += data[start:end]


def demux(data, name='movie'):
    """Walk every packet; the walk must land on the program end code, the last 4 bytes."""
    movie = Movie(name)
    pos = 0
    while True:
        if pos + 4 > len(data) or data[pos:pos + 3] != b'\0\0\1':
            raise arc.FormatError('%s: lost the packet chain at %#x' % (name, pos))
        stream = data[pos + 3]
        if stream == PROGRAM_END:
            if pos + 4 != len(data):
                raise arc.FormatError('%s: %d bytes after the end code'
                                      % (name, len(data) - pos - 4))
            return movie
        if stream == PACK_START:
            if pos % PACK_SPACING or pos + PACK_FIXED_SIZE > len(data):
                raise arc.FormatError('%s: misplaced pack at %#x' % (name, pos))
            movie.packs += 1
            pos += PACK_FIXED_SIZE + (data[pos + 13] & 7)
            continue
        if pos + 6 > len(data):
            raise arc.FormatError('%s: truncated packet at %#x' % (name, pos))
        end = pos + 6 + struct.unpack_from('>H', data, pos + 4)[0]
        if end > len(data):
            raise arc.FormatError('%s: packet at %#x overruns the file' % (name, pos))
        movie.packets[stream] += 1
        if stream == VIDEO:
            take_video(movie, data, pos, end)
        elif stream == PRIVATE_1:
            take_private(movie, data, pos, end)
        elif stream not in (SYSTEM_HEADER, PADDING):
            raise arc.FormatError('%s: unknown stream %#x at %#x' % (name, stream, pos))
        pos = end


def parse_audio_header(audio, name='movie'):
    """Read the SShd/SSbd header that opens the audio; the sample data follows it."""
    if len(audio) < AUDIO_HEADER_SIZE or audio[0:4] != b'SShd' or audio[32:36] != b'SSbd':
        raise arc.FormatError('%s: audio has no SShd/SSbd header' % name)
    size, codec, rate, channels, interleave, loop_start, loop_end = struct.unpack_from(
        '<IIIIIii', audio, 4)
    if size != 24:
        raise arc.FormatError('%s: SShd size is %d' % (name, size))
    data_size = struct.unpack_from('<I', audio, 36)[0]
    return AudioFormat(codec, rate, channels, interleave, loop_start, loop_end, data_size)


def audio_samples(movie, form):
    """Return the sample bytes, without the header and any padding after them."""
    body = movie.audio[AUDIO_HEADER_SIZE:]
    if form.data_size > len(body):
        raise arc.FormatError('%s: SSbd claims more audio than there is' % movie.name)
    return bytes(body[:form.data_size])


def deinterleave(samples, channels, block):
    """Turn blocks of `block` bytes per channel into frames of one sample per channel."""
    stride = channels * block
    if not block or block % 2 or len(samples) % stride:
        raise arc.FormatError('audio does not divide into interleave blocks')
    out = bytearray(len(samples))
    for channel in range(channels):
        plane = b''.join(samples[i + channel * block:i + (channel + 1) * block]
                         for i in range(0, len(samples), stride))
        for byte in range(2):
            out[channel * 2 + byte::channels * 2] = plane[byte::2]
    return bytes(out)


def scan_sequence_header(video, pos, info):
    if pos + 12 > len(video):
        raise arc.FormatError('truncated sequence header')
    width = video[pos + 4] << 4 | video[pos + 5] >> 4
    height = (video[pos + 5] & 15) << 8 | video[pos + 6]
    rate_code = video[pos + 7] & 15
    if rate_code not in FRAME_RATES:
        raise arc.FormatError('unknown frame rate code %d' % rate_code)
    shape = (width, height, video[pos + 7] >> 4, rate_code)
    if info.sequences and shape != (info.width, info.height, info.aspect, info.rate_code):
        raise arc.FormatError('sequence headers disagree at %#x' % pos)
    info.width, info.height, info.aspect, info.rate_code = shape
    info.bit_rate = (video[pos + 8] << 10 | video[pos + 9] << 2 | video[pos + 10] >> 6) * 400
    info.sequences += 1


def scan_extension(video, pos, info):
    if pos + 10 > len(video):
        raise arc.FormatError('truncated extension')
    kind = video[pos + 4] >> 4
    if kind == 1:  # sequence extension
        info.mpeg2 = True
        info.progressive = bool(video[pos + 5] & 8)
    elif kind == 8 and video[pos + 6] & 3 != 3:  # picture coding extension
        raise arc.FormatError('field picture at %#x; a frame is assumed' % pos)


def scan_video(video, name='movie'):
    """Walk the start codes: each picture must hold one slice per macroblock row, in order."""
    info = VideoInfo()
    rows = None
    expected = []
    saw_end = False
    try:
        for match in START_CODE.finditer(video):
            code, pos = match.group(1)[0], match.start()
            if SLICE_FIRST <= code <= SLICE_LAST:
                if rows is None:
                    raise arc.FormatError('slice outside a picture at %#x' % pos)
                rows.append(code)
                continue
            if code == USER_DATA:
                continue
            if code == EXTENSION:
                scan_extension(video, pos, info)
                continue
            if rows is not None:
                if rows != expected:
                    raise arc.FormatError('picture before %#x has rows %s' % (pos, rows[:6]))
                rows = None
            if code == SEQUENCE_HEADER:
                scan_sequence_header(video, pos, info)
                expected = list(range(1, (info.height + MACROBLOCK - 1) // MACROBLOCK + 1))
            elif code == GOP:
                info.gops += 1
            elif code == 0:
                kind = video[pos + 5] >> 3 & 7 if pos + 6 <= len(video) else 0
                if kind not in PICTURE_TYPES:
                    raise arc.FormatError('unknown picture type at %#x' % pos)
                info.types[PICTURE_TYPES[kind]] += 1
                info.pictures += 1
                rows = []
            elif code == SEQUENCE_END:
                saw_end = pos + 4 == len(video)
            else:
                raise arc.FormatError('unknown start code %#x at %#x' % (code, pos))
    except arc.FormatError as error:
        raise arc.FormatError('%s: %s' % (name, error)) from None
    if not info.sequences or not info.pictures:
        raise arc.FormatError('%s: no MPEG video found' % name)
    if not saw_end:
        raise arc.FormatError('%s: video does not end on a sequence end code' % name)
    return info


def worst_pts_drift(movie, form):
    """Return the largest gap, in seconds, between an audio PTS and its byte position."""
    bytes_per_second = form.rate * form.channels * 2
    first_pts, first_bytes = movie.audio_pts[0]
    return max(abs((pts - first_pts) / PTS_CLOCK - (done - first_bytes) / bytes_per_second)
               for pts, done in movie.audio_pts)


def audit(movie):
    """Cross-check one demuxed movie; return (video info, audio format, problems)."""
    info = scan_video(movie.video, movie.name)
    form = parse_audio_header(movie.audio, movie.name)
    problems = []
    if form.codec != AUDIO_CODEC_PCM16:
        problems.append('audio codec %d is not PCM' % form.codec)
    seconds = form.data_size / (form.rate * form.channels * 2)
    if not -AUDIO_SHORT_LIMIT < seconds - info.seconds < AUDIO_TAIL_LIMIT:
        problems.append('audio runs %.3f s, video %.3f s' % (seconds, info.seconds))
    span = (max(movie.video_pts) - min(movie.video_pts)) / PTS_CLOCK
    if abs(span - (info.pictures - 1) / info.frame_rate) > 2 / info.frame_rate:
        problems.append('video PTS spans %.3f s for %d pictures' % (span, info.pictures))
    drift = worst_pts_drift(movie, form)
    if drift > PTS_DRIFT_LIMIT:
        problems.append('audio PTS drifts %.3f s from its byte position' % drift)
    if form.data_size > len(movie.audio) - AUDIO_HEADER_SIZE:
        problems.append('SSbd size is more than the audio stream holds')
    return info, form, problems


def each_movie(disc):
    archive = arc.find_archive(disc, MOVIE_ARCHIVE)
    for entry in sorted(arc.read_toc(disc, archive), key=lambda e: e.name):
        yield entry, disc.read(archive.lba, entry.offset, entry.size)


def digest(data):
    return hashlib.sha1(data).hexdigest()[:8]


def describe(entry, movie, info, form):
    return ('%-16s %9d  %dx%d %s@%.2f %6.2fs %4d pics %s %3d GOP %.1f Mbit/s | '
            'pcm16 %d Hz %dch block %#x %.2fs | v:%s a:%s' % (
                entry.name, entry.size, info.width, info.height,
                'mpeg2' if info.mpeg2 else 'mpeg1', info.frame_rate, info.seconds,
                info.pictures, ''.join('%s%d' % t for t in sorted(info.types.items())),
                info.gops, info.bit_rate / 1e6, form.rate, form.channels,
                form.interleave, form.data_size / (form.rate * form.channels * 2),
                digest(movie.video), digest(audio_samples(movie, form))))


def check(disc):
    failures = []
    streams = collections.Counter()
    for entry, data in each_movie(disc):
        try:
            movie = demux(data, entry.name)
            info, form, problems = audit(movie)
        except arc.FormatError as error:
            failures.append(str(error))
            continue
        streams.update(movie.packets)
        print(describe(entry, movie, info, form))
        failures += ['%s: %s' % (entry.name, problem) for problem in problems]
    print('packets by stream:', {hex(k): v for k, v in sorted(streams.items())})
    print('failures: %d' % len(failures))
    for failure in failures[:20]:
        print('   ', failure)
    return 1 if failures else 0


def write_wav(path, samples, form):
    with wave.open(path, 'wb') as out:
        out.setnchannels(form.channels)
        out.setsampwidth(2)
        out.setframerate(form.rate)
        out.writeframes(deinterleave(samples, form.channels, form.interleave))


def export(disc, out_dir, wanted):
    os.makedirs(out_dir, exist_ok=True)
    written = 0
    for entry, data in each_movie(disc):
        if wanted not in entry.name:
            continue
        movie = demux(data, entry.name)
        form = parse_audio_header(movie.audio, entry.name)
        stem = os.path.join(out_dir, arc.safe_file_name(entry.name))
        with open(stem + '.m2v', 'wb') as out:
            out.write(movie.video)
        write_wav(stem + '.wav', audio_samples(movie, form), form)
        written += 1
    print('wrote %d movies to %s' % (written, out_dir))
    return 0 if written else 1


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(arc.Disc(argv[2]))
    if len(argv) == 5 and argv[1] == 'export':
        return export(arc.Disc(argv[2]), argv[3], argv[4])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
