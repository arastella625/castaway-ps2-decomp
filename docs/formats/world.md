# World and data formats

Agent `world`. Everything here was read from the Wii assembly and then tested on the bytes of the
US PS2 disc. `tools/gamedata.py check <iso>` repeats every whole-archive test below, each with a
negative control, and ends with "all checks pass". All values are little-endian unless stated.

Labels: **proven** means a whole-data test passes and a wrong rule fails it. **Code** means it was
read from the Wii code and the PS2 bytes fit, but nothing tested its effect. **Likely** and **open**
mean what they say.

## What a Godot port needs, in order

1. **Name hash and the ARC tables.** Every cross-reference in the game is a hash of a name. The ARC
   tables are enough; the `.IND` files add nothing a port needs except sample rates.
2. **Model scale.** One float per model. Without it nothing but characters has the right size.
   `tools/gltf.py` and `tools/model.py` should apply it. Details below.
3. **`LEVELS.ARC`.** Terrain, every scattered plant and rock, and the build and placement grids.
   Decoded, except for how models with a rig get sized.
4. **`CreateASim`.** The clothing database, costumes and body part tables. Decoded; `sim.py` can
   drop its name guessing.
5. **`DEFAULT.NGH`.** This is where placed game objects are: the lot file holds none. Sections and
   the object type lists are decoded, and so are the positions in `ObjM`: `tools/placed.py` reads
   them and can write a lot with its objects. `Arry` (floors, walls, tile flags) is still open.
6. **`SimsObjects`, `SimsTiles`, `world`.** Object definitions, wall and floor tiles, lighting and
   weather. Tables read, most columns unnamed.
7. **`EDITHTRE`.** Object behaviour: 5,396 SimAntics BHAVs. Chunks and instruction fields decoded; see [bhav.md](bhav.md). This
   is needed for gameplay, not for showing a lot, and needs a VM, so it comes last.
8. **Not needed:** `DATASETS` (copies of other archives, useful only as preload lists) and the
   `.IND` files.

## Name hash

**Proven** on all 37,911 entries in all 23 archives, 0 wrong.

`EResourceManager::CalcId` returns 0 for null or `undefined` (compared without case), and otherwise
calls `EChecksum::ComputeSymbol`. That's reflected CRC-32 (polynomial 0xEDB88320, final inversion)
over folded bytes:

- `a`-`z` become upper case. `A`-`Z` and `0`-`9` stay. **Everything else becomes `_`.**
- If the first character is a digit, the CRC starts as if a `_` came first. The code writes this as
  `table[0xA0] ^ 0x00FFFFFF`.

Controls: plain CRC-32 misses 37,869 names, dropping the digit rule misses 93, and dropping the
`_` rule misses 1,657. So `a-b` and `a_b` collide, as well as `cliffLush` and `clifflush`.

A separate hash: **class keys** in object streams (below) are plain CRC-32 of the class name, case
kept, from `ETypeInfo::Register`. **Proven** on the 5 classes that occur.

## INDEX.IND and INDEX1.IND

**Proven:** all 23 records equal their archive's table exactly, in order: hash, position and size.

The game opens both at start (`allocateGlobalIndex`) so it can find anything without reading the
archive tables. `INDEX.IND` has 21 records and `INDEX1.IND` has `Movies` and `AudioStreams`.

| Type | Meaning |
|---|---|
| u32 | Count of offsets, less one: 2 per record (42, or 4) |
| u32[] | Offsets: name, record, name, record, ..., then the file size |

A record's name is the manager's name. Its first 8 letters upper-cased give the archive:
`AudioStreams` is `AUDIOSTR.ARC`. A record:

| Type | Meaning |
|---|---|
| u32 | 0 marks this long form. `LookupId` also accepts a short form with the count here |
| u32 | Entries |
| u32 | Record size |
| u32 | Low half 1. High half is bytes of extra data per entry: 0, or 4 for `Samples` |
| u32 | Size of the tail: 0, or 320 for `Samples` |
| u32[] | Hashes, sorted |
| (u32, u32)[] | Position and size |
| u32[] | Extra word per entry, `Samples` only |
| bytes | Tail, `Samples` only |

**This is the missing header for audio.** `SAMPLES.ARC` entries have no header of their own. The
tail is five 64-byte `VAGp` headers named `generic NNNNN Hz`: 24000, 22050, 12000, 44100 and
11025. In the extra word:

- Low 24 bits: the length of the sound data. **Proven:** on all 7,656 it's 64 to 112 bytes less than
  the entry's size, which is padding.
- Bits 24-30: 0 to 4 on every entry, so an index into those headers. **Likely**; rates weren't
  tested against audio.
- Bit 31: open. Set on 144 entries, but on only 36 of the 417 names with `loop`, so it isn't simply "loops".

## DATASETS

**Proven:** all 693 parse to the last byte, and all 15,774 items inside are byte-for-byte copies of
the entry with the same hash in their own archive. None differ and none are missing.

So these are **not behaviour data**, as the task supposed: they're preload bundles, laid out for one
seek. The port can skip them. They are useful as a list of what a lot or object loads.

