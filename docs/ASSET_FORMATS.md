# Asset formats

What's known about the game's data files, worked out from the US disc. All values are
little-endian. `tools/arc.py` reads the archives and `tools/texture.py` decodes textures.
Both read your own disc image in place. Anything they extract is game data and stays out
of the repo.

Each section says how far it has been checked. "Open" items are unknown, not guesses.

More detail is in `docs/formats/`: `audio.md`, `media.md` (movies, fonts and the Apt UI), `morphs.md`
(body and face shapes) and `world.md` (the name hash, lots, data tables and behaviour files). The
tools for those are `tools/audio.py`, `movie.py`, `font.py`, `morph.py`, `placed.py` and `gamedata.py`.

## Disc layout

The disc is plain ISO9660. Game data is 23 `.ARC` archives in `DATA/`, plus `INDEX.IND` and
`INDEX1.IND`. The disc image is about 4.6 GB but the files hold about 1.1 GB. They sit
between sector 1,150,000 and about 3.5 GB in, with padding on both sides.

## ARC archives

Checked on all 23 archives, 37,911 entries: every entry fits inside its file, none overlap,
names are unique within an archive and every table is sorted by hash.

Header, 16 bytes:

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | Offset of the table. 0 means an empty archive (`EFFECTSS`). |
| 0x04 | u32 | Version, always 0x10000 |
| 0x08 | u64 | Windows FILETIME; August 2007 on this disc |

Table: a `u32` entry count, then entries one after another:

| Type | Meaning |
|---|---|
| u32 | Hash of the name; the table is sorted on this |
| u32 | Offset of the data from the start of the archive |
| u32 | Size of the data |
| char[] | Name, NUL-terminated |
| u64 | Windows FILETIME |

Most archives keep the table at the end. `MOVIES` and `AUDIOSTR` keep it at 0x10 and start
their data at 0x800. `MOVIES`, `AUDIOSTR` and `SAMPLES` align every entry to 2048 bytes, one
disc sector, and `DATASETS` to 4096, which suits reading straight from the disc.

Open:
- What `INDEX.IND` adds. It starts with a count of 42 and a list of offsets ending at the
  file size. The entries seem to pair an archive's name with a sorted list of hashes.

## Asset files

Each archive holds one kind of asset, and its files start with the same `u32`. That looks
like a version number for the type. `ERModel::GetTypeVersion` and `GetTypeKey` in the Wii
symbols fit that reading, but this hasn't been confirmed in the code.

| Archive | Files | First u32 | Contents |
|---|---:|---:|---|
| `TEXTURES` | 13,249 | 9 | Textures, tag `TXFL`, see below |
| `SAMPLES` | 7,656 | none | Audio samples with no header, see `formats/audio.md` |
| `SHADERS` | 6,652 | 0x19 | Shaders, tag `SHDR`, see below |
| `ANIMATIO` | 3,104 | 0x16 | Animations, tag `ANIM`, see below |
| `MODELS` | 2,404 | 0x45 | Models, tag `MODL`, see below |
| `SOUNDEVE` | 2,279 | 1 | Sound events, see `formats/audio.md` |
| `DATASETS` | 693 | 0x0a | Preload bundles of other files, see `formats/world.md` |
| `EDITHTRE` | 694 | 3 | Object behaviour, see `formats/world.md` |
| `MOVIES` | 17 | `00 00 01 BA` | Starts with an MPEG program stream pack header |

The remaining archives are small. `FONTS` files use the same opening as textures with the
tag `FONT`. The full list comes from `tools/arc.py survey`.

## Textures (TXFL)

Checked on all 13,249 textures: every one parses, and pixels plus palette fill each file
exactly. Pixel order, palette order and orientation were checked by eye on samples of each
kind, using logos and text, which only look right one way up.

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | Version, always 9 |
| 0x04 | char[4] | `LFXT`, which is `TXFL` as a little-endian integer |
| 0x08 | i32 | Always -1 |
| 0x0C | u32 | Name size, including its NUL |
| 0x10 | char[] | Name |

The rest counts from `n`, the end of the name:

