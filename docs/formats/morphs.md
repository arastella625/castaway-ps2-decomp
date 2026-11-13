## Body and face shapes

`tools/morph.py` reshapes parsed meshes the way the game does. A Sim's shape comes from two systems,
both driven by `CasSimDescription`: lattices (`latticeMorphValues`, float[4] at 0xD0) bend the body,
and face targets (`morphTargetValues`, float[4] at 0xE0) add offsets to the face.

How far each claim was checked:

| Claim | Evidence |
|---|---|
| Lattice file format | All 2,404 models: 1,195 lattices in 769 models parse, and in all 769 the three following counts are 0 and the first mesh record sits exactly 33 bytes on, as in models with no lattices |
| Grid and box maths | 67,308 vertices inside their box come back within 1.5/4096 m with nothing morphed (median about 0.1) |
| Whole body morph | Bit-exact against RAM: torso 468 of 468 vertices, lower body 432 of 432, sleeves 603 of 603, for the test save's player Sim at [3, 0.5, 0.5, 0.5] |
| Unmoved parts | Bit-exact against RAM: face 803 of 803, `af_ha2_wavylong_ln` 409 of 409, shoes 274 of 274 |
| Keyframe weights | PS2 disassembly matches Wii. RAM proves values 0.5 (keyframe 1) and head 3 (none). Above 1.0 and other head values are code and renders only |
| Face targets | PS2 disassembly matches Wii; the 13-model list read from RAM; all 24 targets fit the face. **No face with a non-default value has been compared with the game** |

The PS2 functions, found by following calls, since `pkg/castaway_ps2_names.csv` lacks them:

| PS2 address | Function |
|---|---|
| 0x0043B5E0 | `BSplineVolume::ParameterizeVert` |
| 0x0043B978 | `BSplineVolume::BlendVolumes` |
| 0x0043BE18 | `BSplineVolume::GetPosition` |
| 0x003FAFB0 | `ESubModelShader::AllocateStripsMorph` |
| 0x003FB608 | `ESubModelShader::ApplyMorphTargetsToStripsMorph` |
| 0x003FBBD8 | `ESubModelShader::CreateLatticeList` |
| 0x003FBE50 | `ESubModelShader::AllocateLatticeParameterVerts` |
| 0x003FC040 | `ESubModelShader::CreateLatticeParameterVerts` |
| 0x003FC3C8 | `ESubModelShader::ApplyLatticesToStripsMorph` |
| 0x004544A0 | `ERModel::ApplyLatticesToStripsMorph` |
| 0x00146C88 | `MorphUtilities::ApplyAllLatticeMorphs` |
| 0x00146D58 | `MorphUtilities::GetLatticeIndex` |
| 0x001341D8 | `MorphUtilities::ApplyLattice` |
| 0x00134400 | `MorphUtilities::ApplySimHeadLatticeToBodyPart` |
| 0x00134700 | `MorphUtilities::ApplySimBodyLatticeToBodyPart` |
| 0x001349D8 | `MorphUtilities::GetSimFacialMorphTargetIndicesAndWeights` |
| 0x00134AE8 | `MorphUtilities::GetSimBodyLatticeKeyframeIndicesAndWeights` |
| 0x00134BD0 | `MorphUtilities::GetSimHeadLatticeKeyframeIndicesAndWeights` |
| 0x0013DEB0 | `SimModelPartHead::GetMorphTargetResID` |
| 0x0013DFF0 | `SimModelPartHead::ApplyMorphTargets` |
| 0x0013E1E0 | `SimModelPartHead::ApplyMorphTargetsToRegion` |
| 0x00133C98, 0x001339D8 | `LatticeParam::Instance`, `MorphTargetParam::Instance` |

0x00133C98 and 0x001339D8 are named `CasUtils::GetBodyPartTypeInfoTable` in the names CSV. That's
wrong by `config/symbols.txt`, which has them as the two `Instance` functions.

### Lattices in the file

The lattice array starts at n+0x3D, where n is the end of the name: a `u32` count, then each lattice:

| Field | Size | Meaning |
|---|---|---|
| version | u32 | Always 0 |
| name | 128 bytes | `head_10`, `r_Arm_20`, `l_arm_30`, `torso_40`, `legs_50`, zero padded |
| id | u32 | 10, 20, 30, 40 or 50: the number in the name |
| x, y, z | 3 x u32 | Control points on each axis |
| frames | u32 | Frame 0 is the rest shape, the others are keyframes |
| points | frames x x x y x z x 3 x f32 | Metres. Frame outermost, then x, y, z. Z runs fastest |