`ERDataset::Load`. The header is `EDataHeader`: version 10, tag `TSTD`, -1, name size, name, bytes
left. Then a char[64] name, a u32 item count, and per item:

| Type | Meaning |
|---|---|
| char[32] | Manager name: `Textures`, `Models`, `Datasets` and so on |
| u32 | Name hash |
| u32 | Size |
| u32 | Padding bytes to skip, so data lands on a 2048 or 4096 boundary |
| bytes | The resource file, unchanged |

Kinds found: Shaders 3,703, Textures 3,608, SoundEvents 2,273, Animations 1,613, Models 1,507,
EdithTreeSets 1,381, Datasets 818, Characters 520, EffectsAttachments 207, Flashes 68,
SocialAnimationSets 37, Levels 34, SoundTrackDatas 4, Fonts 1. A dataset can hold other datasets.

## EDITHTRE: object behaviour

**Proven:** all 694 walk to the last byte: 5,396 BHAVs and 61,593 instructions, and every BHAV's
size is exactly its header plus 12 bytes per instruction.

`EREdithTreeSet::Load`. Header version 3, tag `HTDE`, -1, name, bytes left. Then a real Maxis IFF
file: the 64-byte banner `IFF FILE 2.5:TYPE FOLLOWED BY SIZE`, then chunks:

| Type | Meaning |
|---|---|
| char[4] | Type. Only `BHAV` occurs |
| u32 **big-endian** | Size, including these 76 bytes |
| u16 **big-endian** | Id |
| u16 | Flags |
| char[64] | Name, like `Function - Init` |

BHAV body, little-endian: u16 signature (0x8002 on 5,390; 0x8001 and 0x8000 on 3 each), u16
instruction count, 8 more header bytes, then instructions. An instruction is u16 opcode, u8 true
target, u8 false target and 8 operand bytes. **Proven**: `tools/bhav.py` tests this on every tree,
and the interpreter is matched. 587 different opcodes occur, the most common being 2, Expression
(36,093 uses). The set's name matches the object's `ResFile` row (below). What the instructions
mean is in [bhav.md](bhav.md).

## Model scale: how objects get their true size

**Proven.** This closes the open question. The doc's statement that models have "no scale of their
own" is wrong: there's a float in every model header, at the place `ERModel::LoadFromFile` reads
into `+0x70`.

    metres = stored integer x model scale

The header after the `n+0` size field is: u32, 48 bytes, u8, a u32 count of 64-byte records, then
counted arrays of BSplineVolume, ENDummy, ENCamera and SimsLightInfo, then u8, **f32 scale**, u32
submodel count. With empty arrays the scale is at n+0x4E, just before the n+0x52 the doc already has.

Array items:

| Item | Layout |
|---|---|
| BSplineVolume | u32 version 0, char[128], 5 x u32 (open, x, y, z, frames), then frames x x x y x z x 12 bytes if frames > 1 |
| ENDummy | u32 0, char[64], u32, u32 n, n x 80 bytes |
| ENCamera | u32 0, char[64], u32, u32 n, n x 96 bytes |
| SimsLightInfo | 28 bytes |

Tests:

- All 2,404 models walk to a scale that's an exact power of two, from 2^-21 to 2^-5, or 1.0 on the 21
  models with an empty box.
- **All 1,235 models with geometry not at 1/4096 have a largest coordinate between 16,384 and
  32,768.** So the exporter picked the scale to fill the 16-bit range. Real size was then rounded to
  fit, which is why sizes looked random.
- All 33 lots: the union of the lot's float bounds equals the model's box times its scale, within 1
  cm. With twice the scale, 0 of 33 fit. The float bounds were written separately, in metres.
- All 868 `af_` and `am_` models are at 1/4096. So are 280 others, mostly props and trees that fit
  in that range or share rigs. 4096 per metre was never a character rule, just the common value.

Sizes in metres (x, y, z): `doorStylishDoor1` 1.00 x 0.13 x 3.00, one tile wide and full wall height.
`bambooWorkbench` 1.43 x 0.91 x 1.33. `isHamResource` 0.38 x 0.50 x 0.32. `icon_simoleon_x_x` 0.58 x
0.06 x 1.00. `torch` 0.09 x 0.09 x 0.47. `skydome` 1,238 across. The end of the file holds, in stored
units: sphere (4 floats), a box (6), a second box (6), then 7 flag bytes, 71 bytes in all. The second
box is the whole-number range of the vertices.

**Code:** `EIStaticModel` and `EMidLotInstance` apply this as a uniform scale on the draw matrix.
That was read on Wii only. The PS2 bytes prove the numbers, not the PS2 draw path, though the bounds
test shows they mean the same thing there.

## Object streams

`EStream::ReadStructure`, used by lots. **Proven** on all 33.

| Type | Meaning |
|---|---|
| u32 | Number of objects. 0 is a null pointer |
| per class | u32 key, u16 version, u32 count, until the counts add up |
| per object | Its `Read`, then one byte, always 0xB7 |
| u32 | Index of the root object |

