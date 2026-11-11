# UI: the interaction menu

How the game places, sizes and fills the interaction menu: the Flash (Apt) movie
`tsc5_interaction_menu`, driven by `ActionMenu`. Read on PS2, with Wii for the names. Addresses
are PS2 unless they say Wii. **Proven** means read in the PS2 code, or matched; **likely** means
read on Wii only, or inferred from where something is used.

The short version for a port:

- **Where:** the game passes one point, in the movie's 640 × 448 stage units.
  - **In direct control:** the fixed point **(320, 254)**.
  - **Otherwise:** the **cursor's position**.
  - The game never projects the object to the screen, and never offsets or clamps the menu itself.
- **Scale:** Apt is told the stage is 640 × 448 and the PS2 renders at 640 × 448, so the stage
  is the whole screen, drawn 1:1. Nothing is scaled by item count.
- **Layout:** row height, which way the list grows, and how many rows show all belong to the
  movie's own ActionScript. The game passes only the point, the item count, the highlighted item
  and the movie's last choice, and serves the text through the variables below.

## Where the menu opens

**Proven, PS2: `ActionMenu::ShowMenu`** (`0x2C1E18`; Wii `ShowMenu__10ActionMenuFb`).

1. `x = 0x140` (320) and `y = 0xFE` (254).
2. If the `InteractorManager` is initialised (`0x178030`), it asks `GetPlayerInteractor(0, 0)`
   (`0x177F18`). Type 0 is the `DirectInteractor`, the analogue-stick direct control.
   - **There is one:** it's kept at `+0x1AC` and the point stays (320, 254).
   - **There isn't:** the point is `CURTarget::Instance()->GetPosition(&x, &y)` (`0x2802E0`).
3. `VisibilityCallfunction(true, count, x, y, refresh)` sends it to the movie (below).

**The cursor is in stage units. Proven:** `CURTarget::GetPosition`
(`0x2802E0`, matched) returns `+0x8C` and `+0x90`.
- **Bounds:** `CURTarget`'s constructor (`0x278BF0`) sets them at `+0x9C`/`+0xA0` = 0/`0x280` (x) and
  `+0xA4`/`+0xA8` = 0/`0x1C0` (y).
- **Clamping:** `ClampCursor` (`0x2801F0`) keeps the cursor inside them. `SetPosition` (`0x280180`)
  clamps it and passes it to the cursor's own movie as `cursorSetPos(x, y)`, formatted with `%d`.

So the cursor is 0 to 640 by 0 to 448, the same space as the menu's stage. The point is where
the cursor is when the menu opens, not where the object is. In direct control there's no cursor,
and the menu opens at (320, 254), a little below the middle of the stage.

Wii has extra statics for its pointer: `m_ActionMenuPosition_BottomX` = 0.834,
`m_ActionMenuPreviousObjectIcon_X` = 0.6 and `m_ActionMenuNextObjectIcon_X` = 0.805. Its
`ShowMenu` also gives the pointer a hit box:
`SetBoundingBox(0.834 − 0.65, 0.68 − 0.12 × n, 0.834, 0.68)`, with n = min(item count, 6), or 2
when `+0x1B8` is set. **Likely, Wii only,** in screen fractions. This suggests a list of up to 6
rows, each 0.12 of the screen high (about 54 of 448), growing up from 0.68 (about 305) and
spanning x 0.184 to 0.834 (about 118 to 534 of 640). PS2 has none of this: its menu doesn't need
a hit box.

## Scale: stage to screen

**Proven, PS2 and Wii.** `AptAuxPCEorGL_Initialize` (`0x301E90`) fills Apt's init callbacks.
- **The stage size:** `+0xA4` is `getStageWidth` (`0x344108`), returning 640.0, and `+0xA0` is
  `getStageHeight` (`0x344118`), returning 448.0. Wii stores its `getStageWidth__Fv` and
  `getStageHeight__Fv` in the same two slots, with the same values (`0x44200000`,
  `0x43E00000`).
- **The renderer:** PS2's renderer (`0x4A0728`, also used by the Apt glue) is set up at `0x419C38`
  with 640 at `+0x18` and `+0x2CC`, and 448 at `+0x1C` and `+0x2D0`.

So the stage is the screen, 1:1. Nothing in `ActionMenu` scales the movie or depends on the item
count. **For a port:** scale the 640 × 448 stage uniformly to the window, and map every point the
game passes the same way.

