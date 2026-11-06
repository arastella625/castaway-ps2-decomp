#!/usr/bin/env python3
"""Decode the game's audio from your own disc image into WAV files.

Usage:
  python -I tools/audio.py check  <game.iso>
  python -I tools/audio.py export <game.iso> <out_dir> <name_substring>
  python -I tools/audio.py event  <game.iso> <name_substring>

check decodes every entry of SAMPLES and AUDIOSTR, parses SOUNDEVE, AMBIENTS and SOUNDTRA
to their last byte, follows the name hashes between them, and compares each decode against
deliberately wrong ones. It exits non-zero if anything fails. export writes matching samples
and music as WAV; a looping sample gets a standard 'smpl' chunk. event prints which samples
a sound event can play. The WAVs are game data: keep them out of the repo.
"""
import array
import collections
import math
import multiprocessing
import os
import struct
import sys
from dataclasses import dataclass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # -I leaves this out
import arc  # noqa: E402

SAMPLE_ARCHIVE = 'SAMPLES'
STREAM_ARCHIVE = 'AUDIOSTR'
EVENT_ARCHIVE = 'SOUNDEVE'
AMBIENT_ARCHIVE = 'AMBIENTS'
TRACK_ARCHIVE = 'SOUNDTRA'

BLOCK = 16  # one SPU ADPCM block: shift/filter byte, flag byte, 14 bytes of nibbles
BLOCK_SAMPLES = 28
FILTERS = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))  # the SPU's, in 1/64ths
MAX_SHIFT = 12
FLAG_END, FLAG_REPEAT, FLAG_LOOP_START = 1, 2, 4
SILENT_END_BLOCK = b'\x00\x07' + b'\x77' * 14

TRAILER_SIZE = 64
TRAILER_MAGIC = b'VAGp'
TRAILER_VERSION = 2
TRAILER_ALIGN = 64
TRAILER_NAME_SIZE = 16
WAV_SUFFIX = '.wav'

STREAM_HEADER_SIZE = 0x40
MAX_TRACKS = 2
MAX_RATE = 48000

EVENT_TAG = b'TVES'  # "SEVT", stored as a little-endian integer
AMBIENT_TAG = b'TOCS'  # "SCOT"
TRACK_TAG = b'DRTS'  # "STRD"
DATA_VERSION = 1
EVENT_SETTINGS_SIZE = 13  # the bytes between the two opening bytes and the hit lists
AMBIENT_SOUND_BYTES = 10  # the bytes between a sound's six floats and its hash list

# Limits for the comparison with deliberately wrong decodes.
QUIET_POWER = 100  # mean square below this is too quiet to say anything
WANDER_RUN = 256
MOST_LOSSES = 0.15  # share of entries the right decode may lose on; a coin would lose half
WRONG_FILTERS = FILTERS[:1] + FILTERS[2:] + FILTERS[1:2]
WRONG_CHUNK = BLOCK
WRONG_DECODES = ('wrong filter table', 'nibbles swapped', 'wrong interleave')


@dataclass(frozen=True)
class Sample:
    rate: int
    blocks: bytes  # only the blocks that play
    loop_start: int | None  # block number, or None


@dataclass(frozen=True)
class Stream:
    rate: int
    flags: int
    interleave: int
    tracks: tuple  # one bytes per track, cut at the real end


@dataclass(frozen=True)
class Event:
    name: str
    hit_lists: tuple  # of tuples of (stream hash, sample hash); one of the two is 0
    track_hash: int | None


def _make_nibble_tables():
    """Per shift, each byte's two samples as (low nibble, high nibble), shifted."""
    tables = []
    for shift in range(MAX_SHIFT + 1):
        def scaled(nibble):
            signed = nibble - 16 if nibble > 7 else nibble
            return (signed << 12) >> shift
        tables.append(tuple((scaled(b & 15), scaled(b >> 4)) for b in range(256)))
    return tables


NIBBLES = _make_nibble_tables()