A pointer inside the stream is a u32: 0 for null, otherwise object index + 1. A class's version is
set from this table and each `Read` checks it, so files carry the version they need. That's why
lots "have no common header".

## LEVELS.ARC: lots

**Proven:** all 33 parse to the last byte, and the name inside equals the entry's name. Every lot
is laid out the same way.

`ERLevel::AllocAndLoadLevel` reads a u32 (the position of the tail, always file size - 8), the
stream, then a final u32 of 0. Classes: `EInstance` v1, `EStorable` v0, `EResource` v0 (no objects,
just versions), then 1 to 3 `EIStaticSubModel` v0 and one `ERLevel` v7, which is the root.

**A lot places terrain and scenery. It places no game objects.** Those are in `DEFAULT.NGH`.

`EIStaticSubModel`, the terrain:

| Type | Meaning |
|---|---|
| u32 | Flags, 0 on all |
| u32 | Level pointer |
| u32 | Bound tree pointer, 0 on all |
| u32 | 15 on all; open |
| f32 x 6 | Bounds in metres: min, max |
| u32 | Model hash: the lot's model of the same name, `lot_26` in MODELS |
| u32 | Submodel index |
| f32 x 4 | Bounding sphere |

No transform: terrain is already in lot space, in metres after scaling. `tools/model.py` doesn't
split by submodel yet, so use the whole model.

`ERLevel` v7:

| Type | Meaning |
|---|---|
| char[] | Name, NUL-terminated |
| u32 | File version, 47 on all. `>= 39` adds mid-lot objects, `>= 40` adds the grids |
| u32 x 3 | 0 on all |
| u32 | Instance tree size, 0 on all |
| u32 | Open: 1 on `casroom_01` and `worldmap`, else 0 |
| u32 + refs | Submodel list 1, empty on all |
| u32 x 2 | 0 on all |
| u32 + refs | Submodel list 2 |
| u32 + refs | Submodel list 3 |
| u32 + refs | Instance list. A lot's submodels are split between this and list 3 |
| u32, then items | Mid-lot objects |
| grid x 3 | Tile grids |

The lists sort a lot's submodels into groups; which group means what is open.

### Mid-lot objects

`MidLotObject`, 60 bytes, 6,284 in all. Every one names a real resource. This is the scenery: grass,
ferns, trees, rocks, sky, clouds, fog planes and particle anchors.

| Type | Meaning |
|---|---|
| f32 x 3 | Position in metres. 5,692 of 6,284 lie within 1 m of the lot's bounds; the rest are scenery beyond it |
| f32 x 3 | Rotation in **degrees** |
| f32 x 3 | Scale |
| u32 | Character (rig) hash, or 0; set on 2,609 |
| u32 | Model hash |
| u32 | Animation hash |
| u32 | Flags (0, 9, 257 and a few others); the magic cookie, open |
| u32 x 2 | 0 on all |

**Code**, from `EMidLotInstance::Init` and `SetupModel`:

    quaternion = SetEuler(rotation in radians) = Qz * Qy * Qx, X applied first
    matrix = rotation, then PreScale(scale), then PostTranslate(position)
    if character hash == 0: PreScale(model scale)

So **scale here multiplies the model's own scale**. `placed.py lot` follows this, and `lot_01`
looks right by eye: cliffs, palms and plants sit as a coherent cove. That is **likely**, not a test.
The wind plants are the 2,272 with rig `windyPlantSkel`.

### Tile grids

Three `BitArray2D`: u16 (1 on all), u8 x offset, u8 y offset, u8 width, u8 height, then
`ceil(width / 8) x height` bytes, low bit first. Outside the rectangle a bit reads as 0.

**Code**, from `cFixedWorld::Load`, which folds them into per-tile flags. It calls `Get(y, x)`, so
the grid's first axis is tile Y:

| Grid | Effect |
|---|---|
| 0 | Sets flag 0x01 |
| 1 | Clears flags 0x10 and 0x20, which the load otherwise sets on every tile |
| 2 | Sets flag 0x80 |

`IsBuildable` is 0x01 set and neither 0x20 nor 0x80. `IsPlaceable` is neither 0x20 nor 0x80. So grid
0 is where you can build, grid 1 where you can place, and grid 2 an exclusion. Lots are up to 63 by
63 tiles. `### Arry` below tests these meanings against the stored flags and the objects.

## DEFAULT.NGH: placed objects

**Proven** for sections, chunks, `objt` and the `ObjM` and `Arry` streams.

The starting save, on the disc root and outside any archive. Sections sit on 2048-byte boundaries:

| Type | Meaning |
|---|---|
| u32 | Kind: 0, 1, 2 (thumbnails, a different codec) or 3 (a house) |
| u32 | Header size, 0x1C |
| u32 | Size unpacked |
| u32 | Size packed |
| u32 | Checksum |
| u32 | Version, 11 |
| u32 | Items in the section |

Bodies are LZSS: Okumura's, 4096-byte window of zeros, writing from 0xFEE, lengths 3 to 18
(`LZSSCompressor::Decode`). All kind 0, 1 and 3 sections come out at exactly their stated size.

