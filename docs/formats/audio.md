# Audio formats

Worked out from the US disc and the PS2 executable (addresses are from `split\asm\text.s`).
`tools/audio.py` decodes all of it. All values are little-endian. Most claims below are from data
and code. A sample of clips has been heard, as noted under Open.

## Samples (`SAMPLES`, 7,656 entries)

Checked on all 7,656: every entry fits this layout to its last byte, and `audio.py check`
decodes every one. The "mixed first u32" in the docs is not a header. It is the first ADPCM
block, so 0x0c means shift 12, filter 0, flags 0.

There is no header. A sample is, in order:

| Part | Size | Meaning |
|---|---:|---|
| Blocks | data size | SPU ADPCM, 16 bytes each |
| Padding | 0 to 48 | Zeros, up to the next 64 bytes |
| Trailer | 64 | `VAGp` record, at the END of the entry |

The trailer is the usual VAG header moved to the end. The game seeks to -0x40 from the end of
the file to read it (func_004270C8, called by func_00435DA8). **Proven.** It can also take the
same 64 bytes from a table via func_004166C8 (likely the `INDEX.IND` data; not followed up).

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | char[4] | `VAGp` |
| 0x04 | u32 | 2 on all entries |
| 0x0c | u32 | Data size, including the final silent block |
| 0x10 | u32 | Sample rate. func_00416888 turns it into SPU pitch: `(rate << 12) / 48000`. **Proven** |
| 0x14 | u16 x2 | Volume L, R. 0 on all entries, so the game uses 0x3fff |
| 0x18 | u16 | Pitch. 0 on all entries, so it is computed from the rate |
| 0x1a | u16 x2 | ADSR1, ADSR2. 0 on all entries, so the game uses 0x000f, 0x1fc0 |
| 0x20 | char[16] | Source name, cut to 16: the entry name plus `.wav`, or without it (`sims_ea_logo`) |
| 0x30 | u8 | 1 exactly when the sample loops (144 of 144, no exceptions); read as a bool by the loader |
| 0x31 | 15 bytes | Not used here; nearly constant, looks like tool memory garbage (likely) |

All samples are mono. There is one `VAGp` per entry, so the Wii's two-part stereo path
(`ERSampledata::LoadStereo`) has no PS2 counterpart in the data.

Rates: 24,000 x6,420; 22,050 x758; 12,000 x471; 44,100 x5; 11,025 x2.

### Blocks

Byte 0: shift in the low nibble, filter in the high. Byte 1: flags. Bytes 2 to 15: 28 samples,
low nibble first. All blocks in all files have shift <= 12 and filter <= 4. The filter pairs
are the SPU's: (0,0) (60,0) (115,-52) (98,-55) (122,-60), over 64.

Two flag sequences exist, and there are no others:

- One-shot (7,512): 0 ... 0, 1, 7
- Looping (144): 2 ... 2, 6, 2 ... 2, 3, 7

Flag 1 or 3 marks the last block that plays. After it comes one silent block, always
`00 07 77 77 ...`, which never plays. Flag 6 is the loop start; flag 3 jumps back to it.
Loop points are whole blocks, 28 samples apart. Flag meanings are standard SPU behaviour; the
two sequences are checked on every entry, and the trailer byte agrees with them on every entry.

Exact sample length is only known to within a block: the last block is padded with zero nibbles.

"loop" in a name doesn't mean a hardware loop: 386 samples with `loop` or `lp` in the name are
one-shots (`beehive_swarm_loop01`). Presumably the game retriggers them. Open.

## Music (`AUDIOSTR`, 44 entries)

These are SCEE MultiStream `.MIB` files with the `.MIH` header on the front; the IOP module in
`IOPSKU1.DAT` says "MultiStream V6.2". Header, 0x40 bytes, read by func_0046AA38
(`SOUND_GetMIHInfo`). **Proven** from that code; the file size test below passes on all 44.

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | Header size, 0x40 |
| 0x04 | u8 | Flags: 0x24 on one-track files, 0x20 on two-track. Meaning open |
| 0x05 | u24 | Bytes used in the last chunk of each track |
| 0x08 | u32 | Tracks, 1 or 2 |
| 0x0c | u32 | Sample rate. Goes through func_0046A820, `(rate << 12) / 48000` |
| 0x10 | u32 | Interleave, 0x4000 on all |
| 0x14 | u32 | Chunks per track |
| 0x18 | u32[] | Per-track values, read by the code; all zero here |

- `0x40 + tracks * interleave * chunks` is the file size exactly, on all 44.
- Data alternates chunks: track 0, track 1, track 0, ...
- Real end: `(chunks - 1) * interleave + used`. On all 44 everything after it is zero, and the
  last non-zero byte lies in the block just before it. So that reading of 0x05 is proven by data.
- Same ADPCM, with no flags set anywhere. Looping is the game's job, not the file's: streams
  named `_loop` hold no loop point. History carries across chunks.