def decode(blocks, filters=FILTERS, high_first=False, state=(0, 0)):
    """Turn SPU ADPCM blocks into 16-bit samples, keeping history across blocks."""
    out = array.array('h')
    append = out.append
    older, old = state
    for pos in range(0, len(blocks), BLOCK):
        shift_filter = blocks[pos]
        shift, kind = shift_filter & 15, shift_filter >> 4
        if shift > MAX_SHIFT or kind >= len(filters):
            raise arc.FormatError('bad ADPCM block header %#x at %#x' % (shift_filter, pos))
        table = NIBBLES[shift]
        gain_old, gain_older = filters[kind]
        for byte in blocks[pos + 2:pos + BLOCK]:
            first, second = table[byte]
            if high_first:
                first, second = second, first
            older = first + ((old * gain_old + older * gain_older + 32) >> 6)
            older = 32767 if older > 32767 else -32768 if older < -32768 else older
            append(older)
            old, older = second + ((older * gain_old + old * gain_older + 32) >> 6), older
            old = 32767 if old > 32767 else -32768 if old < -32768 else old
            append(old)
    return out


def sample_loop(flags, entry_name):
    """Check the block flag sequence; return (blocks that play, loop start block or None)."""
    last = len(flags) - 1
    ends = [i for i, f in enumerate(flags) if f & FLAG_END]
    if len(ends) != 2 or ends[1] != last or flags[last] != 7:
        raise arc.FormatError('%s: unexpected end flags' % entry_name)
    end = ends[0]
    if end != last - 1:
        raise arc.FormatError('%s: end flag is not on the last sound block' % entry_name)
    starts = [i for i, f in enumerate(flags[:last]) if f & FLAG_LOOP_START]
    if flags[end] == FLAG_END:
        if starts or any(flags[:end]):
            raise arc.FormatError('%s: loop flags in a one-shot sample' % entry_name)
        return end + 1, None
    if flags[end] != FLAG_END | FLAG_REPEAT or len(starts) != 1:
        raise arc.FormatError('%s: bad loop flags' % entry_name)
    if any(f != FLAG_REPEAT for i, f in enumerate(flags[:end]) if i != starts[0]):
        raise arc.FormatError('%s: bad flag inside a looping sample' % entry_name)
    return end + 1, starts[0]