There are 28 house sections, and `urbzui/HouseData` has 28 rows, so **likely** one each. A house
section starts with type tags (reversed), start offsets (which count the header), lengths and u16
ids. All 28 hold the same five chunks:

| Chunk | Id | Meaning |
|---|---|---|
| `Arry` | 11 | `kExtFloorRsrcID`: floor. Starts u32 0, width, height |
| `Arry` | 12 | `kExtWallRsrcID`: walls |
| `Arry` | 8 | `kFlagLayrerRsrcID`: tile flags |
| `objt` | 0 | The object types on the lot |
| `ObjM` | 1 | The objects themselves |

The first word 0 sends `Arry` to `_c2DArray::DoStream`: see `### Arry` below.

`objt` is u32 0, u32 2, `tjbo`, then entries of u32 GUID, 6 x u16 and a name padded to even length,
ending in a zero word. The fifth u16 counts up. **Proven:** 1,134 of 1,135 GUIDs are in
`ObjDefinition`, and flipping one bit finds 0. The one miss is 0x334945B8 in the last house.

### ObjM

`tools/placed.py check` reads all 2,611 objects in the 28 houses. **Proven:** every object ends
exactly on its mark, except two of type 9 that the game throws away, and every house ends on a zero
mark. A stream moved by one byte, a stream cut short, an empty type list and reading every object as
single-tile are all rejected. An earlier version of this page read an extra i32 after the "always 1"
field, which wasn't in the code. The value it kept finding was the attribute count plus a bit, and
every field after it was misread, including the variables.

The chunk is u32 0, a u32 version (0x78 to 0x82), `MjbO`, then a `ReconBuffer` stream from offset 12.
Bits run most significant first within each byte. A compressed integer (`ReconCmprInt`) is:

| Bits | Meaning |
|---|---|
| 1 | 0 means the value is 0, and nothing follows |
| 2 | Index into the widths for this integer size |
| that width | The value, sign-extended |

The widths are 2, 4, 6, 8 for 8-bit integers, 5, 8, 13, 16 for 16-bit, and 6, 11, 21, 32 for 32-bit.

A **mark** drops to a byte boundary and holds a u32 giving the position of the next mark, counted
from offset 12. The reader can skip what it doesn't understand.

| Field | Meaning |
|---|---|
| i8 | Non-zero when the integers below are compressed |
| i16 id, i16 type, repeated | Ends at id 0. Type is a 1-based index into `objt` |
| mark | |
| per object, then a mark | See below. The last mark is 0 |

An object is `cXObject::ReconStream`, read from the PS2 code, then one word from `DoStream`:

| Field | Meaning |
|---|---|
| 4 x i32 | Footprint: minimum x, minimum y, maximum x, maximum y |
| i32, i32 | Centre y, centre x |
| i32 | Level. 1 on every object |
| i16 | Always 1 |
| i16 count, i16 each | Attributes (scope 0) |
| i16 count, i16 each | Semi-global attributes (scope 2) |
| i16 count, i16 each | Static attributes (scope 0x40) |
| 8 x i16 | Temps |
| i32, i32, i8 | A 72-bit set of variable keys |
| i16 per key set | The variables, lowest key first |
| i32 | Stack frames. 1 on every object |
| i32 | Memory reserved for the stack. 512 on every object |
| frames | See below |
| i32, i32 | Relationships: -1, then 0 entries on every object |
| i16 count, (i16 0, i16 object ID) each | Object slots |
| i16 count, i16 each | Scope 0x1B's array. Empty on every object |
| 6 x i32, i16 | Multi-tile objects only: tile offsets, then the lead tile's ID |
| i32 | `ObjectModule`'s word for this ID: low 16 bits idle status, high 16 Sim flags |

A stack frame (`StackElem::ReconStream`):

| Field | Meaning |
|---|---|
| i16 | Stack object |
| i16 | Tree ID |
| i16 | Node |
| i8, i8 | Local count, parameter count |
| i16 each | Parameters, then locals |
| i32 | Waiting flag |
| i16 | Whose trees: the owner's saved number from `objt` |

Multi-tile means `ObjDefinition +0x14` is non-zero. A multi-tile object of type 8 is a `cXPortal`, which
has more fields, but no house has one. `objt`'s six u16 are an i32 init tree version, an i32 main tree
version, an i16 saved number and an i16 type.

**Proven, on the 2,608 objects with a definition:** attribute counts all equal `ObjDefinition +0x74` (0
means 8), and 292 fail at `+0x72`. The semi-global and static lists fit the rules in
`docs/formats/bhav.md` except on 319 objects, where they're one shorter (one type's static list is 0
against 1). So the definitions gained an entry after these houses were saved. The game allocates by
the definition, then overlays what's saved, so the rest stay 0.

**Proven:** each object's one stack frame is at node 0 in its main tree, entry point 1 of its
`ObjFnData` (`docs/formats/bhav.md`), on 2,607 of 2,608, and no frame is at entry point 0, 2 or 3. The
exception is one Conch, saved on the semi-global main tree.