`head_10` is 7 x 6 x 7 with 8 frames (a few have 3, 4 or 5), `torso_40` 6 x 4 x 7 (seen on `af_`) or
6 x 4 x 6 (seen on `am_`), `legs_50` 6 x 4 x 8, and the arms 7 x 4 x 4. All other lattices have 3 frames.
Only 4 models have a short head lattice: `am_ti_mechanic_fl`, `am_ti_mechanic_mr`, `am_jw_beadnecklace`
and `am_ff_goattribal_norm`.

On load, each value becomes `(int)(value * 4096)` in an `s16`, and frames 1 onward become offsets
from frame 0. A lattice with one frame or fewer stores no points; none exist on the disc.

After the lattices come the dummy, camera and light arrays, a byte, a `f32`, then the submodel count.
The `f32` is a power of two: 1/4096 on 1,148 models, including every character, and 1/8192 to 2^-18
or 1/256 on others. This is likely each model's position scale, which would answer the open item on
true sizes. That isn't tested.

### Slot 4

Slot 4 is explained. It's a vertex colour, and its first byte is a lattice tag:
`first byte * 2 == lattice id`. So `05` is head, `0a` right arm, `0f` left arm, `14` torso, `19` legs.
`CreateLatticeList` reads exactly that. Read as bytes, `14 00 00 80` is torso. The fourth byte is 0x80.

The tag has to match exactly. `00` and `80` (as in `80808080`) mean no lattice. So do in-between values
such as `06` to `09` and `15` to `18`, which look like colours blended along seams. RAM confirms it:
all 27 such sleeve vertices stay where they are. 42,814 unique vertices in lattice models have no
lattice. A vertex takes the first lattice in the model's list with its id, so the face's five
`head_10` lattices act as one.

### The maths

For each tagged vertex, once, from its rest position (`ParameterizeVert`):

1. Take the box around frame 0's inner control points, leaving out the outer layer on each axis.
2. `u = clamp((p - low) / (high - low), 0, 1) * (n - 3)` on each axis.
3. `cell = clamp((int)u, 0, n - 4)`, `t = u - cell`.

To morph (`BlendVolumes`, `GetPosition`, `ApplyLatticesToStripsMorph`):

1. For each keyframe with a non-zero weight, in order: `point = (int)(point + offset * weight)`.
2. Evaluate the 4 x 4 x 4 points from `cell`. Each axis uses Hermite curves with tangents `p1 - p0` and
   `p3 - p2`, so the weights are `-h10, h00 + h10, h01 - h11, h11`. In the object these are the
   constants 0, 1, -1, 1 at 0xCC, read from live RAM. Catmull-Rom gives the same answer on an unmorphed
   grid, but fails the RAM test (34 of 468).
3. `position += (int)((result - rest) * 4096)`.

Normals aren't changed. On the six models compared, 97% of normals match the disc byte for byte, even
on morphed vertices.

A vertex outside the box is clamped to its edge, so it moves even with nothing morphed. That's 1,554
of 68,862 tagged vertices. It's the real behaviour: `af_slti_ranger_ln` has 16 and they match RAM.
It only happens when the model is dirty. `SetLatticeWeight` marks a model dirty when a weight changes
from 0, so a part with all weights 0 stays exactly as on disc. The head is always marked.
`apply_lattices` follows this (likely; a fully neutral Sim's RAM wasn't available to confirm it).

### Values to keyframes

From `Sim::BodyPartTypeInfoTableTSC6`, read in RAM at 0x91F490. `LatticeMorphTSC6` is 0x1C bytes:

| Region | Lattice | Range | Normal | By body type 0, 1, 2 |
|---|---|---|---|---|
| 0 | head | 0 to 7 | 3 | Left alone |
| 1 | torso | 0 to 2 | 1 | 1, 0.5, 0 |
| 2 | arms | 0 to 2 | 1 | 1, 0.5, 0 |
| 3 | legs | 0 to 2 | 1 | 1, 0.5, 0 |

- **Body**: below 1, keyframe 1 (thinner) at weight `1 - v`. From 1 to 2, keyframe 0 (heavier) at
  weight `v - 1`. So a body type never reaches a heavier shape; only the values above 1 do.
- **Head**: the keyframes are shapes 0 to 7 with 3 skipped, since 3 is the rest shape. Whole values
  pick one keyframe and fractions blend two neighbours. Shape 3 has no keyframe, so 2.5 is half of
  shape 2 and 3.5 half of shape 4.

Lattices are driven by place in the part's list, not by name (`GetLatticeIndex`):

| Part | Head drives | Torso region drives | Arm region | Leg region |
|---|---|---|---|---|
| `ft`, `ff`, `hh`/`hb`/`ha*`, `ti`, `jw` | Place 0 | Place 1 | | |
| `gl`, `ht` | Place 0 | | | |
| `lb` | | Place 0 | | Place 1 |
| `bt` | | Place 0 | | |
| `slti`, `bc` | | | Places 0 and 1 | |
| `sh` | | | | |

