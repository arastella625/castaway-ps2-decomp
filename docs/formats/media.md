# MOVIES, FONTS and FLASHES

Notes from agent `media`. Values are little-endian unless stated. Each claim says how far it was
checked: proven (tested on all the data, with a negative control where stated), likely, or open.

Tools: `tools/movie.py`, `tools/font.py`. Survey scripts are in this folder under `scratch/`,
mainly `apt_survey.py`, `apt2.py`, `mv2.py`, `mv4.py`, `fneg.py`.

## Movies (MOVIES.ARC)

Sony PSS: an MPEG-2 program stream with PCM audio in private stream 1. 17 files.

Proven on all 17, which `movie.py check` walks packet by packet (29,955 video and 9,695 audio packets):

- Each file is a chain of packs that lands exactly on `00 00 01 B9`, its last 4 bytes.
- Every pack starts on a 16 KiB boundary. File size is packs x 0x4000 + 4.
- Only four stream ids occur: `BB` system header (1 per file), `E0` video, `BD` private 1, `BE` padding.
- There is one video stream and one audio stream. No subtitle or other private stream.
- In `BD` packets the PES header is followed by `FF A0 00 00` in every packet, then audio bytes.

Video, proven by a start code walk over every picture:

- MPEG-2 (sequence extension present), frame pictures only, 4.8 Mbit/s nominal.
- Every picture has one slice per macroblock row, in order: 28 rows NTSC, 32 PAL.
- NTSC files are 640x448 at 29.97; `_pal` and the language `ea_games_xx` files are 640x512 at 25.
- Every stream ends with a sequence end code. The encoder string in user data is TMPGEnc 2.521 or 2.524.
- `progressive_sequence` is set on all but `s2c_intro_pal` and `s2c_ending_pal`.
- Aspect code is 2 (4:3) for `ea_games*` and `s2c_attract*`, 1 (square pixels) for intro and ending.

Audio opens with a 40-byte header, then samples. The `SShd`/`SSbd` names are the ones Sony's ADS format uses:

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | char[4] | `SShd` |
| 0x04 | u32 | 24, size of the fields |
| 0x08 | u32 | Codec, always 1: 16-bit little-endian PCM |
| 0x0C | u32 | Sample rate: 48000, or 44100 in `s2c_attract*` |
| 0x10 | u32 | Channels, always 2 |
| 0x14 | u32 | Interleave, always 0x200 bytes per channel |
| 0x18 | i32 x 2 | Loop start and end, always -1 |
| 0x20 | char[4] | `SSbd` |
| 0x24 | u32 | Sample bytes; equals what follows exactly in all 17 |

How the audio reading was proven:

- Rate, channels and sample size: every audio packet's PTS matches its byte position to under 0.0001 s
  in all 17. Control: patching the rate to 44100 gives 0.352 s drift; 1 channel gives 3.985 s.
- Interleave and the 40-byte offset: in `s2c_intro`, the jump between neighbouring samples is 4,109 at
  0x200 boundaries at phase 0 and about 506 at other phases. Period 0x100 has no spike. `s2c_ending`
  gives 3,272 against 631.
- Byte order: swapping bytes raises the sample-to-sample step about 50 times.

Control for the packet walk: a PES length off by one, a cut last byte and a removed video packet all raise
`FormatError`.

Audio can outlast video: `s2c_ending` 16.00 s against 15.18 s, `s2c_intro` 61.00 s against 60.46 s. The tail
fades to exact silence, so this is likely padding to whole seconds. `check` allows up to 1 s.

| Files | Length | Note |
|---|---|---|
| `ea_games` | 4.00 s | 120 pictures |
| `ea_games_pal` and 9 language files `_da _du _fi _fr _ge _it _no _sp _sw` | 4.00 s | 100 pictures, 10 different video streams |
| `s2c_attract`, `s2c_attract_pal` | 4.00 s, 3.96 s | |
| `s2c_ending`, `s2c_ending_pal` | 15.2 s | Same audio in both |
| `s2c_intro`, `s2c_intro_pal` | 60.5 s | Same audio in both |