Variable 0xB is the object's own ID on all 2,611. Non-zero attributes are on 168 objects, semi-global
ones on 1,488 and static ones on 407. Temps are non-zero on 28. This is what's saved, not what a running
lot ends up with: 2,027 objects reset on load, as `docs/formats/bhav.md` explains.

Positions are in sixteenths of a tile, and a tile is a metre. Controllers such as `The Weather God`
sit at -16, -16, off the lot: 365 objects do, leaving 2,246 on lots.

Variable 1 is the direction, in eighths of a turn, from the code. It's 0 on 1,287 objects and 2, 4 or
6 on 1,322, so placed objects face the four compass directions. Which way that turns is not shown by
anything on the disc so far. Variable 0x1D is the room: -5 on the 365 objects off the lot, 0 elsewhere.

**House N is `lot_(N+1)`, counting the first house as 0**, and x and y are not swapped. HouseData's
row 0 is a placeholder, which fits. Objects land on set cells of their lot's grids at a mean of
0.822, and 24 of 28 lots are above 0.75. Pairing with other lots gives 0.40 to 0.50, and swapping x
and y gives 0.479. I found the offset after seeing the data, but the control supports it. Houses 12
and 18 are low, at 0.59 and 0.63, for reasons not found. What each of the three grids means is
in `### Arry`; this score picks the best-fitting grid per lot.

**The last house is the exception.** It reads as `lot_28` by position, but it looks like an older
`lot_27`: grid 1 agrees 0.908 with `lot_27` and 0.467 with `lot_28`. The tools still call it
`lot_28`, so don't pair that level with this house's objects.

The objects have no height. `placed.py lot` drops each onto the terrain's highest surface over its
one-metre cell, which is a stand-in for what the game does and will be wrong on steep ground.

### Arry

**Proven** for the stream, and for bit 0x10 meaning "not routable". **Likely** for the mesh surface
bit. Other results are labelled below.

`tools/lotgrid.py check` decodes all 84 chunks (ids 8, 11 and 12 in each of 28 houses; there are no
other ids) and each ends on its exact last byte. A stream moved by one or two bytes, cut short, or
with one row too few is rejected on all 84. All 11,073 pad bytes are zero.

From `_c2DArray::DoStream`, reached through `ReadFromDisk` with a plain, uncompressed `ReconBuffer`:

| Type | Meaning |
|---|---|
| u32 | 0. A non-zero first word is a raw form, which no chunk uses |
| i32 | Rows: `GetXSize`, the game's tile X, `placed.py`'s y |
| i32 | Columns: `GetYSize`, the game's tile Y, `placed.py`'s x |
| i32 | Item size: 1, or 8 for walls |
| i32 | 1: packed |
| packets | Until rows x columns x item size bytes are out |

A packet is a u16. Its top bit means a run: one byte follows, repeated by the low 15 bits. Otherwise
the low 15 bits count literal bytes. If the position in the chunk is then odd, one pad byte follows.
Runs work on bytes, not items. Items are row after row.

| Id | Holds |
|---|---|
| 8 | u8 tile flags |
| 11 | u8 floor: 38 on 85,390 tiles, 0 on 6,939 (most on the border), 63 on 2,284, 32 on 19 |
| 12 | 8-byte `TileWallStorage` |

#### Tile flags

| Bit | Meaning | From | Status |
|---|---|---|---|
| 0x01 | Buildable: `IsBuildable` | Grid 0 at load | Code |
| 0x02 | Set on every tile off the array's border | All 94,632 tiles | Pattern proven; reader open |
| 0x04 | Outside: `IsOutside`. Set on every tile | All tiles | Code |
| 0x08 | Roofed: `HasRoof`. Never set on disc | `UpdateRoomStyles` | Code |
| 0x10 | **Not routable** | Grid 1 at load | Proven |
| 0x20 | Not editable: `MayEditTile`, `IsPlaceable` | Grid 1 at load | Code |
| 0x40 | Read by `cXPerson::GatherInteractions`. On 32 tiles of `lot_08` | Disc | Open |
| 0x80 | No building or placing: `IsBuildable`, `IsPlaceable`, `TestPlace` | Grid 2 at load | Code |

**The three level grids are this array's flags.** Load replaces those bits on every tile, with the
same logic in the PS2 code at 0x1C4E30 and Wii `cFixedWorld::Load`, so grid 1 alone decides bit 0x10
when a lot loads. `lotgrid.load_flags` does the same.

The bytes back this up. Flags on disc agree with their own lot's grids 0, 1 and 2 at 0.994, 0.977
and 0.974. Other lots give about 0.88, 0.60 and 0.80, and swapping the axes 0.91, 0.66 and 0.84. 0x10
and 0x20 always move together. The rest looks stale, as if the disc's flags were saved from older
grids. **Likely**, not proven, that the grids win in play: Load skips them if the level's grids
aren't ready, and nothing here watched a running game. That the flags were plainly made from grids
is good evidence that they are ready.

#### Where a Sim can walk

`lotgrid.walkable(lot)` gives the tiles with bit 0x10 clear after load. The game tests this bit in
three places:

- `TILEROUTABLE`, for room routing: bit set means no. On PS2 it's `0x1CA948`. `bhav.md`'s Routing
  section says what else a route reads.