The head region needs at least 7 keyframes in place 0, or it's skipped. That has two effects worth
knowing, and `check` counts 33 such lattices:

- 28 `jw` and 2 `ht` models list only `torso_40`, in a place nothing drives, so their tagged vertices
  never move. A necklace of that kind doesn't follow the body. The game does this too, by the code.
- The 4 models with short head lattices never get a head shape.

### Face targets

`modelCodeIndex` on the head isn't what picks them: `Validate` forces it to 0. The head's clothing set
lists 13 models, read from RAM: the face, then three each of chin, eyes, lips and nose.
`GetMorphTargetResID` calls `CalculateModelIndexFromModelCodeIndexAndStyleIndex` with a fixed code for
each region, and the result is a place in that list:

| Region | Code | Place | Targets, for value 0, 2, 3 |
|---|---|---|---|
| 0 | 2 | 4 | `eyes_base_big`, `_asian`, `_caveman` |
| 1 | 4 | 10 | `nose_base_flat`, `_hook`, `_short` |
| 2 | 3 | 7 | `lips_base_full`, `_narrow`, `_thin` |
| 3 | 1 | 1 | `chin_base_1`, `_2`, `_3` |

Each region's value picks weights (`GetSimFacialMorphTargetIndicesAndWeights`):

| Value | Weights |
|---|---|
| 0 to 1 | First target at `1 - v` |
| 1 | Nothing. A region at its normal value is skipped altogether |
| 1 to 2 | Second target at `v - 1` |
| 2 to 3 | Second at `3 - v`, third at `v - 2` |

`GetMorphTargetWeightScaleFactor` is 1. Each target adds `offset * weight`. Only the head has targets.
Targets are small: at most 40/4096 m (1 cm), on 48 to 104 vertices.

The lattice works from the rest position, so its movement and the face offsets simply add. Call
`apply_lattices` first, then `apply_face`. A face with two targets in one region may be 1/4096 m off,
because the game cuts to whole units after summing a region, and `apply_face` after each target.

### Disagreements with `docs/ASSET_FORMATS.md`

- "They're treated here at full strength" in Assembling a Sim, and `tools/sim.py`: the default Sim has
  no feature offsets at all. A feature is at full strength only at 0, 2 or 3, for one region.
- "Slot 4. The type mask treats it as a plain vertex attribute": now explained, as above.
- "Morph data ... which may be body shapes": they are.
- The batch table's `Open; optional` for slot 4, and the note that the PS2 array order hasn't been
  checked: the order is the Wii's, proved by the fit test.

### Negative controls

`scratch/control.py` breaks one thing at a time:

| Change | Result |
|---|---|
| None | 769 models pass, 0 fail |
| Count read 4 bytes late | 0 lattices found in the 769; 35 models fail on garbage |
| Each lattice 12 bytes short | All 769 fail |
| Grid read z first | 704 fail |
| Box from all points, not inner ones | 704 fail |
| Tag read as the id, not half | No vertex finds a lattice |
| RAM test with Chris's values (1.0) | 34 of 468, 20 of 432 |
| RAM test with Catmull-Rom | 34 of 468, 21 of 432 |
| RAM test with keyframes swapped | 34 of 468, 20 of 432 |
| RAM test, wrong value in one region | Only that region's vertices fail |

In the RAM results, the 34 and 20 are the vertices that never move, so every moving vertex fails.

### Renders

Grey software renders are in `renders/`, with `scratch/render.py`. They're a rough check only:

- `body_all_0_05_1_15_2.png`: 0 is thin, 1 normal, 2 heavy, with no gaps at the seams.
- `body_regions_torso_arms_legs_at_0_then_2.png`: each region moves only its own area.
- `head_values_0_to_7.png`: a smooth run from long and narrow to square.
- `face_*.png`: small changes. Nose and chin show them; these are too low-poly to judge much.
- `control_broken.png`: left and right arm lattices swapped, which flattens the arms into a rod. The
  scrambled leg keyframes in the same control are hard to see.

Open:
- No morphed face has been compared with the game. A Sim with non-default face settings in a RAM
  snapshot would settle targets, regions and the value table in one test. The same goes for a head
  value other than 3 and any body value above 1.
- Which region's name the game's UI uses, and whether the PS2 Create-a-Sim can set head shape or
  face values at all. Both Sims in the snapshot are at defaults.
- Male Sims weren't compared with RAM; the table and code are shared, but the data isn't.
- Why about 3% of normals in RAM differ from the disc, some by a lot, such as (0, -113, -56) to
  (-3, 38, -121). The lattice code doesn't touch them. They may be seam fixes, or a mismatch in my
  batch pairing on repeated strips.
- The `f32` after the arrays as a position scale.
- `SimBodyPart` slot 0x1A0 was confirmed as `CalculateModelIndex...` for the head only.
