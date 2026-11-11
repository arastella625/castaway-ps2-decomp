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
| `IMU_draw_one` | `"1"` if `+0x1A8`, else `"0"`. Set by the menu builder (`0x324D78`) when the object has 3 or more top-level entries and the caller asks for it. The list is then cut to its first interaction. Wii's equivalent is `Set2LineMoreEnable(true)`. See "How the item list is built and ordered" |
| `IMU_show_arrow_left` | `IMU_DirectControl`'s value if `+0x1BC` is set, else `"0"` |
| `IMU_show_arrow_right` | `IMU_DirectControl`'s value if `+0x1B8` is set, else `"0"` |
| `IMU_title` | The object's name (a Sim's own name for a person) |
| `IMU_MoreText` | The UI string `more` |
| `IMU_count` | The harvest count (`+0x1CC`) when above 0, else empty |
| `IMU_00` … | Item *NN*'s name in the current menu, or empty past the end |
| `IMU_icon` | A 2D object, not text; see "The icon box" |

So the arrows are the left and right object-cycling arrows, and they only show in direct control.

**What sets the arrow flags. Proven, PS2**, with Wii's names. Only three places write `+0x1B8` and `+0x1BC`:

- **Cleared:** the constructor (`0x2C142C`) and `TerminateMenu` (`0x2C28A0`) set both to 0.
- **`DirectInteractor::SetLeftArrow(b)`** (`0x176128`) and **`SetRightArrow(b)`** (`0x176150`) write `+0x1BC` and `+0x1B8` on the HUD's (`0x514C04`) action menu (`+0x90`), if there is one.
- **When the menu opens in direct control** (Wii `DirectInteractor::Update`, PS2 `0x14B4B4`), the objects list is `GetInteractionMenuObjectList` (`0x14B7D0`): the cursor-highlighted objects near the Sim. If the list isn't empty and the menu isn't locked, it sets **left off** and **right on if the list has 2 or more objects**.
- **When the player cycles** to another object, `ResetDisplayArrows` (`0x176178`) sets **left = the current object isn't the list's first**, and **right = it isn't the last**. There's no wrap-around: at either end, that side's arrow goes off. The flag rule is proven; that cycling stops at the ends rather than wrapping is likely, from Wii's `Update`, which falls back to the list's last or first object when it finds no next or previous one.

**The visible window. Proven:** `UpdateGetLocalizationItems(sel)` (`0x2DFE40`) binds the movie's
**12 text slots** to the names `IMU_` + `%0.2d` of **max(sel − 6, 0)** onward. So the movie always
holds 12 consecutive item names around the highlighted item: up to 6 before it, and the rest
after. It's called with the selection whenever that changes. Which of those 12 the movie draws,
and where, is in the movie.

**The numbers are absolute item numbers. Proven, PS2.**

- **How a slot is bound.** Slot k (0 to 11) is a buffer at `+0x174 + 4k`. It's filled with `IMU_` and `%0.2d` of start + k, where start = max(sel − 6, 0).
- **How a name is read.** When the movie asks for a name starting `IMU_`, `GetLocalizable` (`0x2C2D40`) reads the number after it with `atoi` (`0x3E0188`). That's an index into **the current menu level**, `+0xF0 + 4 × (+0x150)`, so a submenu's own items.
- **What it returns.** Below that menu's count, it's that item's label (item `+0x44`). Past the end it's empty.

| Selection | Slot 0 … slot 11 |
|---|---|
| 0 to 6 | `IMU_00` … `IMU_11` |
| 7 | `IMU_01` … `IMU_12` |
| 9 | `IMU_03` … `IMU_14` |
| n ≥ 6 | `IMU_`(n − 6) … `IMU_`(n + 5) |

So with 5 items and item 0 highlighted, slots 0 to 4 show the items and slots 5 to 11 are empty. Slot 6 holds the highlighted item once the selection is 6 or more. Before that, slot *sel* does.

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