- `CollisionMap::IsLocationGood`, for placing: bit clear means yes. If it's set, the caller can ask
  for a mesh test. The same shape is on PS2 near 0x315850.
- `CollisionMap::IsMoveBlocked`, for each step of a move: a ghost ignores the bit. Otherwise a set
  bit blocks, unless the Sim is under direct player control. Then a ray goes down to the outer lot
  mesh, and the move is allowed if the surface it hits has bit 0x10 set. The same shape is on PS2
  near 0x316290. On Wii the function then blocks on a wall that isn't a door or gate.

So the tile answer is exact for autonomous Sims and too strict for the player, who can also stand on
routable mesh over tiles marked blocked. **Likely**, that mesh bit is bit 0x10 of a vertex's position
flags in the lot's model: 82% of vertices with it sit on walkable tiles, against 47% on the wrong
lot and a 42% base rate. I took the bit from the code's mask but tested it after seeing the data,
several other bits score well too, and the PS2 raycast's source of the word wasn't followed. Only
this score supports it.

The data agrees that walkable means walkable, though the code told me what to expect. 82.4% of
on-lot objects stand on walkable tiles, against 41% to 49% on wrong lots, 47.8% with axes swapped and
a 39.7% base rate. The others may be scenery beyond the paths, which wasn't checked. In 20 of 28 lots
the walkable tiles are one connected region. In the rest but `lot_06`, one region holds at least 93%
of them. `lot_06` has two, of 2,814 and 323 tiles.

Not in the tile answer:

- **Objects.** Their footprints block at run time (`CCollisionGrid`). Not decoded here.
- **Scripts.** Wii `TryGenericSimCall` call 51 sets or clears bit 0x10 in a rectangle at an object,
  turned by its direction. Which trees use it, and its number on PS2, are **open**.

#### Walls

Byte 0 is the wall bits, read by `HasWalls`. Byte 1 is placement, bytes 2 and 3 styles, and 4 to 7
patterns (`TileWalls`, from the code). Bit 1 is the edge toward the previous row and bit 2 toward the
previous column. Bits 4 and 8 are those edges seen from the other tile, and 0x10 and 0x20 are
diagonals.

**Proven:** lots have almost no walls. 18 lots (`lot_11` to `lot_28`) hold one edge, between (45, 6)
and (45, 7), and both tiles record it. No other tile has a wall bit, and none has a diagonal. Its
pattern, 0x2A, shows on both sides. So `wall_edges` returns one pair at most, and walls don't shape
the lots. **Likely** inert: the other bytes, non-zero on up to 899 tiles a lot with no wall bit, in
blocks like rows 2 to 22, columns 5 to 21 of `lot_02`. They look left over. **Open:** what the lone
edge is. Patterns stay under `WallTile`'s 95 rows, which fits but isn't a test.

#### Floors

**Open.** 32, 38 and 63 don't index `FloorTile`, which has 5 rows, so `IsGrassAt` is false for those
tiles if the PS2 code matches Wii. No walking code above reads this array.

#### The last house

**Disagreement with "House N is `lot_(N+1)`".** It holds for houses 0 to 26, but house 27 looks like
an older `lot_27`, not `lot_28`. Its flags match `lot_27`'s grid 1 at 0.908 and `lot_28`'s at 0.467,
and 85% of its objects are on `lot_27`'s walkable tiles, while `lot_28` isn't among the top four.
HouseData's rows 1 to 27 are used up by houses 0 to 26, leaving none for it. `read_arrays` still
names it `lot_28`, as `placed.py` does. `walkable` follows `lot_28`'s grid 1, so it's right for that
level, but don't pair that level with this house's objects.

### Stand-ins in `placed.py lot`

These make a lot look right and are **not decoded**:

- **Palm crowns.** `isPalmTreeLeaf` is one frond with its own rig. Scanning the whole disc image
  for its name and hash finds only archive tables and one preload list, and the executable has no
  such string, so how the game attaches leaves to `isPalmTreeTrunk` is **open**. The tool repeats
  the frond nine times at `treeanimate3`, 4.12 m up, lifted 20 degrees. The count and lift are guesses.
- **`source_tree_coconutpalm`** names only a rig and an action queue, so it uses
  `isPalmTreeTrunkEdithFruitFull`, chosen by name.

The port also blocks tiles under placed objects over 0.5 m tall. Which objects really block is
**open**.

### Purple terrain

Seven lots (`lot_02`, 04, 15, 16, 22, 23, 30) hold meshes on the shaders `lambert1` and `phong1`,
which are Maya's defaults and point at the 16x16 purple texture `missingshader`. This is on the
disc, not a decoding fault. **Proven:** all 354 such batches are type 0x16 (no normals, with slot
4), and on `lot_16` they lie on the ground, a median of 2 mm from it. Their slot 4 bytes are grey,
all four alike, from 0 to about 128.

**Likely** these are an overlay faded by vertex alpha: the only other terrain of type 0x16 is 13
meshes of `global_transparent` on a white texture, plus `Lava` and `templeBigStones`. **Open:** what
the game draws. `placed.py lot` leaves them out, and `PLAYTEST.md` step 15 looks for them.