| Offset | Type | Meaning |
|---|---|---|
| n+0x00 | u32 | Bytes left in the file after this field |
| n+0x04 | u32 × 2 | Always 0 |
| n+0x0C | u32 | Flags; open |
| n+0x10 | u32 | Always 0 |
| n+0x14 | u16 | Width |
| n+0x16 | u16 | Height |
| n+0x18 | u16 | Palette entries: 256, 16, or 0 for direct colour |
| n+0x1A | u16 | 0, or 2 to 6 growing with image size; open, possibly a mipmap count |
| n+0x1C | u8 | 0 for paletted, 1 for direct colour |
| n+0x1D | u8 | 2 for paletted, 0 for direct colour; open |
| n+0x1E | u8 | Bits per pixel: 4, 8, 16 or 32 |
| n+0x1F | u8 | Always 0x20; possibly the bits per palette entry |
| n+0x20 | u32 | Always 0 |
| n+0x24 | | Pixels, then the palette |

| Kind | Bits | Palette | Textures |
|---|---:|---:|---:|
| Paletted | 8 | 256 | 12,776 |
| Paletted | 4 | 16 | 421 |
| Direct | 32 | none | 49 |
| Direct | 16 | none | 3 |

Things to know when decoding:

- **Rows are stored bottom first.** Flip them for any top-down image format.
- **Pixels aren't swizzled.** They're in plain row order, unlike the GS's own memory layout.
- **Palettes are in linear order.** The usual PS2 CSM1 shuffle isn't applied.
- **Alpha uses the GS range.** 0x80 is opaque, and no texture goes above it. Double it for
  the usual 0-255 range.
- **4-bit pixels** hold the left pixel in the low nibble.
- **16-bit pixels** are 5 bits each of red, green and blue from the low bits up, then one
  alpha bit.
- **Only one image level is stored**, whatever n+0x1A holds.

A smoothness score can't judge palette order. About 200 low-contrast textures score better
with the wrong order, because the wrong palette makes them darker, not because it's right.
Check by eye.

Open: the meaning of the flags (seen with bits 0x1, 0x2, 0x20, 0x40, 0x400 and 0x800).
`ERTexture::Load` at 0x456228 is the place to read them from.

## Models (MODL)

Checked on all 2,404 models, 9,827 meshes, 32,515 batches and 690,896 triangles. Every batch
parses, belongs to a mesh record and matches that record's vertex type, and no record is
empty. 28 models hold no geometry, such as `null` and the particle anchors. Shapes and
textures were checked by eye on about 20 models.

The opening matches textures: version 69, tag `LDOM` (`MODL`), -1, name size, name. `n` is
the end of the name, and n+0x00 is the bytes left after that field.

Next come arrays that are usually empty. The Wii loader, `ERModel::LoadFromFile`, reads
`BSplineVolume`, `ENDummy`, `ENCamera` and `SimsLightInfo` arrays and then the `ESubModel`
array. The PS2 order hasn't been checked in code. With nothing in them, the submodel count
is at n+0x52. A light adds 28 bytes before it, as in `torch`.

`tools/model.py` doesn't walk those arrays. It finds what it needs by its contents:

- **Mesh records** are a `u32` type then a `u32` shader hash. Only hashes found in `SHADERS`
  count, so chance matches aren't a practical problem.
- **Batches** start with the bytes `01 01 00 01 00 80 01 60`.
- **Each batch belongs to the nearest mesh record before it.**

The type is a mask of what the vertices hold, and it matches the batches in every mesh:

| Type | Position | UV | Normal | Slot 4 |
|---|---|---|---|---|
| 0x16 | yes | yes | no | yes |
| 0x1A | yes | yes | yes | no |
| 0x1E | yes | yes | yes | yes |

### Batches

A batch is VIF code, the command stream that DMA feeds to Vector Unit 1, stored ready to send.
A batch holds at most 50 vertices.