## How the item list is built and ordered

The steps are **read on Wii** (`FlashPiMenu`, `ObjTestSim`, `ActionMenu::MenuItem`). PS2's builder makes the same calls in the same order (`0x324D78` and `0x3250B0` below), and the draw-one and sort steps are checked on PS2. Likely overall, with the PS2-checked parts marked.

**1. The interactions, in tree table order.** `CollectInteractionsForObject(object, list, max)` (PS2 `0x34B150`) asks `ObjTestSim::AppendInteractionsForMenu` once per tile of a multi-tile object. Each call walks the object's tree table **from entry 0 upward**, runs each entry's check tree (`RunMenuCheckTree`) and **appends** (`push_back`) every interaction that passes and isn't hidden. The interaction's name can be one the check tree made with MakeActionString. One extra goes first: when the target is another Sim and my current action's entry allows joining, an interaction built from that action is tried before entry 0. **So the list's order is the tree table's entry order**, tile by tile.

**2. Draw-one** (`AddObjectInteractionsToMenu`; **proven, PS2** `0x324D78` on).
- **Count:** with more than 2 interactions, it counts the menu's top-level entries, stopping at 3. Each interaction without a `/` counts as one. Each new group (the name before the first `/`, searched from the second character) counts as one, though only the first two groups are remembered.
- **3 or more, with the caller's flag set:** the list is collected again with a **maximum of 1**, and `IMU_draw_one` (Wii `Set2LineMoreEnable`) is set to 1.
- **Otherwise** it's 0.

So an object with 3 or more top-level entries, when the caller asks for it, shows **only its first interaction** plus the movie's "more".

**3. One submenu per object.** Unless the caller says it's the only object, everything goes inside a submenu named after the object (`GetObjectMenuName`, PS2 `0x3495D8`). `AddMenu` reuses an existing child with the same name and object.

**4. Grouping: `CreateInteractionMenu`** (PS2 `0x3250B0`). Repeat until the list is empty:

1. **Take the first remaining interaction.** Its group is the text before its first `/`; with no `/`, it's the whole name.
2. **The group's colour** is the highest-priority text colour among the interactions in that group.
3. **With a `/`,** `AddMenu` opens the group as a submenu titled with that text.
4. **Sweep the list from the start:**
   - **Two slashes** (`Group/Sub/Item`): it opens a nested submenu `Sub`, adds **every** `Group/Sub/…` interaction to it in list order, removes them, closes it, and restarts the sweep.
   - **One slash in this group,** or no slash when the group itself has none: it's added as an item labelled with the text after the `/` (or the whole name) and removed.
5. **Close the group submenu.**

The resulting order:
- **Groups appear in the order of their first member.**
- **Within a group,** items keep tree table order, except that a nested submenu sits where its first member was and gathers all its members.
- **Ungrouped interactions all appear together,** at the place of the first one.

**Adding is appending.** `AddSubItem` walks to the end of the current submenu and inserts there, so item 0 of each menu is the first one added.

**5. The selected Sim last** (`SortSelectedSimToBottom`; called from `ShowMenu` on the root, PS2 `0x2C3120` with the root at `+0x90`).
- **What it does:** it finds the item whose object is the selected Sim and moves it to the **end** of the list.
- **Where it applies:** the root list, and recursively only the first item's submenu.
- **When it matters:** in a menu of several objects, where each object is a submenu, so the selected Sim's own entry ends up last.

**Item text.** Each label is wrapped in a colour tag for the movie, `<COLOR_FFrrggbb>` (format at PS2 `0x4D95F0`). The colour comes from the table at `0x4D9608`: colour 1 `1AAF2B` (green), 2 `F5F500` (yellow), 3 `911A12` (red), 4 `808080` (grey); colour 0 has no tag. On PS2 a coloured label also has its spaces replaced by `_` (`0x3252E8`). Likely the movie turns them back.

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