## QUICKDAT.ARC

**Proven:** all 7 images end exactly where their directories say, in all languages. Every pointer
word lands inside the image, tables are sorted, and all row names read.

A quickdata file is an `XImage` (`ReloadXImage`): a memory image whose pointers are offsets, with a
list of where the pointers are. That list is what makes these tables readable without knowing their
struct layout. Each `EXPORT_DIRECTORY` is 6 words: 0x18, image offset, image size, fixups offset,
fixups size, base (0). The first directory's image offset also marks the end of the directories, so
`(offset - 24) / 24` is the language count: 21, or 0 for `world`. Directory k is language k.

Fixups, read as words, by low 2 bits (`applyFixups`):

| Bits | Meaning |
|---|---|
| 0 | One pointer at this offset |
| 1 | Run: `word >> 2` pointers, starting at the offset in the next word |
| 2 | Two pointers at `word & ~3` |
| 3 | Three pointers at `word & ~3` |

A language image loads right after the main image, rounded up to 4. It's an array of pointers to
UTF-16 strings, and main-image pointers past the end land in that array. Language 0 and 1 are
English-like, then 2 French, 3 German, 4 Italian, 5 Spanish, 6 Dutch, 7 Danish, 8 Swedish, 9
Norwegian, 10 Finnish. Language 20 is `TEMP` placeholders, and 12 and 16 are blank here. Which of 0
and 1 is US English is open; tools use 0.

Image: u32 1, name pointer, u32 table count, 12 bytes of 0, then 20-byte table entries: name, rows,
row-name array (0 if rows have no names), count, stride. Lookups are binary searches with `strcmp`,
so tables and row names are sorted. A `VECTOR<T>` is one pointer, with its count in the word before
the data.

| File | Tables |
|---|---|
| `CreateASim` | QDLightSet, Sim::BodyPartTypeInfoTableTSC6, Sim::CostumesTSC6, Sim::NPC, Sim::SimPartsTSC6 |
| `SimsObjects` | 25, including ObjDefinition (1,335), ResFile (698), Plans, Ingredient, FoodResult, ClothingItems, AnimRef, TokenCatalogData |
| `SimsTiles` | FenceData, FloorTile, WallTile and their sets |
| `urbzui` | HouseData (28), goals, UI strings |
| `SimsCareers` | Career (11), WStringSet |
| `SimsUI` | LanguageNames, Shift_JISMapping |
| `world` | WorldData, weather, lighting, fog, rain, wind |

`gamedata.py dump <iso> tables` lists all of them. Dumps mark pointers: `'text'`, `L'wide text'`, or
`@offset`. A number is shown as a float if it would make sense as one, so some integers and hashes
print as floats. Known hashes print as names.

### CreateASim

The structs in `src/game/cas/SimBodyPart.h` and `CasSimDescription.h` fit these tables exactly.

`Sim::SimPartsTSC6` has rows `female` and `male`, each 18 vectors. Slots 0 to 12 are clothing set
lists by body part database index: head, facialFeature, hair, glasses, torso, lowerBody, shoes, hat,
necklaceEarring, armAccessory, belt, addOnUpperBody, addOnLowerBody. Slots 13 to 16 are tattoos by
location and 17 is skins, matching `CasSimTextureInfo` at +0x34 and +0x44.

A clothing set is `BodyPartGeometryTSC6`: name, model hashes, texture variation names, flags, hit
points per state, armour. **Proven:** all 1,416 model ids resolve to names, and all 987 torso and leg
`{model}_{variation}_1` textures exist. 457 clothing sets in all; `dump <iso> clothing` lists them.

`Sim::BodyPartTypeInfoTableTSC6` is one row, `CasSimTypeInfo`: a vector of 16 `BodyPartTypeInfo`
(`dump <iso> bodyparts`), type codes `am` and `af`, then `rfl`, `game`, `icon`, body types `normal`,
`skinny`, `ultraskinny`, and skin hit points 50. These are the tables seen in RAM.

| Part | Model codes and styles, with damage states |
|---|---|
| torso | `ti`: `fl` 2, `mr` 2 |
| sleeves | `slti`: `ln` 2, `sr` 2 |
| lowerBody | `lb`: `ln` 2, `cp` 2, `sr` 2 |
| shoes | `sh`: `norm`, `d1`, `d2` |
| hair | `hh`, `hb`, `ha1`, `ha2`, `ha3`: `sh`, `shm`, `ln`, `lnm`, `fl`, `flm` |
| hat | `ht`: `norm`, `back` |
| facialFeature | `ff`: `norm`, `sh`, `fb`, `lb`, `bb` |
| armAccessory | `bc`: `left`, `right`, `both` |

### Which torso, sleeve and leg styles go together

This closes that open question.

- **Sleeves have no clothing list of their own.** `SimBodyPartSleeves::GetBodyPartDBIndex` is 4, the
  torso. A torso set's models are always, in order: `ti_X_fl`, `ti_X_mr`, `slti_X_ln`, `slti_X_sr`
  (**proven** on all 210 models in torso sets, with `-` where one is missing).