- `s2c_attract` has exactly the same video stream as `ea_games` (hashes equal). Its audio is a different
  file at a different rate, with nearly the same loudness at the end. So the US attract movie looks like a copy of
  the logo. The byte equality is proven; what the picture shows has not been seen.
- The 9 language files share one audio track; `ea_games` and `ea_games_pal` share another.

Open:
- No frame has been decoded. Godot has no MPEG-2 decoder and no ffmpeg was used, so the `.m2v` output is proven
  structurally, not by eye. Any MPEG-2 player can confirm it.
- What differs between the language videos (probably on-screen text).
- How the game picks a file. `TheSimsEAMovieState::Startup` is the place to read.

## Fonts (FONTS.ARC)

Two fonts: `systemfont` (size 12, 94 glyphs, ASCII 0x21-0x7E) and `frankfur` (size 24, 267 glyphs: Latin,
Greek, some punctuation, and a stray U+FF92). Keys are 16-bit code units. Neither has a space glyph.

The Wii loaders were read (`ERFont::Load`, `EFontData/Size/Page/Character::Load`, `EFile::ReadStructure`,
`ERFont::DoDraw`, `GetLineSpacing`) and match the PS2 bytes. Both files parse to their exact last byte.
No PS2 symbol for these functions is named yet, so the PS2 code itself has not been read.

| Field | Meaning |
|---|---|
| u32, `TNOF`, -1, u32, name | Version 10, tag, as for textures. `n` is the end of the name |
| u32 | Bytes left after this field |
| u16 x 2 | Saved class versions, EFontData and base. From `operator>>(EFile, EStorable&)` |
| u32 | Number of sizes; 1 in both fonts |
| u32 | Object count (the first pointer read; non-zero means "read a structure") |
| type table | Entries of u32 type hash, u16 version, u32 count, until counts reach the object count |
| objects | Each object in table order, then one byte, always 0xB7, which the loader discards |
| u32 | Index of the root object; 0, the EFontSize |
| i32 x 6 | ascent, descent, em, tracking, line, space (below) |
| table | Kerning, empty in both |

Type hashes: `0xF76B628F` count 0 (class open), `0xA81AD9ED` EFontSize, `0x4BD04BA7` EFontPage,
`0x598DE3B1` EFontCharacter. The classes follow from the fields each object holds, not from the hash.

Saved tables are u32 capacity, u32 count, then pairs of u32 key and u32 value. Saved pointers are 1-based
indices into the object list. Capacity is 0x7F, 0x1FF, or 1 when empty; likely a bucket mask.

Objects:

- **EFontSize**: i32 point size, i32 row height, i32 snap flag; table of code unit to character pointer;
  u32 page count and page pointers.
- **EFontPage**: u32 hash, i32 width, i32 height. The hash is the name hash of both the texture and the
  shader `font_<name>_s<size>_p0`. Proven: found in `TEXTURES`, sizes equal.
- **EFontCharacter**: i32 left, i32 right, u16 row, u16 page. The glyph is columns left to right, and rows
  `row*height` onward, **counted from the first stored row, which is the bottom of a top-down image**. Only
  page 0 exists.

Proven on all 361 glyphs by `font.py check`: every rectangle fits its page, none overlap, each has ink, and no
ink lies outside a rectangle. Blank space is palette entry 1, alpha 2, not 0, so "ink" means alpha above 2.
Controls: top-origin rows, row height -1 and x shifted by 1 each fail. Rendered sentences read correctly in
both fonts, and the top-origin version rendered gibberish.

Metrics, from `ERFont::DoDraw` (EFontData sits at ERFont+0x14). File order is as listed:

| Name | systemfont | frankfur | Use |
|---|---:|---:|---|
| ascent | 62 | 60 | Vertical placement, em units |
| descent | 7 | 6 | Vertical placement, em units |
| em | 55 | 38 | em units to pixels: size / em |
| tracking | -2 | 3 | Added between glyphs when a pair has no kerning |
| line | 67 | 0 | Line spacing: line x height / (ascent - descent); 0 means 1.0 in place of line |
| space | 30 | 17 | Advance for any code with no glyph |

Advance at the font's own size: draw the glyph at `pen - 1`, then `pen += (right - left) - 2`. A code with no
glyph adds `space * size / em` instead. Then add `tracking * size / em` between glyphs, with tracking rounded first when
snap is 1.

How far: the code is read, and the sentences look right but rather widely spaced. Glyph edges are soft, so that
may be correct.

Open:
- Exact spacing against the running game. Compare with a screenshot.
- Vertical metrics are not used by the sample, which draws one line.
- What ascent and descent mean against the row height; em differs a lot between the fonts.
- Which class `0xF76B628F` is.
- How Apt font names map to these two fonts, see below.

## Flash UI (FLASHES.ARC)

Not Scaleform and not SWF. It is EA's **Apt**, a Flash-derived UI format, with Flash's model of timelines, sprites
and ActionScript bytecode. The Wii symbols agree: `AptFile`, `AptViewer`, `AptAuxPCEorGL_LoadFile`, `FlashBigFile`,
`ERFlash`. The record layout below started from what I remember of public Apt notes (OpenSAGE, for EA's C&C
games) and was then tested against the bytes here.

Proven on all 147 by `scratch/apt_survey.py`, 0 failures:

- Wrapper: version 3, tag `HSLF` (`FLSH`), -1, name size, name, u32 bytes left. `ERFlash::Load` reads key 0x464C5348.
- The body is a **BIGF** archive filling the rest of the file. u32 LE size, then big-endian u32 count and header
  end; entries of big-endian u32 offset, u32 size, NUL name. Members follow each other, padded to 4 bytes.
- Members are `<name>.apt`, `<name>.const`, `<name>.geo`, then `001.tgq`, `002.tgq` and so on, then always an entry
  `*.wav` with offset equal to the archive size and size 0xFFFFFFFF, which holds no data.

`.apt`: `Apt Data:7\x1A\0` (128 files) or `Apt Data:6\x1A\0` (19, all `s2c_comp_*`). Offsets inside are from the start
of the member. The rest is pointer-linked records:

- The movie's offset comes from the `.const`. Offset 12 of the `.apt` is not the movie in general.
- Every character starts u32 type, u32 0x09876543. All of them carry that signature.
- Movie: type 9, frame count and offset, one word, character count and offset, width, height, frame time in
  ms (16, 28, 33, 83 or 20), import count and offset (0 in all files), export count and offset.
- Exports are pairs of name offset and character number: 950 in total, 171 distinct, such as `gadget_square`.
- Frames are u32 item count and offset to item pointers. Items begin with a u32 type.

| Character | Count | | Frame item | Count |
|---|---:|---|---|---:|
| Shape (1) | 1,223 | | PlaceObject (3) | 6,723 |
| Bitmap (7) | 1,192 | | Action (1) | 691 |
| Sprite (5) | 1,182 | | FrameLabel (2) | 503 |
| Text (2) | 381 | | RemoveObject (4) | 231 |
| Movie (9) | 147 | | Background (5) | 147 |
| Font (3) | 84 | | InitAction (8) | 24 |
| Button (4) | 1 | | | |

The numbers are measured; the type names come from memory of the public notes, so they are likely, not proven. Movie is
proven by its fields, and Font by the names it holds. Exactly one Background per file fits too.

`.const`: `Apt constant file\x1A\0\0`, u32 movie offset in the `.apt`, u32 count, u32 table offset, then pairs of u32
type and u32 value. Type 1 (13,989) is a string offset, holding script names like `getVariable`, `SELECT`, `DPAD_UP`.
Type 4 (16,907) is open, likely a register.