| Command | Bytes | Meaning |
|---|---|---|
| STCYCL 1,1 | `01 01 00 01` | Write vectors one after another |
| UNPACK S-32 to slot 0 | `00 80 01 60` | One word: the vertex count |
| STCYCL 5,1 | `05 01 00 01` | Write every fifth vector, interleaving the slots |
| UNPACK V4-16 to slot 1 | `01 80 nn 6d` | Positions: x, y, z, flags as `s16` |
| UNPACK V4-8 to slot 2 | `02 80 nn 6e` | Normals: x, y, z as `s8` over 127, then a baked brightness over 127; optional |
| UNPACK V2-16 to slot 3 | `03 80 nn 65` | UVs: u, v as `s16` over 4096 |
| UNPACK V4-8 to slot 4 | `04 80 nn 6e` | Vertex colour on an unlit mesh; its first byte tags the body lattice. Optional |
| UNPACK V4-8 to slot 5 | `05 c0 nn 6e` | Bone weights, one byte per palette slot; optional |
| MSCAL | `00 00 00 14` | Run the VU1 program |

Things to know when decoding:

- **Vertices form triangle strips.** The triangle ending at a vertex is skipped when bit 15
  of that vertex's flags is set. That's how one batch holds several strips.
- **Strips also repeat vertices**, which makes collapsed triangles to drop.
- **Skinned models are stored in bind pose**, so `mm_monkey` comes out in a T-pose without a
  skeleton.
- **V counts rows as stored**, bottom row first. With the top-down PNGs from
  `tools/texture.py`, that's the usual OBJ convention, so V needs no change.
- **Every model has a scale.** Metres are the stored integer times a float kept just before the
  submodel count. It's always a power of two, chosen so the model fills the 16-bit range:
  characters use 1/4096, walls 1/8192, and `isHamResource` 1/131072. A door comes out 1 by 3 metres.
  The offset of that float varies, so read it by walking the header, as `gamedata.py` does.

- **Purple surfaces can be correct.** `lambert1`, Maya's default material name, points at a
  texture named `missingshader`. `lot_23` has 1,549 such triangles, all without normals.
  Whether they're visible in the game is open.

UV orientation is easy to get wrong. Billboards map a whole texture onto a quad, so flipping V
changes what they show but tells you nothing about which way is right, and counting how many
triangles land on opaque pixels gives a near tie. Use models whose UVs trace a shape inside the
texture. `tree_pine` lands on opaque pixels 46 times out of 46 as stored, and once when flipped.

Open:
- The rest of a position's flags, including bit 13.
- The plain copy of each group's vertices stored ahead of its batches.
- The offset and size pairs in a group's tail, and the `02` byte seen before some palettes.
- Any transform on a submodel.
- How rigged scenery gets its size: 2,609 lot objects with a rig skip the model scale.

### Groups, palettes and weights

Checked on all 16,467 groups and 343,914 weighted vertices.

A group is a run of batches with nothing between them. The `u32` after a mesh record's shader
hash is the mesh's group count. Each group has:

1. A bone palette of up to 4 entries, each a flag of 1, a `u16` bone index and a `u8` slot,
   ended by a 0 byte. Slots always run 0, 1, 2, 3. Bone indices point into the skeleton.
2. A `u32` count of unique vertices, then a plain copy of the vertex data.
3. The batches.
4. A tail: on all but static groups `18 00 00 00 00 00 00 00`; an offset and size per batch;
   then the unique vertices, the total vertices and the batch count.

A group with one bone and no slot 5 is bound rigidly to that bone. Otherwise each vertex has one
weight per palette slot. With weights but no list, slot k simply means bone k: on
`doorStylishDoor1`, slot 1 is `door_frame` and slot 2 is `door_rot`. All 385 such groups with a
same-named rig use real bones. A group with neither is static.

The count before the vertices is a small number, so it can turn up earlier by chance. Search only
after the mesh's own record, and prefer a place with a list in front of it. Weights always add up to 255, with at most 3 in use. The low byte of a
position's flags is 4 times the slot of the largest weight, on every vertex.

## Skeletons (CHRC)

Checked on all 338 files and 4,421 bones: every file ends exactly where the format says, parents
and children agree, every parent comes before its children, and every matrix times its inverse
gives the identity.

Most files only name an object's parts. Eight are full rigs: `female_adult`, `Male_adult`,
`mchimp`, `hboar`, `wuPuppy`, `kuKitten`, `chicken` and `isFishSource`. The two adults use the
3ds Max Biped names (`Bip01 L Thigh`) and have the same bone positions.