## What the game passes into the movie

Every call goes through `AptViewer::NewCallFunction` (`0x306610`, viewer at `0x52570C`), with
the arguments as strings. The `%d` format is `0x4CFC10`.

| Call | Arguments | When | Where |
|---|---|---|---|
| `imuShow` | x, y, count | The menu opens | `VisibilityCallfunction` `0x2DFB08`, called from `ShowMenu` |
| `imuRefresh` | count, then `"1"`, `"0"` | Straight after `imuShow` when count ≥ 3, with `+0x8C` = 1, the item names re-bound from 0 and the icon updated | the same |
| `imuRefresh` | count, `"0"`, `"0"` | `VisibilityCallfunction` with its refresh flag | the same |
| `imuRefresh` | count, selected, saved choice | `DoRefresh` (`0x2DFEF0`): when entering a submenu (0, 0), and on backing out (the selection and choice saved for that level) | `BackOutMenu` `0x2C1FE8`, `AcceptMenu` `0x2C2158` |
| `imuHide` | none | The menu closes | `VisibilityCallfunction` |
| `imuInput` | `"up"`, `"down"`, `"select"` or `"cancel"` | Controller events 0x33, 0x34 and so on, one per press | `Update` `0x2C2370` |
| `imuSetBackdrop` | `"1"` or `"0"` | `"0"` every time the menu is shown, refreshed or hidden. `"1"` from `H2DTarget::Update` (`0x29F940`) on a controller state change when the player's Sim allows it (likely) | `SetBackdrop` `0x2E04C8` |
| `imuShowCount` | none | The harvest count `+0x1CC` has changed | `Update` |
| `imuHideCount` | none | 4 seconds later | `Update` |
| `imuBlank` | none | In `Update` (`0x2C2720`), when `+0x1C4` is set and two checks just before it are both false (one is a virtual call on an object at `+0x138`/`+0x13C` of its vtable; what they test isn't read). `IMU_icon` is reset to `empty` and `+0x1C4` cleared with it | `Update` |
| `imuClearList` | none | The menu ends | `TerminateMenu` `0x2C28A0` |

**What the arguments mean:**
- **count** is `+0x88`, the number of items in the current menu or submenu, uncapped.
- **selected** is `+0x8C`.
- **The saved choice** is the `IMU_Choice` value the movie last reported at that level.
  `AcceptMenu` saves it at `+0x130[level]`, and `BackOutMenu` passes it back.

**The harvest count. Proven** (both functions matched):
- `ActionMenu::SetupHarvestCount` (`0x2E0520`) keeps a count only if it's 1 to 9,998. It's called
  by GenericSimCall 0x11 with t0.
- `Update` then calls `imuShowCount`, plays sound `0x5E91B4E5`, and starts a timer of 4.0 that
  drops 1/60 (`0x49FDC4`) each update. At 0 it calls `imuHideCount` and clears the count.

**There is no `imuScrollArrowEnable` or `imuObjectButtonEnable` on PS2:** neither string is in
the executable. The equivalents are variables the movie reads (below). `imuSetBackdrop` and the
rest are as listed.

### Variables the movie reads

**Proven, PS2: `ActionMenu::GetVariable`** (`0x2C29C8`) and **`GetLocalizable`** (`0x2C2D40`).
The movie asks for these by name. The constructor (around `0x2C1370`) registers them, and
registers the sound callbacks `IMU_SndOpen` to `IMU_SndRefresh`.

| Variable | Value |
|---|---|
| `IMU_exists` | `"loaded"` |
| `IMU_DirectControl` | `"1"` if `ShowMenu` found a direct-control interactor (`+0x1AC`), else `"0"` |
| `IMU_draw_one` | `"1"` if `+0x1A8`, else `"0"`. Set by the menu builder (`0x324D78`) when the object's interactions fall into 3 or more top-level groups. Wii's equivalent is `Set2LineMoreEnable(true)`. Likely |
| `IMU_show_arrow_left` | `IMU_DirectControl`'s value if `+0x1BC` is set, else `"0"` |
| `IMU_show_arrow_right` | `IMU_DirectControl`'s value if `+0x1B8` is set, else `"0"` |
| `IMU_title` | The object's name (a Sim's own name for a person) |
| `IMU_MoreText` | The UI string `more` |
| `IMU_count` | The harvest count (`+0x1CC`) when above 0, else empty |
| `IMU_00` … | Item *NN*'s name in the current menu, or empty past the end |
| `IMU_icon` | A 2D object, not text; see "The icon box" |

