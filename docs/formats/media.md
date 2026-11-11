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

`.geo`: `Apt PCGL geometry file\x1A\0`, then pointer-linked records, offsets from the member's start. **Proven** on
all 147 by `apt.py check`:

- **Header:** shape count, shape table offset, bitmap count, bitmap table offset.
- **Bitmap table:** pairs of bitmap character number and sheet number (`001.tgq` is 1). That's how a Bitmap
  selects its sheet.
- **Shape table:** rows of shape character number (always the Shape's own number, in all 1,223), batch count, and
  an offset to that many batch offsets.
- **Batch:** u32 kind, count, vertex offset, ARGB colour. Vertices are x, y float pairs in the shape's own space.
  - **Kind 1:** solid triangles, count triangles (69 batches).
  - **Kind 6:** line segments, count segments of two points: outlines (30).
  - **Kind 5:** textured triangles (1,199). It adds a 4×4 matrix (GL column order) from shape space to bitmap pixels,
    the bitmap's character number, a zero word, and the bitmap's UV rectangle on its sheet, 0x68 bytes in all.
- **The textured fit:** the UV rectangle is the bitmap's area on the sheet, half a texel in from each edge. So the
  bitmap's top left is (u0 × width − 0.5, v0 × height − 0.5), and its size is the rectangle plus one texel. On all
  1,199 textured batches, the matrix maps the batch's points onto exactly 0 to that size, within 0.05 px. A shape
  stretches a 1- or 2-pixel strip by giving the matrix a small scale, such as 0.012.
- **Rows:** V counts rows as stored, with v = 0 the first stored row. `texture.rgba` flips rows for PNG, so a
  renderer samples `texture.stored_rgba`. Checked by eye: with the flip, the item bar drew arrow icons.
- **Bounds:** every point lies inside its Shape character's bounds.

The `.apt` records a layout needs. **Proven** by the same check, 0 failures:

- **Shape (1):** type, signature, bounds (left, top, right, bottom floats), geometry number.
- **Text (2):** type, signature, bounds, font character, alignment (0, 1 or 2; likely left, right, centre), ARGB
  colour, size, three words, initial text offset, variable name offset. The menu's title holds the placeholder
  text `Victorian Love Seat`.
- **Sprite (5):** type, signature, frame count, frames offset, one word.
- **PlaceObject (3), 64 bytes:**
  - Fields: type, flags, depth, character, matrix a b c d, tx, ty (floats), ARGB colour multiply, ARGB colour add,
    ratio, name offset, clip depth, clip actions offset.
  - Flags are SWF PlaceObject2's: move 1, character 2, matrix 4, colour transform 8, ratio 0x10, name 0x20, clip depth
    0x40, clip actions 0x80.
  - **The check:** on all 19,301 placements in every timeline, sprites included, each field without its flag holds
    its default: identity matrix, multiply `ffffffff`, add 0, no name, clip depth −1. Each placed character exists.
  - **The matrix:** x' = a·x + c·y + tx, y' = b·x + d·y + ty. **Likely** for b and c, which are 0 in nearly all
    placements. Mirrored pieces use a = −1 or d = −1.
- **Colour transforms:** multiply `00ffffff` (alpha 0) is how a piece starts hidden and fades in, as in
  `lib_imu_*`.
- **RemoveObject (4):** type, depth. **FrameLabel (2):** type, name offset, then two words.

`.tgq`: a texture without a name. `TXFL` in reading order, 16 zero bytes, then the same fields as an archive texture from
n+0x14, pixels at 36, palette, and an 8-byte trailer of u32 width and height, equal to the header in all 370. All 370 are
8-bit with 256 colours and decode with `tools/texture.py`. Checked by eye on one: a sheet of stone buttons from
`s2c_main_menu`. There are 1,192 Bitmap characters to 370 sheets.

**Scripts: AVM1 bytecode with EA's own opcodes, proven in length.** `apt.py check` sweeps all 2,060 frame and init
actions on the disc, 123,801 instructions. Each ends exactly at its End, with no unknown opcode.
- **Operands of 0x80 and up** are 4-byte aligned words, pointers into the `.apt`:
  - ConstantPool (0x88) is a count and a list of `.const` indices.
  - PushData (0x96) is a count and a list of `.const` indices, where type 4 is a register.
  - DefineFunction (0x9B) and DefineFunction2 (0x8E) give name, parameters, body size and two marker words. Bodies
    follow inline.
- **EA's pushes are not aligned:** byte 0xB5, short 0xB6, float 0xB4, long 0xB7. Neither are the one-byte
  constant-pool operands of 0xA2 (push), 0xAE (push variable), 0xAF (get member) and 0xB0 to 0xB3 (call by name).
- **The names** are from memory of OpenSAGE's table, so **likely**. Four are corrected by use:
  - **0x59 and 0x5A push 0 and 1.** Calls to `getVariable` and `logger` pass their one argument's count with 0x5A.
  - **0xB1 (call by name) and 0xB3 (call method by name) store their result** in the variable named beneath the
    arguments: EA's fused `x = f(...)`. Across the disc nothing after them ever takes a result. Read as a plain call,
    `imuShow_local` ends with `menuOrientation` and its value still on the stack, and attaches `lib_imu_` with no
    corner (the control in `apt_run.py check`).
  - **0x70 pushes `this`** (`this.lockInput = true`, `this.getVariable(...)`).
- **Branches** count from the end of the branch instruction. All 6,484 on the disc land on an instruction start.
- **DefineFunction2's third word** is the register count (low byte) and AVM1's flags (high 16 bits; 0x01 preloads
  `this` into r1). Its parameter list is pairs of register and name.
- **Not swept:** clip actions (257 lists).

`apt.py script <iso> <movie> <out.txt>` writes a listing with names resolved.

Font names in the Font characters are `Arial` (42), `Benguiat Gothic Medium` (33), `_sans` (6),
`Arial Black` (2), `Verdana` (1), none of them the two disc fonts by name.

Open:
- Bytecode: clip actions, and the difference between Apt 6 and 7.
- Button layout, the Text's three middle words and the batch kind 6 line width.
- Mask layers (clip depth, 41 placements): decoded, not applied by `render`.
- The `*.wav` entry's role; perhaps a wildcard for sounds found elsewhere.
- Font name mapping; `AptAuxPCEorGL_Initialize` takes an `AptAuxFontList`.
- For the port this needs an Apt interpreter, or a conversion to Godot scenes plus a script port. SWF tools won't read it.

`tools/apt.py` reads all of this: `check` reads all 147 (1,223 shapes, 1,192 bitmaps, 1,182 sprites, 381 texts, 84
fonts, 950 exports, 370 sheets, 0 failures) and rejects a cut file and a wrong tag; `find` names the movies whose
constants hold a script name; `export` writes a movie's sheets as PNG with its header, exports, fonts and strings.
`check` also proves the `.geo` and PlaceObject decoding above on all 147. Its two controls break the interaction menu:
a UV rectangle moved by one texel, and a scaled placement with its matrix flag cleared. Each is caught.

`layout` writes each export, and `_root` (what frame 0 places), as a node tree in JSON:
- **Sprites:** children by depth, each with its name, matrix, colour transform and its own frame labels with frame
  numbers.
- **Shapes:** their batches, each with its sheet and pixel region.
- **Texts:** font, size, colour and alignment.

`layout` also draws each tree to PNG (nearest texel, texts as their box only). With a label, such as `refresh`, it
draws each export that has the label at the frame before it.

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

**Each piece's layout is proven** (`apt.py layout <iso> tsc5_interaction_menu <dir> refresh`). The root timeline
places nothing, so the scripts attach every piece at run time (`attachMovie`).

- **`lib_imu_dc`** at its shown frame, 10, the frame before `refresh`, reads as the menu:
  - `titlebar_mc`, a title pill: `tb_mid` and two `tb_end` caps, one mirrored.
  - `bkg_mid_mc`, an oval backdrop, with frames labelled `green` and `red`.
  - `IMU_icon`, a 59 × 59 black square at 0.63 scale. **Likely** the slot for the object's picture.
  - `hc_mc`, a count bubble holding `count_txt`.
  - `item_pane_mc`, a column of side panel top, middle (stretched 54×) and bottom.
  - Four arrows, hidden at alpha 0, with frames `inactive`, `on` and `active`.
  - `mask1_mc`, a mask.
- **`lib_imu_ul`, `_ur`, `_ll` and `_lr`:** the same parts arranged for each corner, with the side panel unstretched.
- **The item bars:** `lib_itembar_left`, `_right` and `_dc` hold `highlight_mc`, a tinted gradient with `inactive`
  and `active` frames, `outliner_mc`, the 250 × 25 bar, and `it_mc` with `item_txt`.

**The arrangement at run time.**

**The game's side, proven on PS2** (the decomp kit's `docs/formats/ui.md`):
- **What it sends:** `imuShow(x, y, count)`. From 3 items it follows with `imuRefresh(count, "1", "0")`, which
  highlights item 1.