- **Sleeves copy the torso:** same set, style, texture and damage state (`Validate`), with model
  index 2 + style. So torso `fl` always pairs with sleeve `ln`, and `mr` with `sr`. That's the
  observed Ranger outfit: `ti_ranger_fl`, `slti_ranger_ln`.
- **Set flag 0x40 means sleeveless:** sleeves switch to the nude set.
- **Legs are free.** `lb` model index is the style, and nothing ties it to the torso. Style is a
  choice per set; a missing model (id 0) means that style isn't available, and required parts reject
  it.
- **Belts** are hidden when torso style 0 has flag 0x1000, or style 1 has 0x2000.
- **Hair** set flags: bits 1 to 5 mean generic for style 1 to 5 (from the header comment), and
  `generic` models fill the gaps: `af_hh_bob_sh` uses `af_hh_generic_shm` for the next style.

`Sim::CostumesTSC6`: 9 rows (chef, default, doctor, exec, mechanic, musician, naked, ranger, teacher),
each with two vectors of 13 set names, one per part: male, then female, picked by Sim flag 0 which
also picks `am`. An empty name leaves the part alone; `ChangeToCostume` looks the name up and sets
it. So `ranger` is torso, lowerBody and shoes `ranger` and the rest `none`. `default` also sets hair
(`short`, `wavy`) and facial hair.

`Sim::NPC`: 30 rows with flag words, two hashes (some are `npc_*` resource names) and a display name.

### SimsObjects

- **ObjDefinition**, 196 bytes, is this game's OBJD. **Proven:** the GUID is at +0x1C. It starts with
  version 138. Other fields are open; PC field lists are a good lead.
- **ResFile**, 72 bytes, is one object's old `.iff` as a row: 14 vector pointers and a name hash at
  +0x0C. **Proven:** the hash equals the row name's `CalcId` on 694 of 698, and those 694 are the 694
  Edith tree sets, one to one. 597 also have a dataset of that name. `0x53545223` in its data is
  `STR#`. Vector +0 holds ObjDefinition pointers; the others are open.
- **ObjDefinition::ResData**: interactions, with names like `Fire - Test` and action hashes.
- **Models for objects:** no model hash appears in ObjDefinition. The chain, **proven** to connect
  by names and hashes: the GUID at ObjDefinition +0x1C; the ResFile whose vector +0 points at that
  ObjDefinition row (1,335 of 1,335 rows); the ObjDefinition::ResData row with the ResFile's name;
  then resource hashes from word 7 of that row, of which word 8 is usually the model. Of 2,611
  placed objects, 2,610 have a ResFile, 1,576 reach a model or rig, and 1,448 of those are in
  `MODELS.ARC`. `placed.py lot` draws only those: rigged ones in `CHARACTE.ARC` are left out.
  911 have no ResData row, mostly invisible helpers (`portal_basic`, `spawner_object`), though
  `plumbing_hottub_hotspring` is one too. 123 have a row with no model (`source_tree_koa`).
  **Open:** models shared by many objects (`isTreeSkeleton`) suggest variants chosen at run time.

## Disagreements with docs/ASSET_FORMATS.md

- "Other models hold plain integers with no scale of their own": wrong. See "Model scale". The note
  that `isHamResource` is far too large goes with it.
- "For characters, 4096 units is one metre" is true but not special to characters.
- `tools/gltf.py` says 4096 units is a metre for everything; other models export at the wrong size.
- `DATASETS` is a bundle format, not behaviour data. Behaviour is `EDITHTRE`.
- The `SAMPLES` "first u32 mixed" isn't a version: entries are headerless, and the first bytes are
  sound data.
- The open items on the hash, `INDEX.IND`, submodel transforms and clothing pairing are closed.

## What could still be wrong

- All loader reading is from the Wii build. Where the bytes test it, it's fine. Effects at run time
  (rotation order, the grid meanings, the scale in the draw path) rest on the Wii code alone.
- 2,609 mid-lot objects have a rig and skip `PreScale` in `SetupModel`. I expect the rigged path to
  apply the model scale elsewhere, but didn't find it. Check before trusting their size.
- Bits 24-30 of the sample word are read as a rate index by shape, not by ear.

Open:
- In `Arry`: floor values, flags 0x02 and 0x40, the lone wall edge, the trees that change flag 0x10, and whether the mesh bit is vertex flag 0x10.
- Object height, the sense of direction, and how shared models pick a variant.
- How `missingshader` terrain is drawn.
- How palm leaves attach, and which objects block a Sim.
- Scale for rigged mid-lot objects.
- ObjDefinition, ResFile and most `world` columns. `ERLevel`'s submodel list groups.
- The bodies of most BHAV primitives and some expression scopes; see [bhav.md](bhav.md).
- Kind 0 of `DEFAULT.NGH` holds `TATT`, `SIMI` and `HOUS` chunks. Kind 1 holds `NGBH`, `NBRS` and `FAMI`: `bhav.md`'s Neighbourhood section has them.
- Which of languages 0 and 1 is the US text. Sample word bit 31.