The opening is version 9, tag `CRHC` (`CHRC`), -1, name size, name. Then, from
`ERCharacter::Load` in the Wii code:

| Type | Meaning |
|---|---|
| u32 | Bytes left in the file after this field |
| u32 | Bone count |
| | The bones, one after another |
| f32 × 3 | Bounding sphere centre, in all likelihood |
| f32 × 2 | Bounding sphere radius, stored twice in the rigs checked |

Each bone:

| Type | Meaning |
|---|---|
| i32 | Parent, or -1 |
| u32, i32[] | Child count and children |
| f32 × 3 | Position in the rest pose: world space, in metres |
| f32 × 4 | Rotation in the rest pose: world space, a unit quaternion in x, y, z, w order |
| u8 | 1 when the next two matrices are the identity |
| f32 × 16 | The same rotation as a matrix for row vectors, so its rows are the bone's axes |
| f32 × 16 | Its inverse |
| char[] | Name, NUL-terminated |

`tools/skeleton.py` ties a model to its rig by its same-named file, or by its prefix: `af_` to
`female_adult`, `am_` to `Male_adult`. That covers 1,157 models with no bone index out of range.
Another 212 models use bones but have no known rig, among them `mm_monkey`, `dm_bulldog` and
`fm_cat`. Their rigs aren't in `CHARACTE.ARC`. Don't pair them by name: `mm_monkey` looks like it
fits `mchimp`, and even bends plausibly on it, but its bone indices mean something else. Its
`l_wrist` vertices sit over a metre from that bone. The real `mchimp` models fit to about 0.1 m.

To test a pairing, bend a bone and measure how far mesh edges stretch, then repeat with every
palette shifted by one bone. The worst 1% of edges stretch about ten times further when shifted
on `mchimp` and `af_lb_pcotton_ln`. That control doesn't separate `wuPuppy`, whose neighbouring
bones share a position; there the evidence is the fit of vertices to bones, about 0.04 m.

Open:
- The rigs for `mm_monkey`, `dm_bulldog` and `fm_cat`, and for hand props.
- Whether male and female rigs differ in anything but name.

The rotation was checked two ways. On all 4,421 bones, the quaternion's matrix equals the stored
matrix transposed. And in the rigs with mostly single-child bones, one local axis of that
rotation runs along the bone to its child: 16 of 18 on the adults, every qualifying bone on the
puppy and kitten. The transposed reading does clearly worse.

Rigs don't agree on which way is forward. Adults face -Y, and the animals face +Y.

## Rigged glTF

`tools/gltf.py` writes a `.glb` per model, with textures inside. Checked by exporting all 2,376
models with geometry, 1,157 of them rigged, and reading the bytes back with a separate validator:
sizes, bounds, the node tree, weights, joint ranges, and each joint's global transform times its
inverse bind matrix, which comes to the identity within 1.4e-07. Five rigged models then loaded in
Godot 4.7.2 through its own importer: bone counts match, and the 290 bone positions it reported
match the source within 8 micrometres. Bones posed in Godot move the mesh as expected.

Conventions:

- **Axes:** `(x, y, z)` becomes `(x, z, -y)`, a rotation from the game's Z-up to glTF's Y-up. Adults
  end up facing +Z, as glTF expects, and animals face -Z. The data isn't turned to hide that.
- **Units:** metres, using each model's own scale.
- **UVs:** V becomes `1 - v`, because glTF counts V from the top of the image.
- **Joints:** every bone in the rig, in file order, so joint indices equal bone indices. Local
  transforms come from the rest pose, as the parent's inverse times the bone.
- **Static parts** of a rigged model, with no palette or weights, stay with joint 0.
- **Animations:** `tools/gltf.py animate` samples every whole frame into linear glTF channels, at
  the clip's own stored rate (below). An animation is only added if its node count fits the rig.
- **Models with no known rig** export without a skeleton, so `mm_monkey` comes out static.

Open:
- Most clothing, skin and pets point at a texture named `white`, so `tools/gltf.py` exports them
  white. `tools/sim.py` builds the real textures for humans, as described under "Assembling a Sim".
- Only the first texture layer is exported, and blending isn't reproduced.
- Morphs aren't exported.

## Assembling a Sim