- **Input:** `imuInput("up" / "down" / "select" / "cancel")`, one per press. The movie answers with `IMU_Highlight` and
  `IMU_Choice`, which the game reads back.
- **The point** is in stage units. In direct control (a stick-driven `DirectInteractor`) it's (320, 254).
  Otherwise it's the cursor's position.
- **`IMU_DirectControl`** is "1" in direct control.
- **The stage** is the whole 640 × 448 screen, 1:1.

**The movie's side, run:** `tools/apt_run.py` executes the scripts on stand-in clips, called exactly as above.
- **The stand-ins** are built from the layout trees, with `_width` and `_height` from the shapes' bounds (**likely**
  as Flash measures them).
- **Frame scripts:** each library piece's timeline scripts run too, on the next frame after `attachMovie` or
  `gotoAndPlay`, as in Flash. The pieces drive the menu themselves:
  - frame 0 calls `_parent.updateItemPane(this)`;
  - the show animation's end calls `_parent.callback("show")`;
  - the `refresh` frames call `updateItemPane`, then `callback("refresh")`, which unlocks input (`lockInput`).
- **What `apt_run.py menu` writes:** for each corner and dc, and 0 to 12 items, every state the up and down keys
  reach (65 cases, 1,305 states). Each state has its arrangement, the highlight, and the state each key leads to. The
  port's `scenes/ui/interaction_menu.tscn` follows that graph.