**Disagreement with the names:** `music_stereo_*` are all stored as ONE track. Only
`music_48_title_track` (48 kHz), `music_shopping1` and `music_shopping3` (24 kHz) have two.
41 files are mono at 24 kHz. Treating a `music_stereo_` file as two tracks gives left/right
correlation near 0 at every interleave from 0x10 to 0x4000 (tried on 2 files), against 0.45 to
0.75 for the real pairs. Whether the game spreads them over both speakers is open.

Track order (0 = left) is likely, not proven.

## Sound events (`SOUNDEVE`, 2,279 entries)

Tag `SEVT` (stored `TVES`). All 2,279 parse to the last byte, following
`ERSoundEvent::Load` from the Wii; the order of fields matches the PS2 bytes. PS2 code for this
loader was not read.

| Part | Meaning |
|---|---|
| u32, u32, u32 | Version 1, tag, -1 |
| u32 + chars | Name length with its NUL, name |
| u32 | Bytes that follow |
| u8, u8 | A setting; then a flag. If the flag is 0 the event stops here (8 events, such as `killsound`) |
| 13 bytes | Settings: only their order and widths are known. Includes a byte that is usually 100 |
| u32 | Hit lists |
| per list: u32 + pairs | Pair count, then (stream hash, sample hash) |
| u32 | Hash of a `SOUNDTRA` entry |

Pairs have exactly one non-zero hash. Hashes are the archive `name_hash`, so they join straight
to the table. All 7,686 sample hashes resolve, and all 28 stream hashes do. **Proven.** The Wii
calls a pair `SndEvtHitPatch`.

Events using `playvox` have 3 lists. List 0 is male voices (`...m03`), list 1 is female
(`...f02`), and list 2 was empty in those looked at. Name letters match in over 99% of 3,600
pairs, and pitch agrees, see below. Likely male, female, and something unused.

`python -I tools/audio.py event <iso> <substring>` prints this.

## Ambient scores (`AMBIENTS`, 30 entries)

Tag `SCOT`. All 30 parse to the last byte, following `ERAmbientScore::Load` and
`ERAmbientSound::Load`. Note that the "u32" fields at the top of the Wii code are the data
header: version, tag, id, name length, name, then the total entry size.

After that: 3 bytes, u32 sound count, then for each sound: u32 hash count, six floats, 10 bytes,
then the hashes. Records are not aligned. All 3,051 hashes are samples. The meaning of the
floats and bytes is open.

## Sound tracks (`SOUNDTRA`, 3 entries)

Tag `STRD`: `playfromhitlist`, `playhitpatch`, `playvox`. Same header, then u32 size and u32
words. Words look like a small instruction list (`SndInstruction`); not decoded. They hold no
sample names.

## Coverage

Events and scores name 7,006 of 7,656 samples and 28 of 44 streams. The other 650 samples are
mostly `store`, `ui`, instrument notes and `snapshot`. They are presumably played by name from
code or UI data; not traced.

## How we know it is sound

- **A test that failed.** The first idea was to ask if each block's stored filter fits the
  output best. It scored 99.7% on the right decode and 98.6% on swapped nibbles. It is
  circular, since the residual under the stored filter is the nibbles whatever they are. Dropped.
  First-difference roughness failed too. Prediction with 16 taps did not separate them either.
- **Samples: drift.** The encoder's loop keeps the output on the original, and any decoding
  mistake leaks into low frequencies. Median energy below about 50 Hz: right -26.8 dB, wrong
  filter table -15.3, swapped nibbles -21.6, wrong interleave -16.4. The right decode has less
  drift on 96.3%, 89.8% and 90.9% of the non-silent entries. The rest are mostly footsteps,
  short voices and ambience, and were not examined one by one.
- **Music: clipping.** Drift can't tell because music has real bass (-13 dB in all cases).
  Instead: the right decode clips 0.0000% of samples, a wrong table 1.83%, and it clips less
  than wrong decodes on 44, 44 and 43 of 44 entries.
- **Layout.** At 0x4000 chunk seams the step is 1.00 times typical for the right layout, and 1.30
  when read as two tracks. Left/right correlation is given above.
- **Loops.** Jump from loop end to loop start: median 33. To a block mid-loop: median 471.
- **Voices (scratch `pitch.py`).** Autocorrelation pitch on 120 samples from each list: male
  median 163 Hz, female 276 Hz. That is real speech at about the right speed. It would catch an
  error of 2x in rate but not a small one.
- **Structure.** Corrupting a trailer, end block, flag, size, name, chunk count, end offset or
  track count raises FormatError (10 of 10, scratch `clips.py`).

The 85% pass line in `check` was set after seeing the results; a coin would give 50%.

## Open

- By ear: a sample of the clips was heard on 2026-11-06, and all of those played the right sound.
  Stereo order and looping are still unheard.
- The decoder's rounding (`+32 >> 6`) comes from public SPU descriptions, not from this disc. A
  1-count error would not show in any test here and would not be audible.
- The real SPU applies Gaussian interpolation when pitching; these WAVs are the raw samples.
- Volume, pan, priority and other settings in events and scores.
- Stream flags byte; the 0x18 per-track words; which channel is left.
- `SndInstruction` words.
- How the game loops streams, and where the loop points are held, if anywhere.
- List 2 of `playvox` events. Who plays the 650 unreferenced samples and 16 unreferenced streams.