`tools/sim.py` builds a whole Sim into one `.glb`. Checked across all human models: 1,196 surfaces
find clothing, 10 are skin alone, 12 use their shader's own texture and 1 stays white. All 60 skin
maps exist and all 24 face features fit the face. Four Sims were built and rendered in Godot,
covering both rigs, all the main parts, and one with an animation.

### Checked against the running game

One female Sim in the Ranger outfit was read from PCSX2's memory, and her four composite maps were
compared pixel by pixel with the same maps built by these rules from her parts:

| Map area | Mean difference, of 255 | Within 8 |
|---|---:|---:|
| Torso | 0.57 | 100.0% |
| Legs | 0.53 | 100.0% |
| Head | 0.62 | 99.7% |
| Shoes | 0.64 | 99.7% |
| Sleeves | 0.66 | 100.0% |
| Hair, visible pixels | 0.33 | 100.0% |

Painting the wrong garment instead gives 58 for the torso and 87 for the legs, so the test can fail.
The small differences fit the game reducing each map to 256 colours. This covers one Sim, one
outfit and one skin tone at `_game` resolution. Hats, accessories and male Sims weren't compared.

To find a Sim in memory, search for `CasSimDescription`'s vtable, 0x004ACBF0, at offset 0x330 of
the object. `pkg/cas_layouts.csv` has the fields. Each composite map's texture object holds a
TXFL-style header, pixel and palette pointers. The maps are 128 by 128 at 8 bits, unswizzled, with
the palette in CSM1 order, unlike textures on the disc. Their names come from
`GetCompositeTextureName`, such as `af_cur_AMBER_RASTELLA`.

### Parts

Model names are `{af|am}_{part}_{name}`.

| Part | Meaning |
|---|---|
| `ti`, `lb`, `slti`, `sh` | Torso, lower body, sleeves, shoes. `nude` versions exist of each |
| `ft` | The face: `af_ft_base_face` |
| `chin`, `eyes`, `lips`, `nose` | Offsets for the face, not meshes of their own |
| `hh`, `hb`, `ha1`, `ha2`, `ha3` | Hair, in the game's table order. `hh` is plain, `hb` is the short form seen under a hat, and `ha1` to `ha3` are the Accessory Sets, whole hairstyles with their decoration: `af_ha2_wavylong_ln` is a bun with a flower. `SimBodyPartHair::GetModelCode` picks `hb` on character flag 2, which fits wearing a hat. `am_ha1_generic_fl` points at a female shader, for reasons still open |
| `ht`, `bt`, `bc`, `jw`, `gl`, `ff` | Hats, belts, bracelets, jewellery, glasses, facial hair |

Face features have the same batches and vertices as the face, in the same order, holding offsets
that are mostly zero. A default Sim uses none of them: `morphTargetValues` choose them, and 1 means
no change. `tools/morph.py` applies them, and the body lattices, as the code does. The lattices
match the game's memory exactly on one Sim; a morphed face hasn't been compared. See
`formats/morphs.md`.

### Textures

Most human shaders point at `white`, because the real texture doesn't exist until a Sim does. The
game paints four 256-pixel maps per Sim, or 128-pixel ones from the `_game` textures, using
`SkinCompositor` and `CasSimDescription::GetClothingTextureInfo`.

`GetLayoutMapNumber` and `GetLayoutMapRect` in each `SimBodyPart` class give each part's place.
Rectangles are in 128ths, with rows counted from the bottom, as the textures are stored:

| Part | Map | Rectangle |
|---|---|---|
| Torso | 1 (`ur`) | 0, 0 to 128, 128 |
| Lower body | 3 (`lr`) | 0, 0 to 128, 128 |
| Sleeves | 0 (`ul`) | 0, 64 to 128, 128 |
| Head | 0 (`ul`) | 0, 0 to 64, 64 |
| Shoes | 0 (`ul`) | 64, 0 to 128, 64 |
| Hair | 2 (`ll`) | 64, 0 to 128, 64 |
| Hat | 2 (`ll`) | 0, 64 to 64, 128 |
| Belt, bracelet, glasses, necklace | 2 (`ll`) | 32-unit tiles |