So the arrows are the left and right object-cycling arrows, and they only show in direct control.

**The visible window. Proven:** `UpdateGetLocalizationItems(sel)` (`0x2DFE40`) binds the movie's
**12 text slots** to the names `IMU_` + `%0.2d` of **max(sel − 6, 0)** onward. So the movie always
holds 12 consecutive item names around the highlighted item: up to 6 before it, and the rest
after. It's called with the selection whenever that changes. Which of those 12 the movie draws,
and where, is in the movie.

### Variables the movie sets

**Proven, PS2: `ActionMenu::SetVariable`** (`0x2C2B00`). The movie owns the highlight.

- **`IMU_Highlight` = n:** if n is below the count, the selection `+0x8C` becomes n, the window is
  re-bound and the icon updated.
- **`IMU_Choice` = n:**
  - **n < 0:** `BackOutMenu` (`0x2C1FE8`).
  - **Otherwise:** n is kept at `+0x158`, or 0 if it's past the count, and `AcceptMenu`
    (`0x2C2158`) opens the selected item's submenu or runs it.

Both are ignored while `+0x164` is 0, or while a check at `0x2BB290` on `0x514F1C` is true.
`ShowMenu` and `DoRefresh` clear `+0x164`. What sets it again isn't read: likely the movie's
`IMU_RefreshCallback` once it has redrawn, so a port can accept input once a refresh is done.

## The icon box

**Proven, PS2 (`ActionMenu::UpdateIcon`, `0x2C1D10`), with Wii's names.** `IMU_icon` shows the
selected item's picture through `UIObjectBase::Install2DObject`:

1. **The item has a texture ID** (`+0x4C`): `Install2DObject("IMU_icon", id, 0)` (`0x27A1D0`).
2. **Otherwise, a shader** (`+0x50`): `Install2DObject("IMU_icon", shader, 0)` (`0x27A268`).
3. **Otherwise, if one was shown** (`+0x154`): reset to `0x9F93FF74`, the texture `empty`.

**Where the picture comes from. Proven on PS2 and Wii.** The menu builder fills items `+0x4C`
and `+0x50` from **`GetShaderForObject(interaction->GetIconObject(), &shader)`** (`0x3260E8`;
`GetIconObject` is `Interaction +0x10`). It returns the texture ID and sets the shader:

- **No object:** 0.
- **A definition with GUID `0x7C4`:** `0xFE47C11A`, the texture `go_here`.
- **A Sim:** the selector's thumbnail shader (`ObjSelector::GetThumbnail`, `0x19E6C0`), and its
  resource ID (shader `+0x8`) as the texture ID.
- **A multi-tile object:** the first tile whose definition has a `ResData` row.
- **Anything else:** its definition's `ResData` row (definition `+0xC0`).

The texture ID is **word `+0x1C` of that `ResData` row**, a texture name hash. On the disc, 504
of the 616 rows name a texture there, 501 of them called `…_action_queue`. The other 112 are 0,
so those objects show no icon. For example:

| Object | ID | Texture |
|---|---|---|
| `resource_fruit_coconut` | `0x3747CE02` | `propCoconut_action_queue` |
| `source_tree_coconutpalm` | `0x0292BDC6` | `isCoconutPalmTreeFull_action_queue` |
| `resource_shell_clam` | `0x673C2D8F` | `isShellClam_action_queue` |
| `tool_spear_simple` | `0xB35C5F28` | `isPropSimpleSpear_action_queue` |

They are the same pictures the action queue shows: `ActionQueue::SetIconObject` (`0x1A6238`) and
`ActionQueue::Insert` call the same `GetShaderForObject`.

## Not read

- **Inside the movie:** row height, which way the list grows, how many of the 12 names show, where
  the title, count and icon sit, and how the movie offsets the box from the point it's given and
  keeps it on screen. These are in `tsc5_interaction_menu`'s ActionScript and timeline. The
  game's code doesn't do any of them.
- **What sets `+0x1B8`/`+0x1BC`, the arrow flags.** Likely, from Wii's `SetLeftArrowEnable` and
  `SetRightArrowEnable`: the object-cycling in direct control.