- **Check:** `apt_run.py check` tests the run, with a control.

- **`init`:**
  - `itembar_height` 25, `itembar_width` 251.2;
  - `itempane_max_width` 270, `itempane_max_height` 210;
  - `x_cutoff_max` 640, `y_cutoff_max` 448;
  - `max_selections` 6;
  - `x_direct` and `y_direct` (640, 420), or (605, 385) when `_platform` is `GC`.
- **`imuShow_local(x, y)`:**
  - Clamps x to [20, 620] and y to [20, 420].
  - In direct control, it uses (`x_direct`, `y_direct`) and attaches `lib_imu_dc`. Otherwise it attaches
    `lib_imu_` + `getOrientation(x, y)`.
  - Attaches as `im`, at depth 100.
  - On `GC` in widescreen, adds 110 to x.
  - Sets `im._x` and `im._y` to the point.
- **`getOrientation(x, y)`:** `ul`, so the menu grows right and down from the cursor. It's `ur` if x + 270 > 640,
  `ll` if y + 210 > 448, and `lr` if both.
- **`updateItemPane`:** attaches up to 6 bars, `itembar_<i>_mc`, into `item_pane_mc`, at depth 50 + i.
  - **Which bar:** `lib_itembar_left` in `ul` and `ll`, `lib_itembar_right` in `ur` and `lr`, `lib_itembar_dc` in
    direct control.
  - **Positions, in the pane:**

    | Piece | Bar x | Bar 0 y, then each next bar |
    |---|---|---|
    | `ul` | 44.4 | 25n + 16, then 25 less (n = bars shown) |
    | `ur` | −38.4 | 25n + 16, then 25 less |
    | `ll` | 44.35 | −63, then 25 less |
    | `lr` | −31.6 | −63, then 25 less |
    | `dc` | −31.6 | −65, then 25 less |

  - **Order:** item 0 is the bottom bar in every corner, and the list grows upward. Bar i asks for
    `getloc:IMU_<offset + i>`, and `IMU_NN` names item NN. That holds if the game's 12 bound names, from
    max(highlight − 6, 0), keep their own numbers: the movie's request always falls inside that window. If the
    names were renumbered from 0, the highlighted bar would show the wrong item past item 6. So the first reading is
    **likely**; ui.md's wording allows both.
  - **The side panel:** its middle is stretched to fit the bars: 25n − 43 tall in `ul` and `ur`, 25n − 45.6 in `ll`
    and `lr`, +3 in `dc`. Its top moves to the end. With 0 or 1 items that height is negative, so the panel
    closes: the middle is set 15 tall and hidden, and so is the bottom. In the lower corners and `dc`, the top goes to
    frame `cvs` when closed and `cvn` when open; the upper corners don't change its frame.
  - **Highlight:** the movie's `item_selected` counts from the bottom bar: `imuRefresh_local(count, hilite,
    offset)` sets it to hilite − offset. That bar goes to frame `active`; its gradient's tint settles at `00a7f155`
    (green), against `00b2d1c9` (pale teal) for the others.
- **Keys (`selectButton`, run):**
  - **Up** (+1) moves the highlight to the next item, up the screen; **down** moves it back.
  - **The ends:** both stop at the first and last item.
  - **Scrolling:** with more than 6 items, the list starts to scroll once the highlight passes bar 5, so the highlight
    rides the top bar going up. 12 items: from item 1, 6 ups reach item 7 with items 2 to 7 shown, and 10 reach item
    11 with items 6 to 11.
  - **`select`:** sets `IMU_Choice` to the movie's offset, which is what ui.md finds the game saving per level and
    handing back as `imuRefresh`'s third argument. The item the game runs is the highlight (`IMU_Highlight`).
- **Scroll arrows:** `arrow_up_mc` shows while items lie above the top bar, and `arrow_dn_mc` while the list is
  scrolled. They're the movie's, set by `showArrows`.
- **The pointer** (`ptr_mc`) goes to the selected bar's `_y + _height / 2`. `callback("show")` and `("refresh")` set it
  to alpha 100.

**Open:** the icon (`IMU_icon`, an `_action_queue` texture per ui.md), the show and hide animations as drawn
frames, and whether the bound names keep their numbers (above).