A mesh's UVs run 0 to 1 inside its part's rectangle. So a surface's real texture is that crop of
the skin map with the part's clothing painted over it by alpha, and transparent clothing shows the
skin beneath. Map 2 has no skin, so those parts use their texture alone. `tools/sim.py` builds one
texture per part this way instead of whole maps, which gives the same picture.

- **Skin** is `{af|am}_{ul|ur|lr}_{tone}_normal_{1|2}`. In the game's order, tones are `caucasian`,
  `asian`, `brown`, `darkbrown` and `pale`, numbered 1 to 5 on screen. Underwear is part of the skin. Ignore skin alpha: `am_ur_caucasian_normal_1`
  has alpha 0 on every pixel, while the other maps checked have 255.
- **Clothing** is `{model}_{colour}_{variant}`, with a letter for the colour: `n`, `r`, `b`, `y`,
  `w`, `o`, `l` or `p`.
- **Hair** always uses the `hh` family, from `GetModelCodeForTexture`, in colours `b`, `r`, `w` and
  `y`.
- **Hats, belts and jewellery** often use the shader's name. Some accessories name colours in
  words, such as `af_bc_widewoodcuff_tigereye_1`.
- **Surfaces with a `_base_face` shader** in hair models are scalp, and read the head's skin.
- `_game` textures are half size, `_icon` is for menus, and `_rfl` textures are reflection masks.

Open:
- `SkinCompositor::BlendTexture` hasn't been fully translated. Painting by alpha matches the game
  on the maps compared, but the `layered` flag isn't handled.
- Reflection masks, tattoos and degraded clothing states.
- `tools/sim.py` still takes parts by hand. The `CreateASim` table holds the real rules, and
  `tools/gamedata.py` can dump it: `fl` torsos pair with `ln` sleeves, and `mr` with `sr`.
- Whether hats match the game. `af_ht_captain_norm` over `af_hb_ponytail_sh` looks ragged here.
- The `ll` map's other tiles.
- Pets also use `white`, and their textures aren't decoded.

## Animations (ANIM)

Checked on all 3,104 files, 527,837 frames and 3,047,195 keys. Every file ends where its tables
say, every stream's frames add up to the animation's length, and all 2,487,068 full-rotation keys
are unit length. Five walks were then checked on their rigs, and four rigs played in Godot.

The format comes from the Wii code: `ERAnim::LoadFromFile`, `EAnimController::Animate`,
`ComputeUniformScaleMatrices`, and `ERotDecomp`, `ERotDecomp1Dof` and `EVec3Decomp`. The opening is
version 22, tag `MINA` (`ANIM`), -1, name size, name. Then:

| Type | Meaning |
|---|---|
| u32 | Bytes left in the file after this field |
| u32 | Frames |
| f32 x 6 | Open; usually all 1.0 |
| u32, 8 x i32 each | Node count, then one entry per bone |
| u32, i32[] | Morph channels |
| u32 | Open |
| u32, f32[] | The float table, for constants |
| u32, then words | The stream bits: a bit count, then that many bits in 32-bit words |
| | 3 x u32, u8, 4 x u32, 2 x u8; open |
| u32, then notes | Each is 2 x u32 and a NUL-terminated string |
| u32, then events | 80 bytes each |

Node entries, in order: rotation, a second vector (probably scale), position, visibility, then
four more that `AnimateSpecial` handles. For each channel, 0 means nothing, a positive value is an
index into the float table, and a negative value is the bit position of a stream, negated.

### Streams

Bits run from the low bit of each byte upward. For a rotation, the bit at the stream's position
picks the kind: 1 for a single axis, 0 for a full rotation. The stream starts at the next bit.
Vector streams start at their position. A width of 0 means raw 32-bit floats.

| Stream | Header | Each key |
|---|---|---|
| Full rotation | keys:20, t:5, b:5, q:5 | delta:t, bias:b, curved:1, x y z:q signed, sign of w:1 |
| Single axis | axis f32 x 3, keys:20, t:5, b:5, a:5, range f32, base f32 | delta:t, bias:b, angle:a |
| Vector | keys:20, t:5, b:5, v:5, then scale and minimum f32 for each axis | delta:t, bias:b, curved:1, x y z:v |