`.geo`: `Apt PCGL geometry file\x1A\0`, then a u32 that equals the Shape count in all 147. Binary, with floats visible.
Layout open.

`.tgq`: a texture without a name. `TXFL` in reading order, 16 zero bytes, then the same fields as an archive texture from
n+0x14, pixels at 36, palette, and an 8-byte trailer of u32 width and height, equal to the header in all 370. All 370 are
8-bit with 256 colours and decode with `tools/texture.py`. Checked by eye on one: a sheet of stone buttons from
`s2c_main_menu`. There are 1,192 Bitmap characters to 370 sheets.

Scripts look like ActionScript bytecode with added opcodes, such as `0x96` push, `0x99` jump, and `0xA2`, `0xAE`, `0xB5`.
Likely, not decoded. Font names in the Font characters are `Arial` (42), `Benguiat Gothic Medium` (33), `_sans` (6),
`Arial Black` (2), `Verdana` (1), none of them the two disc fonts by name.

Open:
- `.geo` layout, and how a Bitmap selects its sheet and region. Bitmap numbers exceed the `.tgq` count in 139 files,
  so the number is not a plain sheet index.
- Bytecode: the opcode table, and the difference between Apt 6 and 7.
- PlaceObject, Sprite, Text and Button layouts.
- The `*.wav` entry's role; perhaps a wildcard for sounds found elsewhere.
- Font name mapping; `AptAuxPCEorGL_Initialize` takes an `AptAuxFontList`.
- For the port this needs an Apt interpreter, or a conversion to Godot scenes plus a script port. SWF tools won't read it.

`tools/apt.py` reads all of this: `check` reads all 147 (1,223 shapes, 1,192 bitmaps, 1,182 sprites, 381 texts, 84
fonts, 950 exports, 370 sheets, 0 failures) and rejects a cut file and a wrong tag; `find` names the movies whose
constants hold a script name; `export` writes a movie's sheets as PNG with its header, exports, fonts and strings.

### The interaction menu

**Proven, it's `tsc5_interaction_menu`.** `ActionMenu::SetBackdrop` calls `AptViewer::NewCallFunction` with
`imuSetBackdrop`, and `ActionMenu` builds its localisation keys with the prefix `IMU_`. Of the 147 movies, only
`tsc5_interaction_menu`'s constants hold `imuSetBackdrop`. `ActionMenu`'s constructor loads it through
`UIScreenManager::LoadUIScreen`, and installs `GetVariable`, `SetVariable` and `GetLocalizable` commands for its scripts.

- **The movie:** 640 x 448, 28 ms frames, 2 frames. 24 shapes, 24 bitmaps, 44 sprites, 5 texts, fonts `Arial` and
  `_sans`, and one sheet, `001.tgq`.
- **The sheet:** rounded item buttons (plain white, green-edged and red), left, right, up and down arrows, plus and
  minus icons and a soft backdrop.
- **The exports:** `lib_itembar_left`, `lib_itembar_right`, `lib_itembar_dc`, `lib_titlebar_left` and the corner
  pieces `lib_imu_ul`, `lib_imu_ur`, `lib_imu_ll`, `lib_imu_lr`, `lib_imu_dc`. **Likely**, from these names, it's a list
  of item bars under a title bar, framed by corner pieces, not a pie.
- **The game's calls into it:** `imuShow`, `imuHide`, `imuRefresh`, `imuMore`, `imuInput`, `imuBlank`,
  `imuSetBackdrop`, `imuShowCount`, `imuHideCount`, `imuScrollArrowEnable`, `imuObjectButtonEnable`, `imuClearList`.
  It calls back through `IMU_ShowCallback`, `IMU_HideCallback` and `IMU_RefreshCallback`.

**Open:** where each piece sits. That's in the `.geo` shapes and the PlaceObject records, neither decoded.