def parse_sample(data, entry_name):
    """Check one SAMPLES entry to its last byte: blocks, end block, padding, VAGp trailer."""
    if len(data) < TRAILER_SIZE + 2 * BLOCK or len(data) % TRAILER_ALIGN:
        raise arc.FormatError('%s: size %d does not fit a sample' % (entry_name, len(data)))
    trailer = data[-TRAILER_SIZE:]
    if trailer[:4] != TRAILER_MAGIC:
        raise arc.FormatError('%s: no VAGp trailer' % entry_name)
    version, _, size, rate = struct.unpack_from('<4I', trailer, 4)
    if version != TRAILER_VERSION or not 0 < rate <= MAX_RATE:
        raise arc.FormatError('%s: trailer version %d, rate %d' % (entry_name, version, rate))
    if size % BLOCK or size < 2 * BLOCK or size > len(data) - TRAILER_SIZE:
        raise arc.FormatError('%s: data size %d' % (entry_name, size))
    if -(-size // TRAILER_ALIGN) * TRAILER_ALIGN != len(data) - TRAILER_SIZE:
        raise arc.FormatError('%s: padding is not to the next 64 bytes' % entry_name)
    if any(data[size:-TRAILER_SIZE]):
        raise arc.FormatError('%s: padding is not zero' % entry_name)
    if any(trailer[20:32]):
        raise arc.FormatError('%s: trailer sets volume, pitch or ADSR' % entry_name)
    stored = trailer[32:32 + TRAILER_NAME_SIZE].split(b'\0')[0].decode('latin-1').lower()
    if stored not in (entry_name.lower()[:TRAILER_NAME_SIZE],
                      (entry_name + WAV_SUFFIX).lower()[:TRAILER_NAME_SIZE]):
        raise arc.FormatError('%s: trailer names %r' % (entry_name, stored))
    if data[size - BLOCK:size] != SILENT_END_BLOCK:
        raise arc.FormatError('%s: no silent end block' % entry_name)
    played, loop_start = sample_loop(data[1:size:BLOCK], entry_name)
    if trailer[48] != (loop_start is not None):
        raise arc.FormatError('%s: trailer loop byte disagrees with the flags' % entry_name)
    return Sample(rate, data[:played * BLOCK], loop_start)


def parse_stream(data, entry_name):
    """Check one AUDIOSTR entry: header sizes, block layout, zeros after the real end."""
    if len(data) < STREAM_HEADER_SIZE:
        raise arc.FormatError('%s: too small for a stream' % entry_name)
    words = struct.unpack_from('<6I', data, 0)
    header_size, flags_and_end, count, rate, interleave, chunks = words
    flags, end = flags_and_end & 0xff, flags_and_end >> 8
    if header_size != STREAM_HEADER_SIZE or any(data[6 * 4:STREAM_HEADER_SIZE]):
        raise arc.FormatError('%s: unexpected stream header' % entry_name)
    if not 0 < count <= MAX_TRACKS or not 0 < rate <= MAX_RATE:
        raise arc.FormatError('%s: %d tracks at %d Hz' % (entry_name, count, rate))
    if not interleave or interleave % BLOCK or not chunks or end % BLOCK or end > interleave:
        raise arc.FormatError('%s: bad interleave or end offset' % entry_name)
    if STREAM_HEADER_SIZE + count * interleave * chunks != len(data):
        raise arc.FormatError('%s: header does not add up to the file size' % entry_name)
    body = memoryview(data)[STREAM_HEADER_SIZE:]
    return Stream(rate, flags, interleave, tuple(
        _stream_track(body, track, count, interleave, chunks, end, entry_name)
        for track in range(count)))


def _stream_track(body, track, count, interleave, chunks, end, entry_name):
    parts = [body[(i * count + track) * interleave:(i * count + track + 1) * interleave]
             for i in range(chunks)]
    if any(bytes(parts[-1][end:])):
        raise arc.FormatError('%s: data after the end offset' % entry_name)
    parts[-1] = parts[-1][:end]
    blocks = b''.join(parts)
    if any(blocks[1::BLOCK]):
        raise arc.FormatError('%s: flags set in a stream' % entry_name)
    return blocks


class Reader:
    """Bounds-checked little-endian reads from a small resource."""

    def __init__(self, data, entry_name):
        self.data, self.name, self.pos = data, entry_name, 0

    def take(self, size):
        if size < 0 or self.pos + size > len(self.data):
            raise arc.FormatError('%s: runs out at %#x' % (self.name, self.pos))
        self.pos += size
        return self.data[self.pos - size:self.pos]

    def u32(self):
        return struct.unpack('<I', self.take(4))[0]

    def byte(self):
        return self.take(1)[0]

    def finish(self):
        if self.pos != len(self.data):
            raise arc.FormatError('%s: %d bytes left over' % (self.name, len(self.data) - self.pos))


def read_data_header(reader, tag):
    """Read the version, tag, id and name that open events, scores and tracks; return the name."""
    if reader.u32() != DATA_VERSION or reader.take(4) != tag:
        raise arc.FormatError('%s: not a %r resource' % (reader.name, tag))
    reader.u32()
    name = reader.take(reader.u32()).split(b'\0')[0].decode('latin-1')
    if name.lower() != reader.name.lower():
        raise arc.FormatError('%s: resource calls itself %r' % (reader.name, name))
    return name


def parse_event(data, entry_name):
    """Follow ERSoundEvent::Load: a block of settings, then hit lists, then a track hash."""
    reader = Reader(data, entry_name)
    name = read_data_header(reader, EVENT_TAG)
    if reader.u32() != len(data) - reader.pos:
        raise arc.FormatError('%s: payload size is wrong' % entry_name)
    reader.byte()
    if not reader.byte():
        reader.finish()
        return Event(name, (), None)
    reader.take(EVENT_SETTINGS_SIZE)
    lists = []
    for _ in range(reader.u32()):
        lists.append(tuple((reader.u32(), reader.u32()) for _ in range(reader.u32())))
    track_hash = reader.u32()
    reader.finish()
    return Event(name, tuple(lists), track_hash)


def parse_ambient(data, entry_name):
    """Follow ERAmbientScore::Load; return the hash lists of the score's sounds."""
    reader = Reader(data, entry_name)
    read_data_header(reader, AMBIENT_TAG)
    if reader.u32() != len(data):
        raise arc.FormatError('%s: size field is wrong' % entry_name)
    reader.take(3)
    sounds = []
    for _ in range(reader.u32()):
        count = reader.u32()
        reader.take(6 * 4 + AMBIENT_SOUND_BYTES)
        sounds.append(tuple(reader.u32() for _ in range(count)))
    reader.finish()
    return sounds


def parse_track(data, entry_name):
    """A track is a small program: return its instruction words."""
    reader = Reader(data, entry_name)
    read_data_header(reader, TRACK_TAG)
    size = reader.u32()
    if size % 4 or size != len(data) - reader.pos:
        raise arc.FormatError('%s: payload size is wrong' % entry_name)
    return struct.unpack('<%dI' % (size // 4), reader.take(size))


# --- How to tell sound from noise ---------------------------------------------------------

def wander(samples):
    """Return the share of the energy below about 50 Hz, in dB, from averages of short runs.

    The encoder ran the decoder in its own loop, so each block corrects the error left by
    the last one and the output tracks the original. Any mistake in decoding breaks that
    loop, and the filters, whose poles sit near 1, integrate the error into slow drift.
    Nothing in this measure is given by the decoder itself: the first test tried here, how
    well each stored filter fits the output, passes on garbage because the residual under
    the stored filter is just the nibbles.
    """
    total = sum(s * s for s in samples)
    if total < len(samples) * QUIET_POWER or len(samples) < WANDER_RUN:
        return None
    runs = len(samples) // WANDER_RUN
    slow = sum((sum(samples[i * WANDER_RUN:(i + 1) * WANDER_RUN]) / WANDER_RUN) ** 2
               for i in range(runs)) * WANDER_RUN
    return 10 * math.log10(max(slow / total, 1e-9))


def score(blocks, filters=FILTERS, high_first=False):
    samples = decode(blocks, filters, high_first)
    return {'wander': wander(samples),
            'clipped': sum(1 for s in samples if s in (32767, -32768)), 'count': len(samples)}


def every_other_chunk(blocks):
    """The blocks a reader would keep if it wrongly took this for two-way interleaved data."""
    return b''.join(blocks[pos:pos + WRONG_CHUNK]
                    for pos in range(0, len(blocks), 2 * WRONG_CHUNK))


def compare(blocks):
    """Score the right decode and each deliberately wrong one."""
    return {'right': score(blocks),
            WRONG_DECODES[0]: score(blocks, WRONG_FILTERS),
            WRONG_DECODES[1]: score(blocks, high_first=True),
            WRONG_DECODES[2]: score(every_other_chunk(blocks))}


def seam_steps(samples, every):
    """Return (sum, count) of the steps at each seam, and of all the other steps."""
    seam = other = 0
    seams = others = 0
    for i in range(1, len(samples)):
        step = abs(samples[i] - samples[i - 1])
        if i % every == 0:
            seam += step
            seams += 1
        else:
            other += step
            others += 1
    return seam, seams, other, others


def correlation(left, right):
    size = min(len(left), len(right))
    left, right = left[:size], right[:size]
    mean_l, mean_r = sum(left) / size, sum(right) / size
    both = sum((a - mean_l) * (b - mean_r) for a, b in zip(left, right))
    energy_l = sum((a - mean_l) ** 2 for a in left)
    energy_r = sum((b - mean_r) ** 2 for b in right)
    return both / (energy_l * energy_r) ** 0.5 if energy_l and energy_r else 0.0


def stream_layout_test(stream):
    """Check the track layout: seams should not show, and stereo tracks should correlate."""
    chunk = stream.interleave // BLOCK * BLOCK_SAMPLES
    tracks = [decode(t) for t in stream.tracks]
    result = {'right seams': [seam_steps(t, chunk) for t in tracks]}
    # Wrong: read the whole track as two tracks that alternate each interleave chunk.
    blocks = stream.tracks[0]
    wrong = [b''.join(blocks[p:p + stream.interleave]
                      for p in range(start, len(blocks), 2 * stream.interleave))
             for start in (0, stream.interleave)]
    wrong = [decode(w) for w in wrong]
    result['wrong seams'] = [seam_steps(t, chunk) for t in wrong]
    result['wrong correlation'] = correlation(wrong[0], wrong[1])
    if len(tracks) == 2:
        result['right correlation'] = correlation(tracks[0], tracks[1])
    return result


# --- Workers for check ---------------------------------------------------------------------

_worker = {}


def _worker_start(iso):
    _worker['disc'] = arc.Disc(iso)


def _read(archive, entry):
    return _worker['disc'].read(archive.lba, entry.offset, entry.size)


def check_sample(job):
    archive, entry = job
    try:
        data = _read(archive, entry)
        sample = parse_sample(data, entry.name)
        return {'name': entry.name, 'rate': sample.rate, 'loop': sample.loop_start,
                'blocks': len(sample.blocks) // BLOCK,
                'compare': compare(sample.blocks), 'loop_seam': _loop_seam(sample)}
    except arc.FormatError as error:
        return {'name': entry.name, 'error': str(error)}


def _loop_seam(sample):
    """Return the jump from the loop's end to its start, and to a block halfway through."""
    if sample.loop_start is None:
        return None
    samples = decode(sample.blocks)
    first = sample.loop_start
    middle = (first + len(samples) // BLOCK_SAMPLES) // 2
    return (abs(samples[first * BLOCK_SAMPLES] - samples[-1]),
            abs(samples[middle * BLOCK_SAMPLES] - samples[-1]))


def check_stream(job):
    archive, entry = job
    try:
        stream = parse_stream(_read(archive, entry), entry.name)
        return {'name': entry.name, 'rate': stream.rate, 'flags': stream.flags,
                'tracks': len(stream.tracks), 'bytes': len(stream.tracks[0]),
                'compare': compare(stream.tracks[0]), 'layout': stream_layout_test(stream)}
    except arc.FormatError as error:
        return {'name': entry.name, 'error': str(error)}


def run_jobs(iso, function, jobs):
    workers = max(1, (os.cpu_count() or 2) // 2)
    with multiprocessing.Pool(workers, _worker_start, (iso,)) as pool:
        return list(pool.imap_unordered(function, jobs, chunksize=4))


# --- Reports -------------------------------------------------------------------------------

def report_failures(label, results):
    failed = [r for r in results if 'error' in r]
    print('%s: %d of %d decoded and passed every structural test'
          % (label, len(results) - len(failed), len(results)))
    for result in failed[:20]:
        print('    FAIL', result['error'])
    return len(failed)


def _median(values):
    values = sorted(v for v in values if v is not None)
    return values[len(values) // 2] if values else 0.0


def _clip_rate(result):
    return result['clipped'] / max(1, result['count'])


def _drift(result):
    return result['wander']


def _wins(results, name, measure):
    """Return how many entries the right decode beat a wrong one on, those tested, the losers."""
    wins, losers = 0, []
    for result in results:
        right, wrong = measure(result['compare']['right']), measure(result['compare'][name])
        if right is None or wrong is None:
            continue
        if right < wrong:
            wins += 1
        else:
            losers.append(result['name'])
    return wins, wins + len(losers), losers


def report_comparison(results, measure):
    """Print each decode's scores; return 1 if the right decode loses too often."""
    good = [r for r in results if 'error' not in r]
    too_many = 0
    for name in ('right',) + WRONG_DECODES:
        scores = [r['compare'][name] for r in good]
        clipped = sum(s['clipped'] for s in scores) / max(1, sum(s['count'] for s in scores))
        print('    %-20s median drift %6.1f dB, clipped %.4f%%'
              % (name, _median(s['wander'] for s in scores), 100 * clipped))
    for name in WRONG_DECODES:
        wins, tested, losers = _wins(good, name, measure)
        groups = collections.Counter(n.split('_')[0] for n in losers)
        print('    %s lower than %-18s on %d of %d; others mostly %s'
              % (measure.__name__.strip('_'), name, wins, tested, groups.most_common(3)))
        too_many |= tested - wins > MOST_LOSSES * tested
    return int(too_many)


def report_samples(results):
    good = [r for r in results if 'error' not in r]
    rates = collections.Counter(r['rate'] for r in good)
    print('    rates:', ', '.join('%d Hz x%d' % item for item in sorted(rates.items())))
    loops = [r for r in good if r['loop'] is not None]
    print('    looping: %d; one-shot: %d' % (len(loops), len(good) - len(loops)))
    seams = [r['loop_seam'] for r in loops]
    print('    jump from loop end: to loop start median %d, to a block mid-loop median %d'
          % (_median(s[0] for s in seams), _median(s[1] for s in seams)))


def _seam_ratio(parts):
    seam = sum(p[0] for p in parts) / max(1, sum(p[1] for p in parts))
    other = sum(p[2] for p in parts) / max(1, sum(p[3] for p in parts))
    return seam / max(other, 1e-9)


def report_streams(results):
    """Print stream facts; return how many wrong layouts this test failed to tell apart."""
    good = [r for r in results if 'error' not in r]
    kinds = collections.Counter((r['tracks'], r['rate'], hex(r['flags'])) for r in good)
    print('    tracks, rate, flags:', sorted(kinds.items()))
    right = [p for r in good for p in r['layout']['right seams']]
    wrong = [p for r in good for p in r['layout']['wrong seams']]
    print('    step at chunk seams over a typical step: right layout %.2f, wrong layout %.2f'
          % (_seam_ratio(right), _seam_ratio(wrong)))
    for result in sorted(good, key=lambda r: r['name']):
        if result['tracks'] == 2:
            print('    %s: left/right correlation %.3f; wrong layout %.3f'
                  % (result['name'], result['layout']['right correlation'],
                     result['layout']['wrong correlation']))
    return 0 if _seam_ratio(right) < _seam_ratio(wrong) else 1


def check_links(disc):
    """Parse the small archives to the last byte and follow every hash; return failures."""
    tables = {name: read_entries(disc, name) for name in
              (SAMPLE_ARCHIVE, STREAM_ARCHIVE, EVENT_ARCHIVE, AMBIENT_ARCHIVE, TRACK_ARCHIVE)}
    samples = {e.name_hash for _, e in tables[SAMPLE_ARCHIVE]}
    streams = {e.name_hash for _, e in tables[STREAM_ARCHIVE]}
    tracks = {e.name_hash for _, e in tables[TRACK_ARCHIVE]}
    failures = 0
    used = set()
    missing = collections.Counter()
    used_streams = set()
    no_track = 0

    def parsed(archive_name, parser):
        nonlocal failures
        results = []
        for archive, entry in tables[archive_name]:
            try:
                results.append(parser(disc.read(archive.lba, entry.offset, entry.size),
                                      entry.name))
            except arc.FormatError as error:
                failures += 1
                print('    FAIL', error)
        print('%s: %d of %d parsed to the last byte'
              % (archive_name, len(results), len(tables[archive_name])))
        return results

    for event in parsed(EVENT_ARCHIVE, parse_event):
        for hit_list in event.hit_lists:
            for stream_hash, sample_hash in hit_list:
                if stream_hash and not sample_hash and stream_hash in streams:
                    used_streams.add(stream_hash)
                elif sample_hash in samples and not stream_hash:
                    used.add(sample_hash)
                else:
                    missing[sample_hash] += 1
        if event.track_hash is None:
            no_track += 1
        elif event.track_hash not in tracks:
            failures += 1
            print('    FAIL %s: unknown track %#x' % (event.name, event.track_hash))
    print('    events with no hit lists: %d; streams named by events: %d'
          % (no_track, len(used_streams)))
    event_used = len(used)
    ambient_hashes = 0
    for sounds in parsed(AMBIENT_ARCHIVE, parse_ambient):
        for hashes in sounds:
            for name_hash in hashes:
                ambient_hashes += 1
                if name_hash in samples:
                    used.add(name_hash)
                else:
                    missing[name_hash] += 1
    parsed(TRACK_ARCHIVE, parse_track)
    print('    samples named by events %d; by events or ambient scores %d of %d; '
          'ambient hashes %d; hashes with no sample %d'
          % (event_used, len(used), len(samples), ambient_hashes, sum(missing.values())))
    return failures + sum(missing.values())


def read_entries(disc, name):
    archive = arc.find_archive(disc, name)
    return [(archive, entry) for entry in arc.read_toc(disc, archive)]


def check(iso):
    disc = arc.Disc(iso)
    failures = check_links(disc)
    streams = sorted(read_entries(disc, STREAM_ARCHIVE), key=lambda j: -j[1].size)
    results = run_jobs(iso, check_stream, streams)
    failures += report_failures(STREAM_ARCHIVE, results)
    failures += report_streams(results)
    failures += report_comparison(results, _clip_rate)  # music has bass, so drift can't tell
    results = run_jobs(iso, check_sample, read_entries(disc, SAMPLE_ARCHIVE))
    failures += report_failures(SAMPLE_ARCHIVE, results)
    report_samples(results)
    failures += report_comparison(results, _drift)  # most samples are too quiet to clip
    print('result: %s' % ('PASS' if not failures else '%d FAILURES' % failures))
    return 1 if failures else 0


# --- WAV output ----------------------------------------------------------------------------

def pcm_bytes(samples):
    if sys.byteorder == 'big':
        samples = array.array('h', samples)
        samples.byteswap()
    return samples.tobytes()


def riff_chunk(tag, body):
    return tag + struct.pack('<I', len(body)) + body + b'\0' * (len(body) & 1)


def smpl_chunk(rate, start, end):
    """The standard sampler chunk with one forward loop; the end sample is inclusive."""
    nanoseconds = 1000000000 // rate
    header = struct.pack('<9I', 0, 0, nanoseconds, 60, 0, 0, 0, 1, 0)
    return riff_chunk(b'smpl', header + struct.pack('<6I', 0, 0, start, end, 0, 0))


def wav_bytes(channels, rate, pcm, extra=b''):
    format_body = struct.pack('<HHIIHH', 1, channels, rate, rate * channels * 2, channels * 2, 16)
    body = b'WAVE' + riff_chunk(b'fmt ', format_body) + riff_chunk(b'data', pcm) + extra
    return riff_chunk(b'RIFF', body)


def sample_wav(sample):
    samples = decode(sample.blocks)
    extra = b''
    if sample.loop_start is not None:
        extra = smpl_chunk(sample.rate, sample.loop_start * BLOCK_SAMPLES, len(samples) - 1)
    return wav_bytes(1, sample.rate, pcm_bytes(samples), extra)


def stream_wav(stream):
    tracks = [decode(t) for t in stream.tracks]
    if len(tracks) == 1:
        return wav_bytes(1, stream.rate, pcm_bytes(tracks[0]))
    both = array.array('h', bytes(2 * len(tracks) * len(tracks[0])))
    for number, track in enumerate(tracks):
        both[number::len(tracks)] = track
    return wav_bytes(len(tracks), stream.rate, pcm_bytes(both))


def export(iso, out_dir, wanted):
    disc = arc.Disc(iso)
    os.makedirs(out_dir, exist_ok=True)
    written = 0
    for name, parser, render in ((SAMPLE_ARCHIVE, parse_sample, sample_wav),
                                 (STREAM_ARCHIVE, parse_stream, stream_wav)):
        for archive, entry in read_entries(disc, name):
            if wanted not in entry.name:
                continue
            data = disc.read(archive.lba, entry.offset, entry.size)
            target = '%s.%08x.wav' % (arc.safe_file_name(entry.name), entry.name_hash)
            with open(os.path.join(out_dir, target), 'wb') as out:
                out.write(render(parser(data, entry.name)))
            written += 1
    print('wrote %d files to %s' % (written, out_dir))
    return 0 if written else 1


def show_events(iso, wanted):
    disc = arc.Disc(iso)
    audio = {e.name_hash: e.name for name in (SAMPLE_ARCHIVE, STREAM_ARCHIVE)
             for _, e in read_entries(disc, name)}
    tracks = {e.name_hash: e.name for _, e in read_entries(disc, TRACK_ARCHIVE)}
    shown = 0
    for archive, entry in read_entries(disc, EVENT_ARCHIVE):
        if wanted not in entry.name:
            continue
        event = parse_event(disc.read(archive.lba, entry.offset, entry.size), entry.name)
        print('%s (track %s)' % (event.name, tracks.get(event.track_hash)))
        for number, hit_list in enumerate(event.hit_lists):
            print('  list %d:' % number, ', '.join(
                audio.get(a or b, '%08x?' % (a or b)) for a, b in hit_list))
        shown += 1
    return 0 if shown else 1


def main(argv):
    if len(argv) == 3 and argv[1] == 'check':
        return check(argv[2])
    if len(argv) == 5 and argv[1] == 'export':
        return export(argv[2], argv[3], argv[4])
    if len(argv) == 4 and argv[1] == 'event':
        return show_events(argv[2], argv[3])
    print(__doc__)
    return 2


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except (arc.FormatError, OSError) as error:
        print('error: %s' % error, file=sys.stderr)
        print('Check that the path points at an unmodified copy of the game.', file=sys.stderr)
        sys.exit(1)