- Key 0 is at frame 0. Each later key is its delta plus 1 after the one before. Key 0's delta also
  counts toward the stream's total.
- A signed value is scaled by `1 / (2^(bits-1) - 1)`. An unsigned one by `1 / (2^bits - 1)`.
- `w` is rebuilt from unit length, and always `w`: this isn't the smallest-three method.
- Angles are `base + value x range / (2^a - 1)`.

### Curves

Each key has a bias. Blending quaternions always means a straight blend, then normalising.

- A full rotation is a straight blend unless either end's curved bit is set. Then it's a cubic
  Bezier through four quaternions, built by repeated blending. The inner points come from
  `ERotDecomp::GetQ`.
- A vector is the same choice between straight and cubic Hermite.
- A single axis is always cubic Hermite on the angle, kept within half a turn of the start. Then
  it becomes a quaternion about the axis.
- Hermite tangents are `share x ((1 + bias) x before + (1 - bias) x after)`. `share` is the time on
  that side over the time on both. End keys use `1.5 x slope - 0.5 x neighbour's tangent`, or
  just the slope with only two keys.

### Applying a pose

Channels are changes from the rest pose, in world axes, not local transforms. For each bone, in
order, with `b` its rest position, `q` its rotation and `d` its position channel:

    point -> rotate(q, point - b) + b + d, then the parent's result

The result maps rest-pose points straight to posed ones, so the game needs no inverse bind
matrix. `tools/gltf.py` applies that to each bone's rest pose, then takes the parent's inverse.

This was tested on foot height, which a wrong rule can't pass by luck. In
`a_motive_bladder_low_walk_01`, the feet stay within 8 mm of the ground. Reversing the rotations
gives -80 to +109 mm, and skipping the pivot gives -132 mm. `a_walk_ghost` really does hover,
5 to 13 cm up.

Some large jumps between frames are real: `a2o_sofa_make_mover_turn_left_reject` has 180 degree
snaps in one frame. No neighbouring keys have opposite signs, so `tools/gltf.py` needs no special
handling beyond keeping its own samples on one side.

**Proven, each clip has its own frame rate.** The first float after the stream bits is the clip's
frames a second. `ERAnim::LoadFromFile` reads it into `+0x70`, then `+0x74` and `+0x78`, and
`SAnimator2::getAnimDuration` gives a clip's length as (N - 1) / that rate. 2,109 clips are at 30,
971 at 60 (the adult walk, run and idles among them), and 15 at odd rates (0.01 to 15). So
`af_walk_base`, 68 frames at 60, lasts 1.117 s, not 2.233 s.

**Corrected.** This page said every clip runs at 30 frames a second. The 380 floats seen rising at
30 a second in a running game were 30-a-second clips; the export had timed every clip that way.

### Rigs by prefix

| Prefix | Nodes | Rig |
|---|---:|---|
| `a`, `a2o`, `a2a`, `am`, `af` | 58 | The adults |
| `f` | 68 | `wuPuppy` and `kuKitten` |
| `h` | 49 | `hboar` |
| `s` | 57 | `mchimp` |
| `d`, `d2a`, `l2a` | 74 | Not in `CHARACTE.ARC` |
| `m` | 39 | Not in `CHARACTE.ARC`; `m_walk` is an example |
| `o` and others | 1 to 13 and more | Objects |

Open:
- The scale channel, which isn't applied, and the visibility, morph and special channels.
- The six floats in the header (all 1.0 on the clips checked), and the settings after the rate.
- What notes and events do, and how the game blends animations together.
- `EVisInfoDecomp`.

## Shaders (SHDR)

Checked on all 6,652 shaders. 6,644 name a texture that exists, and 8 name none.

The opening is version 0x19, tag `RDHS` (`SHDR`), a word that's 0 for full shaders and 1 for
the short form, name size, name. `n` is the end of the name, and n+0x00 is the bytes left after
that field. A full shader's first byte after that is its layer count.

The texture hash is at n+0x3F for the first layer and n+0x6F for the second, or at n+0x10 or
n+0x0F in the short form. `tools/model.py` tries those in that order and exports only the first
texture it finds.

Open: everything else, including the blend settings and how layers combine. Some shaders list
several textures in a row, such as `rainstreaks`, which may be animation frames.
