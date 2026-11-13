# BHAV: how object behaviour runs

Objects do everything through trees: small programs stored as `BHAV` chunks in `EDITHTRE` sets (the chunk format is in [world.md](world.md)). This page says what the instructions mean, for anyone writing an interpreter. The source for most of it is the matched C++ in `src/game/simulator/core2/` and `src/game/simulator/core/Behavior.cpp`.

How sure each part is:

- **Proven:** read from PS2 code that byte-matches, or tested against all 5,396 trees on the disc by `tools/bhav.py`. It has a negative control: each rule also runs at a wrong offset and must fail there.
- **Likely:** read from the Wii build's code, with the PS2 code agreeing in shape. Wii and PS2 are the same build, so this is strong, but the PS2 function itself isn't matched yet.
- **Open:** not worked out.

Run the check with:

```bash
python -I tools/bhav.py <game.iso>
```

## Tree layout

**Proven.** A tree is a 12-byte header, then 12-byte nodes. `ObjectDataBehaviorTree::GetNode` computes the address as header + 12 + index x 12.

| Offset | Type | Meaning |
|---|---|---|
| 0 | s16 | Signature: 0x8002 on 5,390 trees, 0x8001 and 0x8000 on 3 each |
| 2 | s16 | Node count |
| 4 | u8 | Type |
| 5 | u8 | Parameter count |
| 6 | u8 | Local count |
| 7 | u8 | 0 on every tree; open |
| 8 | u8 | Open. Values fall off from 1 upward, which looks like a version count |

Old signatures limit the counts. 0x8002 and above use the bytes as they are. 0x8001 caps both at 4. 0x8000 always has 4 parameters and no locals.

Type 4 makes `Gosub` start at the node named by the first parameter, clamped to the last node, instead of node 0. **No tree on this disc has type 4**: the type is 0 on 5,390 trees and 0x16 on 6, so an interpreter can leave this out until proved otherwise. What 0x16 means is open.

## Instructions

**Proven.**

| Offset | Type | Meaning |
|---|---|---|
| 0 | u16 | Opcode. Bit 15 is masked off everywhere, so only 15 bits count |
| 2 | u8 | Target if the instruction returned true |
| 3 | u8 | Target if it returned false |
| 4 | 8 bytes | Operands |

A target is a node index, or one of the special values below. All 123,186 targets on the disc are in range. `DoNodeAction` treats any index from 0xFB up as special, so 0xFB and 0xFC are error 0x44C too. No tree uses them.

| Target | Meaning |
|---|---|
| 0xFD | Error. Reaching it stops the tree with error 0x44C |
| 0xFE | Return true to the calling tree |
| 0xFF | Return false to the calling tree |

Opcode ranges, from `Behavior::GetResFileID`:

| Opcode | Meaning | Uses on disc |
|---|---|---|
| 0x0000 to 0x00FF | Primitive, built into the game | 50,849 |
| 0x0100 to 0x0FFF | Call a global tree | 5,822 |
| 0x1000 to 0x1FFF | Call the object's private tree | 1,463 |
| 0x2000 to 0x2328 | Call a semi-global tree, shared by a group of objects | 3,453 |
| 0x7FFF | Blank node with all-FF operands (6, in two trees). It isn't a tree, so running it stops with an error | 6 |

Each object has one namespace ID for private trees and one for semi-global trees, kept in its `ObjSelector`; there is one global ID (a static at `0x492898`). A tree is found by that ID plus the opcode.

**Which set an object's semi-global trees are in. Proven:** `ResFile` row `+0x34` is a pointer to another `ResFile` row, and the set with that row's name holds them. `ObjectFolder::GetSemiGlobalID` reads it, and the result goes in `ObjSelector +8`. 579 of the 698 rows have one, and a null means the object has no semi-global set. All 3,453 semi-global calls on the disc find their tree this way; the control, the next set along, finds 141. A `ResFile` row and the `EDITHTRE` set of the same name go together.

The selector decides this, not the running tree's own set. A semi-global tree that calls a private tree (0x1000 up) calls the private tree of the object it's running on, and the same goes for constants. Semi-global sets link to nothing, except `car_sg`, which links to itself. `counter_sg` is the only set with semi-global trees that nothing links to, so its trees never run.

**Calls.** The 8 operand bytes are four s16 parameters. All FF means "pass nothing": the call uses the object's default parameters (`TreeSim::mDefaultParams`). 6,887 of the 10,744 calls do this.

## Running a tree

**Proven.** `TreeSim` holds a stack of frames. Each frame has the tree, the current node, the stack object, and that tree's parameters and locals as s16 values. Locals sit straight after parameters, and each frame is rounded up to a multiple of four values.

`TreeSim::Simulate` is the tick. It clears the iteration count and calls `DoNodeAction` on the top frame until something stops it:

| `DoNodeAction` returns | Meaning | `Simulate` |
|---|---|---|
| 0 | Keep going in this tick | loops |
| 1 | Stop until the next tick | returns 1 |
| 2 | Failed | returns 0 |

For each node, `DoNodeAction` adds one to the iteration count and then tests it. So 24,999 nodes run in a tick, and the 25,000th stops the tree with error 0x44D (`sMaxIterations`, `0x492418`), which catches infinite loops. Then:

- A call pushes a frame with the callee's parameter and local counts. If that fails, the tree stops with error 0x3E8 when the tree can still be loaded, and 0x44E when it can't.
- A primitive goes to `TryElement`, the virtual switch below, and the result decides what happens next:

| `TryElement` returns | Effect |
|---|---|
| 1 | Go to the true target; last result = true |
| 0 | Go to the false target; last result = false |
| 2 | Stop for this tick. The node doesn't change, so it runs again next tick. This is how a primitive waits |
| 3 | Run the same node again in this tick |
| -1, 4, other | Failed: `DoNodeAction` returns 2 and `Simulate` 0, and the node doesn't change |

**What a failure without an error does. Read on PS2 and Wii, likely.** When nothing set an error (no `cXObject::Error`), a failed primitive's node runs again next tick, like 2. The only difference is that `cXObject::Simulate` skips that tick's upkeep: the countdown variable and `SpriteSlot::Tick`, which ages balloons. Dialog (below) returns 4 to wait for the player, and GenericSimCall 8 and 0x10 return 4 while they wait.

- A node index of 0xFE or 0xFF pops the frame, sets the last result and takes the caller's true or false target, so a call looks like a primitive to its caller. Popping the last frame is error 0x3E9.

**Proven**, from the matched code, for a port's edge cases:

- A new frame's locals aren't cleared. `StackElem::Setup` copies parameters only, so locals hold whatever was in stack memory. A tree that reads a local before writing it is reading old data; a port should zero them.
- A root tree under `Simulate` can't return, because popping the last frame is error 0x3E9. Only `RunCheckTree`'s marker frame makes a return safe.
- `RunCheckTree` leaves the stack and iteration count alone when it fails, and `HandleError` cleans up. A primitive that fails without setting an error, such as 0x26 on a Sim, becomes 0x44F there.

A frame also has a word at `+8`. When a callee returns and it's set, the caller stays on the same node instead of taking a target. **Likely** that's how a primitive that starts a subtree gets run again afterwards; `mWaiting` is our name. Moving to a new node clears it.

`RunCheckTree` runs a whole tree inside one call, with its own iteration count, and returns the tree's true or false. Check trees use it to answer a question without waiting. It pushes a marker frame with no behaviour, so it knows when the tree has returned.

`Behavior::GetNodeRef` keeps recently used trees in `s_cachedTrees`, most recent first; a miss reuses the last slot. A port with everything in memory doesn't need this.

**What an error does. Likely, from both builds:** `cXObject::Error` sets bit 0x100 of the object's flags (`+0xE4`) and a flag in `ObjectModule`. It doesn't stop anything itself; the tree stops because the primitive returns -1. `cXObject::HandleError` runs later: if the flag is set and an error is recorded, it clears the flag, calls `ObjectModule::CleanupPeople` and then the object's `Cleanup(0)`. If the object has a tree for entry point 3, `Cleanup` runs it, sets object data 0x3E to 0, clears bit 5 of data 8, and resets the stack to the tree for entry point 1, on every tile of a multi-tile object. With no such tree it picks the object up, resets it and puts it back. Both paths then set the idle status and quiet the object's sounds. So one error resets that object rather than ending the game.

**What an error does to a Sim and its actions.** Read on Wii (`cXPerson::Cleanup(cXObject*)`); PS2 `cXPerson::Cleanup` (`0x1B0D68`) makes the same calls (`TreeSim::Reset`, `RunTree`, `ActionQueue::Remove` `0x1A69B0`, `0x1DD210`, `0x1DD2B8`). Likely.

- **Cleanup(0) on the Sim**, after the Sim's own tree errored, does all of this:
  - drops and cleans up what it holds, popping its stack;
  - person data 0, 0x1B and data 0x22, 9 to 0;
  - stops all its animation tracks and balloons;
  - clears its routes and its places in other Sims' routing and trapped queues;
  - finds it a good location if it's out of bounds;
  - **resets its stack to the main tree** (entry point 1), and **runs its cleanup tree** (entry point 3).
- **The running action is completed** (`CompleteCurrentAction`, PS2 `0x1DD210`): removed from the queue with `ActionQueue::Remove`. **Nothing else runs for it, no entry point 4.**
- **Person data 0xE** goes to 0 (−100 if the Sim is the immediate-action Sim).
- **Every waiting action** is then deleted with `DeleteTopAction` (PS2 `0x1DD2B8`) until the queue is empty. That's **`ActionSkipped`**, so **each one's entry point 4 runs**.
- **When another object errors**, `CleanupPeople(object)` calls each Sim's `Cleanup(object)`:
  - **Tied to the object** (holding it, routing to it, or its current action's icon object): the Sim gets the full reset above.
  - **Otherwise:**
    - if the running action's stack object is that object, the action is completed as above;
    - if only its icon is, the icon is cleared;
    - every waiting action whose stack or icon object is that object is removed with `ActionQueue::Remove`, **without** entry point 4;
    - routes that were moving that object (a Sim, type 2) are marked done.
- **For a port:**
  - a Sim's error ends its running action **silently**, runs entry point 4 for each waiting one, empties the queue, and restarts the Sim on its main tree after its cleanup tree;
  - "ended by an error" for the running action matches the first part.

Error codes seen so far:

| Code | Meaning |
|---|---|
| 0x001 | Expression operator above 0x12 |
| 0x002 | Temp number out of range |
| 0x005 | Primitive not in the `TryElement` switch |
| 0x006 | Attribute number out of range |
| 0x007 | Variable doesn't exist, or a bad action queue index |
| 0x008 | Parameter or local number out of range |
| 0x009 | Scope out of range, or `InterpWriteValue` given a scope it can't write. Expression can't reach the second: see "Writing" |
| 0x00A | No stack object |
| 0x00D | Bit number above 16 in an expression |
| 0x00F | Scope 0xA with data other than 0 |
| 0x004 | Random's limit is 0 |
| 0x013 | An expression tried to write to something it can't |
| 0x014 | CheckObject test 4 with a relationship type outside 1 to 9 |
| 0x015 | No stack object, in scopes that need a Sim or a slot |
| 0x017 | TestObjectType's object doesn't exist |
| 0x01A | Motive number out of range, or ad number out of range |
| 0x01B | No such object slot |
| 0x01C | Not a Sim |
| 0x021 | Person data field doesn't exist |
| 0x023 | Definition word number out of range |
| 0x026 | Division by zero |
| 0x028 | No such room |
| 0x029 | Room data number out of range |
| 0x01E | SetToNext search type that doesn't exist |
| 0x02F | Tree constant not found |
| 0x032 | Indexed data number out of range |
| 0x033 | SetToNext's current object doesn't exist, in search type 8 |
| 0x036 | Tutorial mode isn't 0 or 1 |
| 0x038 | No such Neighbor |
| 0x03A | Scope 0x21, which always fails |
| 0x03C | Neighbourhood variable or goal number out of range |
| 0x03E | TreeBreak stopped the tree |
| 0x03F | Global number out of range |
| 0x040 | Type attribute number out of range |
| 0x05D | No such ingredient, token or plan |
| 0x3E8 | A call failed, though its tree loads |
| 0x3E9 | Stack underflow |
| 0x44C | Target 0xFD reached, or a bad transition |
| 0x44D | Over 25,000 iterations in one tick |
| 0x44E | Node or tree missing |
| 0x44F | A check tree didn't finish |

## Primitives

**Proven** for the numbers and addresses: `cXObject::TryElement` byte-matches, so each number goes to the function at that address, and the three inlined cases below are exact. The name is the Wii function at the same place.

The switch is `cXObject::TryElement` (`0x234678`), 73 cases, and `cXPerson::TryElement` (`0x1AD720`), which handles the Sim-only opcodes and passes everything else to the first. It isn't matched yet, so its column below is still **Likely**. The names are from pairing the two platforms' jump tables, and they hold up: of the 56 cases that call a function, 53 keep exactly the same order in the Wii and PS2 binaries. The other three are explained: `TreeBreak`'s case is inlined, and `KillSounds` and `HUDData` are tiny functions placed elsewhere. Nine names also agree with the name list, which was made from different evidence.

On a non-Sim object, an opcode with no `cXObject` handler is error 5. Opcode 0x26 on a Sim returns -1 at once. Five primitives are inlined in the switch rather than being functions. **Proven:**

- `WantsAndFearsNotify` and `ChangeWantFearTree` return 1 and do nothing else.
- `TreeBreak` reads a value (`+0` data, `+2` scope) and returns 1 if it's 0. Otherwise `+4` bit 0 returns 1 (log only), bit 1 stops the tree with error 0x3E, and neither returns 1. The literal form (135 of 235 nodes, value 1, flags 0) just returns 1.
- **`TreeBreak` with all-FF operands is a deliberate trap, and it does stop the tree.** Scope -1 isn't special: `InterpValue` gives error 9, on both builds, with nothing in front of it. All 99 such nodes have false target 0xFD, 86 are reached only by a false branch and 3 by both, and 10 by nothing. `global/Sim - Is Alive?` is `CheckObject` with its false branch going to one. So it's an "this can't happen" check: it runs only when the test before it failed, and the object gets reset (see "What an error does"). A port should keep the error.
- `Tutorial` claims the tutorial for this object (`+0` is 0) or releases it (1). Only one object has it at a time: claiming fails if another has it, releasing fails if this one doesn't. Any other value is error 0x36.
- `ParticleEffect` needs a Sim (error 0x1C). It passes the operands as two words to a virtual on the Sim's animator, at `+0x5F0`, and returns 1 if that returned 1, otherwise 0. No tree uses it.

| Opcode | Primitive | Address | Sim override | Address | Uses |
|---|---|---|---|---|---|
| 0x00 | Idle | 0x230B40 |  |  | 3 |
| 0x01 | GenericSimCall | 0x22D518 |  |  | 290 |
| 0x02 | Expression | 0x234F90 |  |  | 36093 |
| 0x03 | (none) |  | FindBestAction | 0x1AF6B0 | 7 |
| 0x04 | Grab | 0x230ED0 |  |  | 31 |
| 0x05 | Drop | 0x2389E8 |  |  | 7 |
| 0x06 | (none) |  | ChangeSuit | 0x1AEC30 | 317 |
| 0x07 | Update | 0x230CC0 |  |  | 111 |
| 0x08 | Random | 0x231128 |  |  | 445 |
| 0x09 | Burn | 0x22A260 |  |  | 0 |
| 0x0A | Tutorial | inlined |  |  | 0 |
| 0x0B | DistanceTo | 0x233210 |  |  | 144 |
| 0x0C | DirectionTo | 0x233420 |  |  | 39 |
| 0x0D | PushAction | 0x228788 |  |  | 334 |
| 0x0E | FindFunctionalObject | 0x22CFD0 |  |  | 37 |
| 0x0F | TreeBreak | inlined |  |  | 235 |
| 0x10 | FindGoodLocation | 0x224D60 |  |  | 219 |
| 0x11 | (none) |  | IdleForInput | 0x1AEEC0 | 3 |
| 0x12 | KillObject | 0x2309B8 |  |  | 328 |
| 0x13 | IsPersonInArea | 0x2247C8 |  |  | 2 |
| 0x14 | CallFunctionalTree | 0x22D358 |  |  | 79 |
| 0x15 | SyncAudio | 0x23CC68 |  |  | 191 |
| 0x16 | (none) |  | LookTowards | 0x1A8770 | 103 |
| 0x17 | PlaySound | 0x234CF8 |  |  | 574 |
| 0x18 | Relationship | 0x229A40 |  |  | 12 |
| 0x19 | Budget | 0x22BD80 |  |  | 37 |
| 0x1A | Relationship2 | 0x228ED0 |  |  | 358 |
| 0x1B | (none) |  | GotoRelative | 0x1AD320 | 157 |
| 0x1C | CallNamedTree | 0x226948 |  |  | 356 |
| 0x1D | SetMotiveDelta | 0x234310 | SetMotiveDelta | 0x1A83D0 | 401 |
| 0x1E | (none) |  | GosubFoundAction | 0x1A85E0 | 5 |
| 0x1F | SetToNext | 0x22BF10 |  |  | 1248 |
| 0x20 | TestObjectType | 0x2245E8 |  |  | 609 |
| 0x21 | Find5WorstMotives | 0x228D10 |  |  | 0 |
| 0x22 | (none) |  |  |  | 0 |
| 0x23 | UserEvent | 0x2243D8 |  |  | 38 |
| 0x24 | Dialog | 0x2306E0 |  |  | 505 |
| 0x25 | (none) |  | TestInteractingWith | 0x1DCCA8 | 33 |
| 0x26 | (none) |  |  |  | 0 |
| 0x27 | ListenForInput | 0x23C960 |  |  | 19 |
| 0x28 | CameraControl | 0x232378 |  |  | 57 |
| 0x29 | SetBalloon | 0x2262B0 |  |  | 336 |
| 0x2A | CreateObject | 0x22AA30 |  |  | 401 |
| 0x2B | DropOnto | 0x22B770 |  |  | 15 |
| 0x2C | Animate | 0x234558 | Animate | 0x234558 or a vcall | 3953 |
| 0x2D | (none) |  | GotoRoutingSlot | 0x1A8CA8 | 418 |
| 0x2E | Snap | 0x238B88 |  |  | 115 |
| 0x2F | NotifyPlans | 0x23CFD8 |  |  | 60 |
| 0x30 | KillSounds | 0x255CC8 |  |  | 83 |
| 0x31 | NotifyStackObject | 0x233678 |  |  | 236 |
| 0x32 | MakeActionString | 0x2284F8 |  |  | 320 |
| 0x33 | PreloadObject | 0x22B668 |  |  | 3 |
| 0x34 | RoomEffect | 0x231300 |  |  | 12 |
| 0x35 | CheckObject | 0x231958 |  |  | 451 |
| 0x36 | ManageClothing | 0x23D140 |  |  | 17 |
| 0x37 | AnimateObject | 0x231C40 |  |  | 250 |
| 0x38 | CompleteGoal | 0x232018 |  |  | 138 |
| 0x39 | HUDCreate | 0x239CC8 |  |  | 1 |
| 0x3A | HUDAddItem | 0x239E50 |  |  | 3 |
| 0x3B | HUDManage | 0x23A0A0 |  |  | 4 |
| 0x3C | HUDData | 0x255EF0 |  |  | 8 |
| 0x3D | ParticleEffect | inlined |  |  | 0 |
| 0x3E | (none) |  | SocialMode | 0x1DCEB0 | 0 |
| 0x3F | Memory | 0x233898 |  |  | 23 |
| 0x40 | (none) |  | IdleAnimate | 0x1AF460 | 50 |
| 0x41 | PassiveInfluence | 0x234160 |  |  | 39 |
| 0x42 | WantsAndFearsNotify | inlined |  |  | 154 |
| 0x43 | ChangeWantFearTree | inlined |  |  | 13 |
| 0x44 | CustomUI | 0x23A168 |  |  | 15 |
| 0x45 | LotTransition | 0x23A448 |  |  | 11 |
| 0x46 | FamilyInventory | 0x23A9D0 |  |  | 214 |
| 0x47 | (none) |  | FollowSims | 0x1A9228 | 9 |
| 0x48 | SnapObject | 0x22B9C0 |  |  | 70 |

Numbers in common with PC *The Sims* are a useful lead for what a primitive does, but they aren't evidence here. Read the function first.

## Expression (opcode 2)

59% of all instructions, so this is the one to get right first. `TryExpression` is `0x234F90`.

**Proven** for the layout: all 36,093 are in range, and `TryElement` passes the operands straight to it. **Likely** for the rest, but strongly: it's read from the PS2 code itself, and the C++ in `cXObjectInterp.cpp` (under `NON_MATCHING`) has 575 of the game's 628 words the same. What differs is register choice and scheduling, not behaviour.

| Operand offset | Type | Meaning |
|---|---|---|
| 0 | s16 | Left data |
| 2 | s16 | Right data |
| 4 | u8 | No code reads it. 0, 0xFE or 0xFF on disc |
| 5 | s8 | Operator, 0 to 0x12. Above that is error 1 |
| 6 | s8 | Left scope |
| 7 | s8 | Right scope |

It reads the right operand first, then the left. A scope of 7 skips `InterpValue` and takes the data word, so a literal left has no address: anything that writes it is error 0x13.

| Op | Meaning | Returns | Writes? | Uses |
|---|---|---|---|---|
| 0x00 | left > right | the comparison | | 1,441 |
| 0x01 | left < right | the comparison | | 901 |
| 0x02 | left == right | the comparison | | 7,905 |
| 0x03 | left += right | 1 | yes | 1,125 |
| 0x04 | left -= right | 1 | yes | 403 |
| 0x05 | left = right | 1 | yes | 19,584 |
| 0x06 | left *= right | 1 | yes | 209 |
| 0x07 | left /= right; right 0 is error 0x26 | 1 | yes | 192 |
| 0x08 | Test bit `right` of left, counting from 1 | the bit | | 545 |
| 0x09 | Set bit `right` of left | 1 | yes | 880 |
| 0x0A | Clear bit `right` of left | 1 | see below | 435 |
| 0x0B | ++left, then left < right | the comparison | yes | 194 |
| 0x0C | left %= right; both made positive first; right 0 is error 0x26 | 1 | yes | 104 |
| 0x0D | left &= right | 1 | yes | 11 |
| 0x0E | left >= right | the comparison | | 806 |
| 0x0F | left <= right | the comparison | | 568 |
| 0x10 | left != right | the comparison | | 549 |
| 0x11 | --left, then left > right | the comparison | yes | 239 |
| 0x12 | left \|= right | 1 | yes | 2 |

Rules a port has to copy:

- **Writing.** With an s16 address, the new value goes through `InterpWriteValue`. With no address but a float one (a motive), the sum is done on the float. With neither, it's error 0x13. Operators 9, 0xA, 0xC, 0xD and 0x12 never use the float: on a motive they're error 0x13. This test comes first, so **an expression that writes an unwritable scope is error 0x13, not 9**. Error 9 is `InterpWriteValue`'s own default, which Expression never reaches, because every scope that gives an address is one it can write. The two `left = right` on scope 0x2A are 0x13.
- **A failed write doesn't stop the expression.** `TryExpression` ignores what `InterpWriteValue` returns. Writing scope 4 with a missing variable records error 7 and calls `Error`, and the instruction still takes its true target. That's in the PS2 code: there's no test after any of its write calls.
- **Bit numbers.** Anything above 16 is error 0xD. Below 1 isn't checked.
- **After a write**, for scopes 0xE, 0xF and 0x11, the float is clamped to -100.0 to 100.0. Then if `EdithVariableSet::s_bTaggedGraphicField` is set, it's cleared, and `TagGraphicsField` runs on me (left scope 3) or the stack object (left scope 4).
- **Operator 0xA doesn't count as a write**, so it does neither of those. That's in the code, and it may be a slip in the original. It changes nothing unless the left scope is a motive, which is error 0x13 anyway, or a variable that sets the tag.
- **Operators 9 and 0xA with left data 8** also set a global (`0x49DDA4`, Wii `OnEdithSetFlagField1`) when bit 0x2000 changes. What reads it is open.
- **++ and -- on a motive** would read the s16 afterwards through a null pointer. No tree does it, so a port can leave that out.

Checked against all 36,093 expressions on the disc (a one-off query, not part of `tools/bhav.py`). A motive on the left only meets operators 0 to 5 and 0xE to 0x10. Nothing writes a literal. Every literal bit number is 1 to 16. Two `left = right` have left scope 0x2A, which can't be written, so those two would stop with error 0x13 if they ever ran. Every operator 9 and 0xA with left data 8 has left scope 3 or 4 but one, so that's object variable 8, a flags word.

## Scopes

A value is a scope and a data word. `cXObject::InterpValue` (`0x235968`) reads it, returning 1, or -1 once it has stopped the tree. `InterpWriteValue` (`0x238860`) writes it back.

**Proven** for writing: `InterpWriteValue` byte-matches. A write goes through the address `InterpValue` gave, for the scopes marked yes below. Scopes 3 and 4 use `EdithVariableSet::WriteVar` (error 7 if the variable doesn't exist), and every other scope is error 9. A write through a scope 6 data word that has no address would crash; no tree should do that.

**Likely**, but strongly, for reading: this is read from the PS2 code, not the Wii build, and 2,872 of its 3,060 words line up, with the same register allocation and block order. The C++ is in `cXObjectInterp.cpp` under `NON_MATCHING`. Where the two builds differ the PS2 behaviour is what's written here. Wii calls `ObjectModule::PersonalityAccessed`, `SkillAccessed` and `MotiveAccessed` when a value is read; PS2 has none of those. Scopes 0x27, 0x2A, 0x42 and 0x43 are placed differently in the two jump tables, and the PS2 cases are what's listed.

"Stack object" is the object whose ID is in the frame. If there's no such object it's error 0xA, except where the table says otherwise. Above scope 0x44 is error 9.

| Scope | Value | Errors | Write |
|---|---|---|---|
| 0x00 | My attribute `data` | 6 range | yes |
| 0x01 | Stack object's attribute | 6 | yes |
| 0x02 | My semi-global attribute | 6 | yes |
| 0x03 | My object variable; reading variable 0x19 clears my idle status | 7 no such variable | WriteVar |
| 0x04 | Stack object's variable; the same for 0x19 | | WriteVar |
| 0x05 | Stack object's semi-global attribute | 6 | yes |
| 0x06 | Global `data`, below 0x30. 0x20 and 0x2E are the player cheats' high and low 16 bits, 0xB is the home tiki lot, 3 has no address | 0x3F | yes, if it has an address |
| 0x07 | **Literal**: the data word itself | | no |
| 0x08 | My temp `data`, below 8 | 2 | yes |
| 0x09 | Parameter `data` of this tree. The limit is enforced | 8 | yes |
| 0x0A | The frame's stack object ID. Data must be 0 | 0xF | yes |
| 0x0B | My temp, indexed by my temp `data` | 2 | yes |
| 0x0C | Range of ad `data` of the running check tree. 0 if ads aren't allowed | 0x1A | yes |
| 0x0D | Stack object's temp | 2 | yes |
| 0x0E | My motive `data`, below 16, rounded half away from zero | 0x1A, 0x1C not a Sim | float |
| 0x0F | Stack object's motive | 0x1A, 0x15, 0x1C | float |
| 0x10 | ID of the object in stack object's slot `data` | 0x15, 0x1B no slot | no |
| 0x11 | Stack object's motive, indexed by my temp `data` | 2, 0x1A, 0x15, 0x1C | float |
| 0x12 | My person data field | 0x1C, 0x21 | yes |
| 0x13 | Stack object's person data field | 0x21, 0x15, 0x1C | yes |
| 0x14 | ID of the object in my slot `data` | 0x1B | no |
| 0x15 | Word `data` of stack object's definition, below 0x62 | 0x23 | no |
| 0x16 | Stack object's attribute, indexed by parameter `data`. The game reads the object's attribute count before it tests for no object, so that test can't help; a port should test first and give error 0xA | 8, 6 | yes |
| 0x17 | Data of the room numbered by my temp 0; see below | 0x28, 0x29 | no |
| 0x18 | A virtual call on the neighbourhood with the stack object's ID and `data` (slot 1); what it holds is open | | yes |
| 0x19 | **Local** `data` of this tree. The limit is enforced | 8 | yes |
| 0x1A | Tree constant; see below. Index 0x7F means temp 0 holds it | 0x2F | no |
| 0x1B | Word of an array at `+0x6C` of the stack object, numbered by my temp `data`. It's its own array with a count at `+0x70`, not the attributes: see "Loading an object". No expression uses this scope | 2, 0x32 | yes |
| 0x1C | Personality variable of ad `data`, like 0x0C | 0x1A | yes |
| 0x1D | Minimum of ad `data`, like 0x0C | 0x1A | yes |
| 0x1E | My person data field, numbered by my temp `data` | 0x1C, 2, 0x21 | yes |
| 0x1F | Stack object's person data field, numbered by my temp `data` | 0x15, 0x1C, 2, 0x21 | yes |
| 0x20 | Person data of the Neighbor whose ID is the stack object | 0x38, 0x21 | no |
| 0x21 | Always error 0x3A | 0x3A | no |
| 0x22 | Neighbourhood variable `data`, 1 to 0x23 | 0x3C | yes |
| 0x23 | 1 if the stack object's type has function `data` | | no |
| 0x24 | My type attribute | 0x40 | yes |
| 0x25 | Stack object's type attribute | 0x40 | yes |
| 0x26 | Definition word of the Neighbor whose ID is the stack object | 0x38, 0x23 | no |
| 0x27 | Always 0 | | no |
| 0x28 | Bit 0 of lot `data`'s flags: visited. 1 to 0x1B, otherwise 0 | | no |
| 0x29 | Whether goal entry `data` of kind 4 is set | 0x3C | no |
| 0x2A | Column `data` of the Ingredient row with the ID in my temp 0; see below | 0x5D | no |
| 0x2B | 9, if the stack object is a Sim with a description | 0x15, 0x1C | no |
| 0x2C | 0, with the same tests | 0x15, 0x1C | no |
| 0x2D | Tree constant: temp 0 is the table ID as a whole, `data` the index; index 0x7F means temp 1 holds it | 0x2F | no |
| 0x2E | Variable `data` of the family of the Neighbor whose ID is the stack object; 0 if there's none | 0x38 | yes |
| 0x2F | Stack object's action queue: data 0 and 2 the stack object ID of the action, 1 and 3 its tree table index. Action number is `data` / 2, and -1 if it isn't there | 0x1C, 7 | no |
| 0x30 | My raw person data word `data`, below 0x24 | 0x1C, 0x21 | yes |
| 0x31 | My raw person data word, numbered by my temp `data` | 0x1C, 2, 0x21 | yes |
| 0x32 | Stack object's raw person data word | 0x21, 0x15, 0x1C | yes |
| 0x33 | Stack object's, numbered by my temp `data` | 2, 0x21, 0x15, 0x1C | yes |
| 0x34, 0x3A | My Neighbor data word `data`, below 0x31 | 0x1C, 0x21 | yes |
| 0x35, 0x3B | The same, numbered by my temp `data` | 2, 0x1C, 0x21 | yes |
| 0x36, 0x3C | Stack object's Neighbor data word | 0x21, 0x15, 0x1C | yes |
| 0x37, 0x3D | The same, numbered by my temp `data` | 2, 0x21, 0x15, 0x1C | yes |
| 0x38, 0x3E | Data word of the Neighbor whose ID is the stack object | 0x21, 0x38 | yes |
| 0x39, 0x3F | The same, numbered by my temp `data` | 2, 0x21, 0x38 | yes |
| 0x40 | My static attribute | 6 | yes |
| 0x41 | Stack object's static attribute | 6 | yes |
| 0x42 | Column `data` of the InventoryTokens row with the ID in my temp 0; see below | 0x5D | no |
| 0x43 | Column `data` of the plan with the ID in my temp 0; see below | 0x5D | no |
| 0x44 | Lot transition in my temp 0, 1 to 0x47: data 0 bit 0 of its flags, 1 bits 0 and 1 both set, 2 bit 2 | | no |

### Tree constants (scopes 0x1A and 0x2D)

**Proven**, by `tools/bhav.py` on every operand, and from `XObjLang::GetConstantsID` and `QuickDataBehaviorConstants` in the Wii build.

Scope 0x1A's data word packs three things. The PS2 function at `0x1E4D90` has the same logic:

| Bits | Meaning |
|---|---|
| 15 to 13 | Where: 0 private, 1 semi-global, 2 global. Anything else is error 0x2F |
| 12 to 7 | Table number. It's added to 0x1000, 0x2000 or 0x100 to give the table ID |
| 6 to 0 | Index in the table. 0x7F means temp 0 holds it |

The table ID goes through the same ranges as a tree ID, so private means the running object's `ResFile` row, semi-global its linked row, and global the row named `global`. `Behavior::GetResFileID` picks. Note that a private constant used in a semi-global tree belongs to the object running it.

**Where they are:** the `ResFile` table in `SimsObjects` quickdata, not `EDITHTRE`. Row `+0x20` is a `VECTOR<BehaviorConstants>` of 16-byte entries, sorted by ID:

| Offset | Type | Meaning |
|---|---|---|
| 0 | u32 | Table ID in the low 16 bits |
| 4 | `VECTOR<BehaviorConstantValue>` | The values |
| 8 | pointer | Title, like `Tuning - Food Points` |
| 12 | u8 | 1 if it's a tuning table (`IsTuning`) |

A `BehaviorConstantValue` is 8 bytes: an s16 value, 2 bytes of 0xFA padding, and a pointer to the value's label. `tools/bhav.py` has `read_resfiles`, which returns all of this.

An index at or past the end is error 0x2F, and so is a table that doesn't exist. There are 1,450 tables and 13,911 values. Of the 6,346 scope 0x1A operands in expressions, 6,012 are found in every context their tree can run in, 232 in some, and 102 in none. With private and semi-global swapped, the control, only 1,375 of 3,877 are found. The 102 are in objects this game cut, like `npc_grimreaper`, `npc_repoman` and `car_sg`, whose tables were pruned while their trees stayed, plus `counter_sg` and a few in `bed_sg`, `door_sg`, `food_stove_sg`, `scenario_sg`, `roof_sg`, `wall_sg` and `window_sg`. Reaching one stops the tree with error 0x2F, so a port should do the same.

The limits on scopes 9 and 0x19 are real: `InterpValue` tests them, and past the end is error 8. The disc has 51 local and 2 parameter operands past their tree's counts, 44 of them in `generator_visit/Load - Place Family`. If one is ever reached, the tree stops there.

Person data fields (0x12, 0x13, 0x1E, 0x1F) are split between two arrays. `cXPerson::TranslasteToNewPersonDataIndex` (a table at `0x492350`) gives the index in the Sim's own array at `+0x548`; if that's -1, the table at `0x4925F0` gives the index in the Neighbor's array at `+0x6C`. Neither is error 0x21.

A motive is a float at Sim `+0x180`. The s16 is `x < 0 ? x - 0.5 : x + 0.5`, cut to an integer.

Room data (0x17). Room 0xFFFB, a diagonal tile holding two rooms, gives 0, 1, 0, 0 for data 0 to 3, because it has no `Room` to look up. An outdoor tile is an ordinary room: see Rooms.

| Data | Value |
|---|---|
| 0 | Ambient light x 100 |
| 1 | 1 if outside |
| 2 | 1 (the room exists) |
| 3 | Area |
| 4 | Environment score: the room's score, or the manager's outside score if the room is outside |

Ingredient columns (0x2A, a 0x44-byte row): 0 `+2`, 1 `+0x38`, 2 `+0xC`, 3 `+0x10`, 4 `+0x14`, 5 `+0x18`, 6 `+0x1C`. Above 6 gives 0.

Token columns (0x42, a 0x24-byte row, ID at `+0xC`): 0 `+0x1C`, 1 `+0x20`, 2 `+0x10`, 3 `+0x1E`, 4 `+0x22`, 5 `+0x14`, 6 always 0, 7 `+0xE`, 8 `+4`, 9 `+6`, 10 `+0`, 11 `+2`, 12 the token's index in the tribe inventory or -1, 13 `+0x18`, 14 `+0x1A`, 15 the harvest tool, 16 whether it's ever been collected. Above 16 gives 0.

Plan columns (0x43): 0 unlocked, 1 `+2`, 2 `+8`, 3 `+4`, 4 `+6`, 5 put in hand, 6 put on ground, 7 put in inventory, 8 add token to inventory, 9 construction motion, 10 the tool resolved to a token, 11 resource pile, 12 tool used, 13 time to make, 14 to 22 the first three build motives as motive, minimum, maximum, 23 skill to improve, 24 `+0x5E`. Above 24 gives 0.

The stack object is the frame's object ID. A call passes it on to the called tree; `RunCheckTree` takes it as a parameter.

## Other primitives' operands

**Likely.** The Wii build keeps small accessor functions for each primitive's operands, and they give the fields of most primitives. These are the ones a port needs soonest; bit numbers count from 0 at the low end. Offsets are within the 8 operand bytes.

| Primitive | Fields |
|---|---|
| Animate (Sim) | `+3` bit 7 mirrored; `+5` bit 1 backwards, bit 2 uses stack variable, bit 5 use local for events, bit 7 preload only |
| AnimateObject | `+0` flags: bit 0 use stack object, 1 ignore anim tags, 2 preload only, 3 sync object location, 4 object anim table, 5 run immediately; `+1` anim tag target; `+2` state owner; `+4` animation index; `+6` sync object location |
| Budget | `+4` bit 0 just test, bit 1 clear means subtract |
| CallNamedTree | See its own section |
| CreateObject | See its own section |
| Dialog | `+0`, `+3`, `+4` button strings, `+2` message, `+6` title; `+1` icon index; `+5` bits 0-3 the type; `+7` bits 1-3 icon type, bits 4-6 result temp, bit 7 semi-global strings. See its own section |
| DirectionTo, DistanceTo | A flag byte (`+4`, `+2`), bit 0 set: the "from" value is the scope in the next byte and the data after it. Clear: scope 3, data 0xB. DistanceTo bit 1 returns sixteenths of a tile |
| FindGoodLocation | `+2` flags: bit 0 start at relative object, 1 not prefer empty tiles, 2 editable tiles only (tile flag 0x20 clear, as `MayEditTile` in world.md), 3 orientation in temp 0, 4 prefer under roof |
| Grab | `+0` bit 0 parenting prop bone |
| IdleAnimate | `+2` low byte decrement parameter, high byte sync object; `+5` flags; `+6` behaviour; `+7` literal loop count |
| KillObject | `+2` bit 0 return immediately, bit 1 clear means clean up all |
| PlaySound | **Proven, read line by line on PS2 (`0x234CF8`), not matched.** `+0` s16 the sound, or temp 0 if `+4` bit 7. Source: me if `+4` is 0xFF, else the stack object if bit 1, else me. Bit 0, looped, isn't read on PS2. The sound's `SoundInfo` is looked up in my tree's namespace, then the global one. Found, with a sound event ID (`+0x4C`) and the player ready, it queues a play of that event from the source at volume -400. **It returns 1 on every path, with no error**, found or not |
| PushAction | See its own section |
| Relationship | Flag byte `+4`: bit 0 clear means create, bit 1 use neighbours |
| SetBalloon | `+0` bit 0 over stack object; `+2` index, group; `+6` type, then flags |
| SetMotiveDelta | `+3`: bit 0 clear all, bit 1 influence on me, bit 2 influence on stack object |
| SetToNext | `+4` low 4 bits search type, bit 5 GUID from temps, bit 6 skip non-leads; bit 7 means `+5` and `+7` give target owner and data, otherwise it's the stack object |
| Snap | `+4` bit 0 origin only, 1 ask person to move, 2 use footprint |
| TestObjectType | Read from the PS2 code. `+0` s32 GUID; `+4` s16 data and `+6` u8 scope give the object to test, a missing object being error 0x17; `+7` flags. Bit 2: true if its GUID is the one in temps 1 and 2 (high, low). Else bit 0: true if the GUID's selector has the same semi-global namespace. Else bit 1: store the GUID in temps 1 and 2, true. Else: true if its GUID or its master selector's GUID is the one given. An object with no selector is false |
| TreeBreak | `+4` bit 0 log only, bit 1 break all |

The full list is `*Param` in the Wii symbols. `tools/bhav.py` doesn't test any of this table.

`cXObject::TryTestObjectType` matches the game, so its row is proven.

## SetToNext (opcode 0x1F)

**Proven**: `TrySetToNext` matches the game byte for byte, so this section is what the code does. 1,248 uses.

It steps a value through a list of things. The value is the target; `+4` is the flags.

| Operand | Meaning |
|---|---|
| `+0` s32 | GUID for search types 4 and 7 |
| `+4` bits 0 to 3 | Search type |
| `+4` bit 5 | Take the GUID from temps 1 and 2 (high, low) |
| `+4` bit 6 | Skip non-leads. See "Skipping" below |
| `+4` bit 7 | Target given: `+5` is its scope and `+7` its data. Clear means scope 0xA data 0, the stack object |
| `+6` u8 | For search type 9, the local holding the centre object |

The target is read first, and it must have an address or it's error 0x13. Returning 1 takes the true branch. In each type "writes" means through `InterpWriteValue`. Object IDs and list positions start from 1, and 0 means none.

| Type | Steps through | At the end |
|---|---|---|
| 0, 2, 6, 12 | Objects in their linked list, from the first if the target is 0, otherwise from the object after it (object `+0xB8`; the head is ObjectModule `+0x28`). Type 2 skips Sims. Type 6 wants variable 0x3B equal to this tree's parameter 0. Type 12 wants objects in the world, which means both tile coordinates above 0 | False, target unchanged. An object that no longer exists is false |
| 1 | Sims in ObjectModule's list at `+0x2050`: the one after the target, or the first if it's 0 | Writes 0. Returns target != 0 |
| 3 | Parts of the multi-tile object holding the target. Not multi-tile: writes the same ID, true. Otherwise child (`+0x13C`) then sibling (`+0x140`) | Writes 0, false. Going all the way round with skipping on is false |
| 4 | Objects by position (ObjectModule `+0x2088`, then 16 special slots) with the GUID's selector, from the position after the target | Writes 0. Returns target != 0 |
| 5 | Neighbour IDs: the next with an entry after the target | Writes it, 0 at the end. Returns target != 0 |
| 7 | Neighbours, as 5, until one has the GUID, or until temps 1 and 2 equal the GUID when bit 5 is set | False |
| 8 | Objects on the target's tile, wrapping. A target of 0 is false, a missing object is error 0x33, and one not in the world is false. If the target isn't found on its own tile, false. Going round to the target with skipping on is false | False |
| 9 | The four tiles around the object in local `+6`, in the order (1,0), (0,1), (-1,0), (0,-1). It carries on from the side the target is on; a target of 0 starts at (1,0) | False after (0,-1) |
| 10 | Nothing | False |
| 11 | Lots in the neighbourhood list at `+0xA0`: the one after the target, wrapping to the first; the first if it's 0 | False if the target isn't in the list |
| Others | | Error 0x1E, with skipping on or off |

Search type 9's object has to be in the same room (object variable 0x1D) as the centre object, and off-grid tiles are skipped. A missing centre object is error 0xA, and a local past the tree's count is error 8.

**Skipping.** Proven: both builds test the flag in the same seven places, and the match pins the PS2 one. An earlier version of this page said only 0, 2 and 6 allow it. That rule is only about the list walk, and each other type has its own test:

| Type | With skipping on |
|---|---|
| 0, 2, 6 | Passes over objects where `IsInteractionTile(true)` is false |
| 1, 5, 7, 10, 11 | The flag is ignored |
| 3 | Passes over parts where `IsInteractionTile(false)` is false, the only type that doesn't require the object to be in the world. Coming back to the start is false, with no write |
| 4 | An object also needs `IsInteractionTile(true)` |
| 8 | Passes over objects where `IsInteractionTile(true)` is false. Coming back to the target is false, with no write |
| 9 | An object also needs `IsInteractionTile(true)` |
| 12 | Turned off before anything else |

`IsInteractionTile(inWorld)` asks whether a tile of an object counts as the object. In order:

1. If `inWorld` is set and the object isn't in the world: false.
2. `ObjDefinition +0x8A` is 2: false.
3. It's a Sim: true.
4. It's single-tile: true.
5. `ObjDefinition +0x10` is negative: true.
6. `ObjDefinition +0x8A` is 1: false.
7. True if it has a tree for entry point 8.

Only the `IsInteractionTile` body is Wii-only; the object's tile is the same data on both.

## CreateObject (opcode 0x2A)

**Likely**, read from the Wii build. 401 uses. It isn't matched, and the PS2 function (`0x22AA30`) hasn't been compared line by line.

| Operand | Meaning |
|---|---|
| `+0` u32 | GUID |
| `+4` s8 | Where to put it, below |
| `+5` bit 0 | No duplicate: false if the tile already has an object of this type |
| `+5` bit 1 | Pass my ID as the new object's parameter 0, and my stack object as its stack object |
| `+5` bit 2 | Make the Neighbor whose ID is my stack object, not a GUID |
| `+5` bit 3 | Empty tiles only: false if the tile has any object |
| `+5` bit 4 | Pass my temp 0 as its parameter 0. With bit 1 as well, error 0x37 |
| `+5` bit 5 | Face the way my stack object faces |
| `+5` bit 6 | Take the GUID from temps 1 and 2 (high, low) |
| `+6` s8 | For places 8 and 9, a local |

| Place | Tile |
|---|---|
| 0 | One tile ahead of me |
| 1, 2, 5 | My tile. 1 and 2 let it overlap me |
| 3 | One tile ahead of my stack object, in its room. No stack object is false |
| 4 | My stack object's tile, in its room. No stack object is false |
| 6 | Out of the world, at (-1, -1) |
| 7 | The tile of the object whose ID is parameter 0. No parameters is error 8, no object is false |
| 8 | The tile of the object whose ID is in local `+6`. A bad local is error 8, no object is false |
| 9 | Next to me, in the direction of my facing plus local `+6`, and not through a wall |

In order:

1. A GUID of 0 with no flag saying where else to get one is error 0xC. An unknown GUID is false.
2. If the object's data isn't loaded, it returns 2 and waits, but only during the main simulation.
3. It works out the tile, then tests no-duplicate and empty-tiles.
4. Unless the place is 6, it's false if the tile is out of bounds or marked not routable, or in the wrong room when a room is set.
5. `MakeNewOutOfWorldObject` builds it, with each multi-tile part. `AddObject` puts it at the front of the object list.
6. Facing is copied by turning up to four times.
7. Unless the place is 6, it tests `CanPlace` and then `Place`. If it can't be placed, a Sim tries `TryFindSafeLocForSim`, and anything else is killed and it's false.
8. A Neighbor who's a visitor runs its `init visitor` tree.
9. The parameters from `+5` go into the new object's first stack frame.
10. **My stack object becomes the new object's ID**, and it returns 1.

## Animate (opcode 0x2C)

3,953 uses. On a Sim, `cXPerson::TryElement` sends behaviours 7 and 8 to the object wrapper and everything else to `SAnimator2::TryAnimate` (`0x104050`) through the animator at Sim `+0x5F0`. **Proven** for that function, which byte-matches. What it calls is **likely**, read from the Wii build.

| Operand | Meaning |
|---|---|
| `+0` s16 | Animation. 0 turns the current one off |
| `+2` u8 | The local that events go to, with `+5` bit 5 |
| `+3` | Bit 7 mirrored. Bits 0 to 6: behaviour number minus 3 |
| `+5` bit 0 | Behaviour 1, when bits 0 to 6 of `+3` are 0 |
| `+5` bit 1 | Backwards |
| `+5` bit 2 | The animation is in parameter 0, not `+0` |
| `+5` bit 4 | Behaviour 2, or with bit 0 behaviour 3 |
| `+5` bit 5 | Events go to the local in `+2`, not parameter 0 |
| `+5` bit 7 | Preload only |
| `+7` s8 | Sync object, checked before the animation starts |

| Behaviour | Path |
|---|---|
| 0, 3, 6 and up | Normal. 3 passes a flag to the start of the animation; 6 picks an algorithmic idle |
| 1 | `BackgroundBehavior` |
| 2, 5 | `CarryBehavior`, a virtual function |
| 4 | `FXAnimationBehavior` |
| 7, 8 | The object wrapper: 7 me, 8 the stack object |

The primitive runs again each tick until it returns something other than 2. In order:

1. The Sim's model isn't loaded: return 2.
2. An animation is still loading: return 2.
3. Preload only, `+5` bit 7: the animation is resolved, its events and resource are asked to load, and that's all. It returns 1 if `EResourceManager::PreloadResource` says it's there and 0 if not, and a failed lookup returns 1. Nothing plays, and it doesn't wait. **Likely**, Wii, and it's `0x120D60` on PS2.
4. The Sim has a queued operation: return 2.
5. An animation of 0 turns the current one off.
6. If this animation hasn't started, resolve and start it. A sync object that isn't ready gives its own return value.
7. If an animation event is waiting, store its number in the event target and **return 0**, the false branch.
8. Otherwise, if the animation has finished, deliver any end event the same way, or stop and **return 1**, the true branch.
9. Otherwise **return 2** and wait.

**Behaviour 4, an overlay ("effect") animation.** `SAnimator2::FXAnimationBehavior` (PS2 `0x10CCF8`). Read on Wii, where it has names, and likely.
- **Operands:** `+3` = 1 picks it.
- **What it does:** it plays the clip on a separate animation track (`EAnimController` track flag `0x8000`), on top of whatever the Sim's main animation is doing. It never waits for the clip and never delivers events.
- **Animation 0:**
  - sets that track's intensity to the constant at Wii `lbl_805D9238` (likely 0, fading it out);
  - finishes any events still owed;
  - **returns 1**.
- **Another animation:**
  1. resolves the clip as above, and loads it. A lookup or load that isn't done returns its own value, so 2 while loading, or the error;
  2. starts it on the overlay track at the speed the Sim uses (negative with `+5` bit 1);
  3. **returns 1** at once.
- **Example: `Motive Check - Hygiene` (`person_sg` 0x201C).**
  - Node 4 preloads animation 1 (`+5` bit 7). Node 6 starts it as an overlay (`01 00 00 01 00 20 00 00`). **Node 20 turns it off** (`00 00 00 01 00 20 00 00`): animation 0, behaviour 4, both branches 254.
  - For a castaway with no action, kind 0 resolves animation 1 to `template_person`'s list 0x81 entry 1, **`a_idle_lowhygiene`**.
- **For a port:** return 1. Start or stop an overlay if it can play one, and leave the Sim's main animation alone.

So a tree animates in a loop: the false branch handles an event, then comes back to the same node, and the true branch means it finished. With no events to deliver, one play is many ticks of 2 and then a 1. A port without real animation can return 1 at once, or after the animation's length, and trees that wait for events will just see none. The event target is `+2`'s local or parameter 0, and a tree with neither is error 8. End events are negative in the animator (`+0x194`), and with `+5` bit 1 set they're stored as `-1 - value`.

**Which clip Animate's number means. Proven, read on PS2** (`resolveSkillForPrimitive`, `0x1038D8`, and what it calls). Operand `+4`, read as a signed byte, picks the table:

| `+4` | Table |
|---|---|
| 0 | A list of the **controlling object**: the object of the Sim's newest action (`cXPerson::GetControllingObject`, `0x1DD8E8`), or the Sim itself with none. With `+5` bit 2 the list is the stack object's, and the number is `GetParams()[+0]` |
| 1 | **The global row's list `0x80`** (holder `+0x18`, `0x1E1520`; Wii `GetGlobalAnimRef`). 62 uses: listening, watching, reacting to art |
| 2 | **The global row's list `0x82`** (holder `+0x14`, `0x1E1498`; Wii `GetPersonStockAnimRef`). 1 use, in `car_sg` |
| 3 | A list by Sim type on the same holder (`+0x1C` plus 4 x type, `0x1E15A8`) |

**The holder. Proven, PS2** (`0x1C00C8` on, Wii `InitSkillLookup`). It's `GlobalSkillTables` at `0x492410`. Each table is the global object's row's animation list with that set ID: `+0` `0x96`, `+4` `0x98`, `+8` `0x99`, `+0xC` `0xA4`, `+0x10` `0xA5`, `+0x14` `0x82`, `+0x18` `0x80`, then `+0x1C` six lists by Sim type (`0x9C` plus the type). This corrects the earlier reading of kinds 1 and 2, which had the two lists the other way round and called them unread. Neither kind is on the coconut's or the palm's trees: Eat, Identify (`Interaction - Taste`) and Harvest use kinds 0 and 3 only.
| Others | No clip, and the primitive returns 1 |

The lists are the `ResFile` row's `+4` vector. Its entries are 8 bytes: a vector of pointers to `AnimRefName` rows, each of which points at an `AnimRef` row, whose word is the clip's name hash, and an s16 set ID. For kind 0 the set is `0x81` plus the **Sim type** (see below), looked up on the object's own row and then its semi-global row (`0x19EFD8`, `0x19F0A8`); for kind 3 it is the global row's `0x9C` plus the code. A code of 4 falls back to 3, and an empty entry in the first list falls through to the next. So `Interaction - Eat` on a coconut plays `resources_sg`'s list `0x81`: 3 `a2o-peelfruit`, 9 `a2o-eatfruit-small`, 0xA to 0xC the large-fruit clips; and `Reach - Grab`'s `Animate` with `+4` of 3 and parameter 0 set to 10 plays `a2o_reach_floorht` from the global list `0x9C`. A clip the Sim's kind can't play (none found) is error 0x42.

**The "body code" is the Sim type. Proven, PS2.** Both the kind 0 and kind 3 lookups call `Neighbor::GetSimType` (`0x2513A8`: `lh $v0, 0xC4($a0)` on the Sim's `Neighbor`), which is the neighbour's **person data 0x2C**, the same field as Wii's `Neighbor::GetSimType`. It isn't an `ESim` field, as this section used to say. Wii's tests give the values: 2 to 4 are pets, 3 and 4 dogs. The neighbourhood on the disc has types 0 (`pizzadude`), 1 (`officer`, `repoman`, `firefighter`, `npc_paperdelivery`) and 5 (`maid`, `thief`). The castaways are made at run time, so their type isn't on the disc: **likely** 0, an ordinary adult.

**Events are the clip's notes.** An `ANIM` file ends in notes, each a time in milliseconds, a kind and a string (`docs/ASSET_FORMATS.md`). Kind 6 notes are what the tree gets: the string is the event number. `a2o_reach_floorht` has 101, 104 to 108, then 102 at 1,200 ms and 103 at 2,250 ms, and `Reach - Grab` does its `Grab` when the event in its local equals 102, the number it chose for a floor-height reach. `a2o-eatfruit-small` sends 501 at 900 ms, the bite. The 80-byte event records after the notes are empty on every clip on the disc. Kind 1 notes are footsteps (`step`, `scuf`) and kind 5 sounds.

On an object that isn't a Sim, `cXObject::TryAnimate` (`0x234558`) is all there is: behaviour 7 uses me, 8 the stack object, and anything else is error 5. It resolves a skill for passive influence and plays nothing. It matches the game.

## AnimateObject (opcode 0x37)

**Proven for the primitive: read line by line on PS2 (`0x231C40`, 244 words) and the same as Wii step for step. Not matched.** What the object model does with it, `ISimsObjectModel::TryAnimateObject` (PS2 `0x332840`), is **likely**: read on Wii only. 250 uses. It animates an object that isn't a Sim, through the object's model.

| Operand | Meaning |
|---|---|
| `+0` bit 0 | The stack object animates, not me. Missing is error 0x15 |
| `+0` bit 1 | Ignore animation events |
| `+0` bit 2 | Preload only |
| `+0` bit 3 | Set a sync object, with preload only |
| `+0` bit 4 | `+4` is an index into the object's animation table |
| `+0` bit 5 | Run immediately |
| `+1` u8 | The local that events go to |
| `+2`, `+3` | Otherwise the animation is an operand: scope `+2`, data `+3` |
| `+4` u16 | The table index, or temp 0 if it's 0 |
| `+6` u8 | The sync object, an operand |

1. The object's model is its `ISimsObjectModel`. An object without one is error 0x15.
2. **The animation:**
   - With `+0` bit 4, it's entry `+4` (or temp 0) of the model's animation table, and object data 0x23 = that index. An entry that doesn't exist is error 0x42.
   - Otherwise it's the operand at `+2`/`+3` (an `InterpValue` error stops the tree), and data 0x23 = 0.
3. **Object data 0x38 holds the animation flags.**
   - Bit 1 = run immediately.
   - Bit 0 = table animation.
   - Unless it runs immediately, and only if bit 3 isn't set yet, bits 2 and 3 are set as well.
4. **Preload only:**
   - With bit 3, the sync object is resolved from `+6` and stored at object `+0x124` (`+0x12C` = 1, `+0x128` = 0). The stack object itself, or none, clears all three.
   - Without bit 3, my `+0x124` is cleared.
   - Then the model preloads the animation, and its result is the primitive's.
5. **Otherwise:** the model's `TryAnimateObject(animation, event target)`, and its result is the primitive's. The event target is local `+1`, or none with `+0` bit 1.

**The model's part, likely, from Wii:**

1. If a load is pending for another animation, it's dropped. If it's pending for this one and not loaded yet: return 2.
2. The first time, object data 0 = the animation (unless it's a table animation), and the model marks it started.
3. **Events:** with an event target, a queued event goes into the local and it **returns 0**, the false branch, clearing flag bit 3. With none queued the local gets -1. Without a target, a queued event is just dropped.
4. **Waiting:** unless it runs immediately, it waits while the animation plays.
   - A loaded animation shorter than the time so far is done.
   - Otherwise it **returns 2**, wait.
   - An animation that isn't loaded starts loading and returns 2.
5. **Done:** clear started and flag bit 3, **return 1**, the true branch.

**For a port:** return 1 at once for "run immediately", or after the animation's length otherwise. Write -1 into the event local, since no events are delivered. Error 0x15 for a missing stack object or a missing model, and 0x42 for a table index the object doesn't have.

## Idle (opcode 0)

**Proven:** `cXObject::TryIdle` byte-matches. 3 uses on the disc.

`+0` s16 is a parameter number, and that parameter is the counter. Past the tree's parameters is error 8.

1. If the object's idle status is 0, the counter is set to 0 and the status to -1.
2. A negative counter becomes 0.
3. A counter of 0 returns 1, the true branch.
4. Otherwise it returns 2, so the node runs again next tick. Normally the counter goes down by 1. If the object can sleep, the counter stays and the status becomes counter - 1 instead.

An object can sleep if it isn't a Sim, its variable 0x19 is 0, and no sprite slot is active. Idle status is a per-object word in `ObjectModule`, and it's what reading variable 0x19 clears, which ends the sleep at step 1. A port can ignore sleeping and always count down: the tick count comes out the same unless something wakes the object early.

## IdleAnimate (opcode 0x40)

**Proven, PS2:** read from `cXPerson::TryIdleAnimate` (`0x1AF460`, Wii name), not matched. 50 uses. Sims only.

It plays an idle clip on a loop and counts loops down, then takes the true branch. Operands:

| Operand | Meaning |
|---|---|
| `+0` | Clip number, resolved like Animate's |
| `+2` | Low byte: the parameter that holds the loop count. High byte: a sync object |
| `+5` | Bit 0 backwards, 1 the clip number is in parameter 0, 2 algorithmic idle selection, 3 idle overlays, 4 the loop count is the literal `+7`, bits 5 to 7 the clip table kind (as Animate's `+4`) |
| `+6` | Behaviour: 0, 1 or 2 (below) |
| `+7` | Literal loop count, when `+5` bit 4 is set |

Each tick:

1. The Sim's model isn't loaded: return 2 (run again next tick).
2. The player is steering this Sim directly (the animator's player control is on, it's the selected Sim and the direct interactor's status is 1): return 1.
3. Call the animator's `TryIdleAnimate` (vtable `+0x50`, slot 9, `SAnimator2::TryIdleAnimate`, `0x103AC8`). It resolves the clip with Animate's tables, or picks an algorithmic idle, loads it, waits for the sync object if there is one, and starts it looping. It returns 2 while loading and 1 once started, and **delivers no events**. A 2 or -1 from it is returned as it is.
4. Behaviour 0: return 1. The clip is left looping and the tree moves on.
5. The loop counter is the Sim's word `+0x5F4` (Wii `+0x5E0`); -0x8000 means unset. When unset it's loaded from `+7` if `+5` bit 4 is set, otherwise from the parameter `+2` names. That parameter number must be below the frame's parameter count, and at least 1 when `+5` bit 1 is set (parameter 0 then holds the clip), or it's error 8.
6. Counter 0: clear bit 6 (0x40) of my variable 8, set the counter back to -0x8000, return 1.
7. Behaviour 1: the counter goes down by 1 every tick; return 2. Behaviour 2: it goes down by 1 each time the clip's track completes, and the track is restarted; return 2.

So for a port: start the idle clip looping; behaviour 0 is true at once, behaviour 1 waits the count in **ticks**, behaviour 2 waits the count in **completed loops**. NotifyStackObject's case 0x40 sets `+0x5F4` to 0, which ends a running IdleAnimate on its next tick.

## Random (opcode 8)

**Proven:** `cXObject::TryRandom` byte-matches.

| Operand | Meaning |
|---|---|
| `+0` s16, `+2` s16 | Target data and scope |
| `+4` s16, `+6` s16 | Limit data and scope |

A limit of 0 is error 4. The result is a random number modulo the limit, taken as unsigned, so a negative limit is a very large one. It's written through `InterpWriteValue`, or to the float for a motive, and with neither address it's error 0x13. It returns 1.

The generator is at `0x262AF0`: `state = state * 0x41C64E6D + 0x3039`, returning bits 16 to 47 of the 64-bit result of that multiply and add. State is 32 bits, at `0x5112E8`.

## CheckObject (opcode 0x35)

**Proven**: `TryCheckObject` matches the game byte for byte.

`+0` data and `+2` scope give an object ID, and `+4` s16 is the test. Reading the value can fail with the usual errors. Any other test is false.

| Test | True when |
|---|---|
| 0 | The object is the player's Sim and it has a player interactor that isn't autonomous |
| 1 | The object exists |
| 2 | The object exists and is a Sim |
| 3 | A Neighbor has that ID |
| 4 | Always true. Temp 0 is a relationship type, 1 to 9, or error 0x14. It's replaced by how many relationships of that type the Sim has, or 0 |
| 5 | The object is the player's Sim and it has no player interactor |

## Master selectors

**Proven.** A multi-tile object has a definition for each tile and one more, its master, which stands for the whole object. `ObjSelector::GetMasterSelector` (`0x1D9E48`) caches the answer at selector `+0x90`, and `ObjectFolder::GetMasterSelector` (`0x254718`) finds it:

| Definition word | Meaning |
|---|---|
| `+0x14` s16 | Which multi-tile object in this file. 0 is a single tile |
| `+0x16` s16 | -1 on the master. On a part, its place: **x in the high byte (signed), y in the low byte**. Proven, see below. (This row used to have them the other way round.) |
| `+0x1C` u32 | GUID. The master's GUID is just the master definition's GUID |

1. A definition with `+0x14` of 0 is its own master.
2. Otherwise it walks every selector for one with the same file (selector `+4`, the namespace ID), the same `+0x14`, and `+0x16` of -1.
3. If none is found, it's its own master.

**Which byte is x. Proven, PS2.** A part's tile in its object is x in the high byte of `+0x16` and y in the low byte, in the game's axes (x is the first word of a tile point, see "Which way is x" under Routing). The chain:

1. **`ObjDefinition::GetMultiTileOffsets` (`0x253278`)** gives the high byte, sign-extended, as its first result and the low byte, unsigned, as its second.
2. **`cXMTObject::Initialize` (`0x250168`)** stores those at `+0x144` and `+0x148`, and definition `+0x70` (the level) at `+0x14C`.
3. **`AssignOffsets`**, which matches, subtracts the first part's `+0x144` and `+0x148` from each part's, and turns the result by the first part's direction into `+0x150` and `+0x154`.
4. **`cXMTObject::Place` (`0x2000A0`)** builds a tile point from `+0x150` and `+0x154` through `0x175800`, which stores its first argument × 16 as x and its second × 16 as y. It then moves each part by it.

So the high byte ends up in x. Wii's `ObjDefinition::GetSubIndex(a, b)`, `(a << 8) | (b & 0xFF)`, packs x first, y second. On a part whose object faces direction 0, the part's tile is the first part's plus (high byte, low byte) whole tiles; other directions turn that offset as `AssignOffsets` does.

So there's no stored master GUID to decode. **On the disc:** 509 definitions are single tiles and 826 are in 131 multi-tile objects, and every one of the 131 has exactly one master. `+0x10` is the control, and gives 123 with one, 6 with none and 2 with two. `tools/bhav.py` reruns this, and `read_masters` returns the table from GUID to master GUID.

`+0x10` is a different thing: `GetInteractionLeader` looks for the part whose `+0x10` is the negative of this part's, and a part with `+0x10` of 0 or less is its own leader.

`TestObjectType`, with none of its flag bits set, is true if the object's GUID or its master's GUID equals the operand. The other primitives that ask for a master are SetBalloon and GenericSimCall calls 0x39 and 0x3C.

## NotifyStackObject (opcode 0x31)

**Proven**: `TryNotifyStackObject` matches the game byte for byte. 236 uses. No operands.

It wakes the stack object if its main tree is waiting. A missing stack object is error 0x15. It takes the stack object's main frame and reads the node that frame is on. No frame, or no such node, is just true. Otherwise, by the node's primitive:

| Primitive | What it does |
|---|---|
| 0, Idle | Sets the parameter that node counts down in to 0, and clears the object's idle status. A parameter number past the frame's count is error 8 |
| 0x11, IdleForInput | Sets that parameter to 0 |
| 0x40 | Sets the Sim's idle loop count, `+0x5F4`, to 0. Not a Sim is error 0x1C |
| Others | Nothing |

It always returns 1, the true branch, unless there's an error.

## KillObject (opcode 0x12)

**Proven**: `TryKillObject` matches the game byte for byte. 328 uses.

| Operand | Meaning |
|---|---|
| `+0` s16 | 0 me, 1 the stack object, 2 nobody. Anything else is error 9 |
| `+2` bit 0 | Return at once, even if I'm killing myself |
| `+2` bit 1 | Clear means clean up everything |

1. If frame `+8` is 1, this node already ran: return 1.
2. Find the object. No object is **false**, and so is `+0` of 2, since ID 0 isn't an object.
3. `AddToKillQueue` queues it. A Sim's data is saved first through `SavePersistentData`.
4. If I killed myself in the main simulation, and bit 0 is clear, frame `+8` becomes 1 and it returns 2, so the tree stops here. Otherwise **true**.

### What happens to a killed object

The primitive only queues. The object goes when the queue is flushed.

**`ObjectModule::AddToKillQueue(id, cleanupAll)`**, PS2 `0x222A48`. **Proven:** read on PS2 and the same as Wii step for step.

1. No such object: nothing happens.
2. A part of a multi-tile object that isn't dynamic is swapped for the object's first part (PS2 `+0x140`), and that part's ID is queued instead.
3. It walks up the containers (object data 0xE and 0x1A on PS2, `GetContainer` on Wii). If any container is already queued, it stops: the container takes this object with it.
4. If the ID is already queued, it stops.
5. `Pickup()` (PS2 vtable `+0x128`): **the object leaves the world now**, not at the flush.
6. Misc flag 0x1000 (`+0xE4`) is set when `cleanupAll` is false and cleared when it's true.
7. The ID is pushed onto the s16 vector at ObjectModule `+0x2040` (Wii `+0x203C`).

**`KillQueueFlush`**, run by `ObjectModule::PostSim` (after each simulation step) and by `PostLoad`. **Proven on Wii, likely on PS2:** PS2's flush hasn't been located yet. It takes IDs from the front of the queue until it's empty. For each one that still exists, a dynamic multi-tile object is first removed from the dynamic list, then `ObjectModule::KillObject(id)` runs:

1. The object, and every part of a multi-tile one, is marked as being killed, and Sims inside each are taken out (`ExtractContainedSims`).
2. `Pickup()` again.
3. With certain definition flags (`+0xB6` bits 1 to 2) and a house loaded, and not while killing everything, the house is recalculated. Types 5, 8 and 0xE also update room styles and wall posts.
4. `KillOutOfWorldObject(id, false)`.
5. If object data 7 was non-zero, global event 0x8E is dispatched with the object's room.

**`KillOutOfWorldObject(id, partsOnly)`**, **proven on Wii, likely on PS2:**

1. If the definition's `+0x9C` is set, the object's instance is removed from the tree branch manager.
2. Every object forgets it (`ClearAwarenessOfObject`).
3. A multi-tile part, unless called for parts: if the object has a lead (`+0x13C`), the lead is killed instead. Otherwise every part gets misc flag 0x40, and each part in the chain at `+0x138` is killed with `partsOnly` true. Then it returns.
4. Otherwise: global event 0x83 with the ID, misc flag 0x40, and:
   - It stops being the tutorial object if it was.
   - If it's the selected Sim, the selection moves on to the next Sim, or to none.
5. `CleanupPeople(object)`, so every Sim drops it, then `object->Cleanup(0)`. That runs the object's **Cleanup tree (entry point 3)** on each part, clears data 0x3E, and resets each part's stack to its main tree.
6. **Everything in its object slots is killed too** (`KillObject` on each non-zero slot ID).
7. It's cut from its container (`HierSever`) and removed from the object list.
8. If the action menu had it selected, that selection is cleared.
9. The object is deleted (destructor, vtable `+0x18` on Wii).
10. Every remaining object's relationship matrix drops the ID.
11. Global event 0x84 with the ID.

**For a port:** queue in the primitive and pick the object up at once. Flush after the step: run Cleanup (entry point 3), kill slot contents, then remove the object and its relationships.

## CallFunctionalTree (opcode 0x14)

**Proven**: `TryCallFunctionalTree` (`0x22D358`) matches the game byte for byte. 79 uses. It runs one of the stack object's functional trees, an entry point, as a subroutine.

| Operand | Meaning |
|---|---|
| `+0` s16 | Which function, below |
| `+2` bit 0 | Change the icon: the last argument of `GosubObjectTree` |

| `+0` | 0 to 9 | 0xA | 0xB | 0xC | 0xD | 0xE | 0xF | 0x10 | Others |
|---|---|---|---|---|---|---|---|---|---|
| Entry point | 0x12 to 0x1B | 0xE | 0x1C | 0x10 | none, false | 0x1D | 0x20 | 0x21 | none, false |

1. The stack object must exist, or it's error 0x15.
2. Its tree for that entry point comes from its selector's function table.
3. **No tree is false**: it returns 0, and nothing runs.
4. The tree is pushed with 8 parameters: temps 0 to 3, then four zeros. Then it **returns 3**, so the pushed tree runs now. When it returns, the node takes the true or false branch from its result, as a call does (see "Running a tree").
5. If the push fails, it's false.

So this is how entry points 14, 16, 18 to 29, 32 and 33 are reached, which "Entry points" lists as having no caller by number.

## NotifyPlans (opcode 0x2F)

**Proven**: `TryNotifyPlans` (`0x23CFD8`) matches the game byte for byte. 60 uses. It tells the plan system that something happened to a Sim.

| Operand | Meaning |
|---|---|
| `+0` s8 | The notification. 0 is error 0x6A |
| `+1` u8, `+2` s16 | An operand, scope and data: the notification's value |
| `+4` bit 0 | The Sim is the object in temp 0, not me |

1. **The Sim:**
   - With `+4` bit 0, it's the object whose ID is in temp 0. A missing object, or one that isn't a Sim (type 2), is error 0x1C.
   - Otherwise it's me, as a Sim.
2. **No Sim (I'm not one) returns 1**, true, and nothing is sent.
3. The operand is read. An error there stops the tree with error 9.
4. A notification of 0 is error 0x6A.
5. `PlanManager::SendNotification(notification, value, Sim)`, then **true**. What the plans do with it is in `PlanManager`, which isn't read yet.

## FamilyInventory (opcode 0x46)

`TryFamilyInventory`, PS2 `0x23A9D0`, 2,020 instructions with the inventory calls inlined. Parked 2 words off with the logic complete (`cXObjectPrims.cpp`, under `NON_MATCHING`): the game copies the looked-up neighbour through a temporary, and nothing else differs.

- **Proven on PS2:** everything in this section. The 14 operations, where each error comes from, the operands read through `InterpValue`, and the detail of each operation, which was read on Wii first (`TryFamilyInventory__8cXObject` names everything it calls) and then written and compared with the PS2 code word for word.
- **Corrections to the Wii reading, from PS2:** the ownable Find and Remove pick the item with the most ownership points for the owner, not the first with the GUID; removing a token can unlock something; what a remove returns depends on the category (op 1); op 7 reads the owner before it checks the item.

| Operand | Meaning |
|---|---|
| `+0` s8 | Operation, 0 to 0xD below. Anything else does nothing and is true |
| `+1` bits 0 to 2 | Category: 0 object, 1 ownable, 2 token, 3 clothing |
| `+1` bit 5 | Remove an object by an object ID in `+2` |
| `+1` bit 6 | Whose inventory: the neighbour with the stack object's ID, not the stack object's Sim |
| `+1` bit 7 | `+2` is an item's place in the inventory (or an object ID, for an add), not a GUID |
| `+2`, `+3` u8 | Operands, in LotTransition's one-byte code: 0 my ID, 1 the stack object's ID, 2 to 9 temps 0 to 7, 0xA to 0x11 scope 0x19 0 to 7, 0x12 to 0x19 scope 9 0 to 7, anything else the number code − 0x7E |
| `+4` s32 | The literal: an object's or ownable's GUID, or a token's or clothing item's ID |

**Whose inventory:**

- With `+1` bit 6, it's the neighbour whose ID is the stack object's. None is error 0x38.
- Otherwise it's the stack object, which must exist (error 0x15) and be a Sim (error 0x1C), through its neighbour.
- Either way the inventory is the neighbour's family's.

The result is true unless an operation below says otherwise.

**A family holds at most 0x20 items that aren't tokens. Proven, PS2** (`InventoryItems::AddOwnableItem`, `0x1F9CB8`, matches). The count is the byte at the inventory's `+0`. An object or ownable add when it's full adds nothing and is false. A successful add gives the item an instance ID from a global counter, if it has none.

**What each add does. Proven, PS2** (`AddObjectItem` by GUID `0x1F9798` and by object `0x1F9908`, and `AddOwnableItem` by object `0x1F9CB8`, all three match; the ownable-by-GUID add is inlined in `0x23A9D0`):

| Add | Token GUID becomes a token | On-add tree | Add goals |
|---|---|---|---|
| Object by GUID (`+4`) | yes | no | yes |
| Object by object (bit 7) | yes | yes | yes |
| Ownable by GUID (`+4`) | no | no | no |
| Ownable by object (bit 7) | no | yes | no |

- **A token GUID:** before the limit is checked, the object's GUID is looked up in the first word of each `InventoryTokens` row. If a row matches, one of that row's token (ID at `+0xC`) is added instead, and the add is true even when the inventory is full. GUID 0 is never a token.
- **The on-add tree** runs with the family, the object and the new item's place (the last one). An add by GUID has no object, so it doesn't run.
- **The add goals** (`0x1F9AC0`, Wii `CheckForAddGoals`) get the GUID and 0. They run after the on-add tree.

**Tokens and clothing. Proven, PS2** (`AddTokenItem` `0x1F9608` and `AddClothingItem` `0x1F9DB8`, both match). Neither counts toward the 0x20 limit.

- **A token add** first sets the token's bit in the "ever collected" bits (token column 16), then adds the count to the token's item if the inventory has one, or makes a new item with that count. Either way the add goals get 0 and the token's ID.
- **A clothing add** looks the ID up in the `ClothingItems` table (0x10-byte rows: ID at `+0`, body-part name at `+4`, clothing-set name at `+8`). An unknown ID adds nothing. Otherwise the set's man's piece and woman's piece **each** become an item, if the set has one for that sex. So one clothing add can add two items, and Count (op 3) goes up by two. Adding clothing ID 0x90 also unlocks goal-and-unlock entry 0x20B of kind 2. Clothing adds don't run the add goals.

**Removing. Proven, PS2** (`RemoveItem` `0x1FA390`, matches; every remove in FamilyInventory goes through it). It takes a place and a count. A place past the end does nothing. If the item's count minus the count asked for is above 0, the item's count goes down by that much. Otherwise the item is deleted and taken out of the list, so every later item's place goes down by one, and if it was an object or an ownable the non-token count goes down by one. Asking for more than the item has removes it, with no error. Objects, ownables and clothing always have a count of 1.

| Op | What it does |
|---|---|
| 0 | **Add**, by category. Object or ownable with bit 7: the object whose ID is `+2` (none is error 0x33), added as itself. Without bit 7: the GUID in `+4`. An object whose GUID is a token's adds one of that token instead (see the table above). True if the add worked. Token: ID `+2` with bit 7, else `+4`, count `+3`. Clothing: ID `+2` with bit 7, else `+4`. Tokens and clothing are always true |
| 1 | **Remove**, by category. With bit 7, the item at place `+2`: one of it, or `+3` of a token. Otherwise: an object by GUID `+4`, or with bit 5 the object whose ID is `+2`, found by GUID (see below); an ownable by owner `+3` and GUID `+4` (the one with the most ownership points for that owner, see op 2); a token by ID `+4`, count `+3`; clothing by ID `+4`. The result: by place, always true; an object by GUID or an ownable, always true, found or not; an object by object ID, false when the object, the tribe inventory or the item is missing; a token or clothing by ID, false when there is none. Removing a token with an ID from 0xD5 to 0x112 also unlocks goal-and-unlock entry 0x70 of kind 2 |
| 2 | **Find**: the place of the first object with GUID `+4`, the ownable with GUID `+4` whose ownership points for owner `+3` are the highest and above 0, the first token with ID `+4`, or the first clothing with ID `+4`. It's written to `+2`, -1 if there's none, and the result is true only if found |
| 3 | **Count**: `+2` = the number of items |
| 4 | Item at place `+2` gets a new attribute `+3` |
| 5 | `+3` = attribute temp 0 of item `+2` |
| 6 | `+3` = the number of attributes of item `+2` |
| 7 | `+3` = the owner ID of item `+2`. No such item is error 0x6C |
| 8 | `+3` = the category of item `+2`. Error 0x6C if there's none |
| 9 | `+3` = the count of item `+2`. Error 0x6C if there's none |
| 0xA | Item `+2` gets status text `+4` (a byte). Error 0x6C if there's none |
| 0xB | `+2` = the number of items that aren't tokens |
| 0xC | Item `+2`: attribute temp 0 = `+3` |
| 0xD | `+2` = 0 for an object or ownable, or `+4` for a token or clothing item. Any other category is error 0x18 |

**Quirks, proven on PS2:** removing an object by object ID (op 1, category 0, bit 5) finds the object's place in the **tribe's** inventory, by the GUID in words 0xE and 0xF of its definition, then removes that place from the target inventory. A port that keeps one inventory won't notice. With no tribe, no object or no such item it's false. Operations 4, 5, 6 and 0xC don't check that the item exists, and op 7 reads the item's owner before it checks the item, so a place past the end reads through a null pointer before error 0x6C.

## The family inventory: what's in it, what the screen shows, and what its actions run

**The items.** The family's `InventoryItems` holds `InventoryItem`s of four kinds (the `+1` category above): object, ownable, token and clothing.
- **Fields:** each has an instance ID, a count (1 for everything but tokens), attributes, an owner and a status byte.
- **A token item** points at its `InventoryTokens` row (`TokenItem +0xC`, Wii `GetToken`).
- **Opcode 0x46:** Port's INVENTORY_ADD, REMOVE, FIND, COUNT and NON_TOKEN_COUNT are operations 0, 1, 2, 3 and 0xB in the table above.

**`InventoryTokens` rows** (0x24 bytes, 214 on the disc). Proven, PS2 and disc:

| Offset | Holds | Expression column (scope 0x42) |
|---|---|---|
| `+0` u32 | The object GUID the token stands for, 0 for none (gold, pearls and other finds have none) | 0xA (low half), 0xB (high half) |
| `+4` | **Flags** (below) | 8 |
| `+8` | Pointer to the token's **`TokenCatalogData`** row | — |
| `+0xC` u16 | Token ID | — |
| `+0xE`, `+0x10`, `+0x14`, `+0x18` to `+0x22` | Other columns | 7, 2, 5, 13, 14, 0, 3, 1, 4 |

- Column 12 is the token's place in the tribe inventory (`GetTokenItemIndex`), and column 16 whether it was ever collected.
- **Flags** (bit masks; an expression's "bit N" is `1 << (N − 1)`). Proven on PS2: `TokenItem`'s tests match (`IsTool` to `IsDye`, `0x24F070` to `0x24F150`):

| Mask | Meaning |
|---|---|
| 0x1 | tool |
| 0x2 | resource |
| 0x4 | food |
| 0x10 | can be put in the hand (with 0x100) |
| 0x20 | fast tool |
| 0x80 | **fast food** (can be eaten from the inventory) |
| 0x100 | hand object |
| 0x200 | needs identifying |
| 0x400 | can be planted |
| 0x4000 | dye |

- **`TokenCatalogData`** (0x18 bytes, 570 rows): `+4` the **name** and `+8` the **description**, both language strings; `+0xC` and `+0x10` the **icon** shader's name hash; `+0x14` a small number, likely a sort group.

| Token | ID | GUID | Flags | Icon |
|---|---|---|---|---|
| Coconut | 0xAE | `0xA78125FE` | 0x1096: resource, food, 0x10, fast food | `ocs_food_coconut` |
| Clam Meat | 0x130 | — | 0x1006 | `resource_shell_clam` |
| Large Clam Shell | 0x5E | `0xCC155766` | 0x1012 | `resource_shell_clam` |
| Gold | 0x133 | — | 0x1002 | `ocs_resource_gold` |
| Pearl | 0x136 | — | 0x1002 | `ocs_resource_pearls` |
| Jade, Moonstone | 0x135, 0x134 | — | 0x1002, 0x1000 | — |
| Treasure Map Piece | 0x32 | `0xDEC81937` | 0x1012 | — |

**What the opening adds.**
- **A palm harvest** adds one coconut token (0xAE) through `Sub - Add Resources` (FamilyInventory op 0, token, count 1). See "Harvest" above.
- **A clam harvest** adds the clam's own resource token, then may run `source_sg` 0x203F `Sub - Process Special Resources`. That adds **one** of Gold 0x133, Pearl 0x136, Jade 0x135 or Moonstone 0x134, each with its message, or once a Treasure Map Piece 0x32.
- **Putting a coconut away** (`Interaction - Put in Inventory`, `resources_sg` 0x2008) adds it as its token, since its GUID is the coconut token's.

**The screen.** The Inventory screen is **`NV5Target`** (movie `tsc6_inventory`); Frontend is building it.
- **Each entry** is a `UIInventoryItem`: the item's count, its name, description and icon from the token's catalogue row, and owner and status lines.
- **Tabs** come from the flags: any food (food or an ingredient), any tool (tool or fast tool), resource, object, clothing. Ingredient sub-tests (fruit, fish, meat and so on) go through the `IngredientManager`. Read on Wii, likely.

**The actions on an entry. Proven, PS2** (`0x2E5188` Eat, `0x2E5238` Put in hand; Wii `NV5Target::DoEatFromInventory`, `DoPutInHand`).
- **How they run:** each runs tree **0x20A0, `id_perform_inventory_action`**, on the selected Sim (`_globals +0x174`) through the Sim's TreeSim virtual (vtable `+0x50`), with the Sim's own behaviour and itself as the stack object.
  - **Eat:** parameters **{1, token ID, 0, 0}**.
  - **Put in hand:** **{0, token ID, 0, 0}**.
  - Wii also has `DoWearFromInventory`, which puts on a clothing item directly (`InventoryItems::PutOnClothingItem`), with no tree. The PS2 copy isn't read.
- **What the tree does:**
  1. **Make the object:** a token ID ≤ 0 is false. Otherwise it reads the token's GUID (columns 0xB and 0xA) and **CreateObject**s it out of the world. A failed create is false.
  2. **Eat** (parameter 0 ≠ 0) needs **food (0x4) and fast food (0x80)**, else false. It then:
     - snaps the new object to the Sim (SnapObject);
     - **pushes the object's interaction 0** on the Sim, priority 3, with the object as the queue icon. For a coconut, interaction 0 is **Eat** (check tree `resources_sg` 0x200D, action tree **0x200C `Interaction - Eat`**);
     - **removes one of the token** from the family inventory (FamilyInventory op 1, token, by its place from column 12), and returns true.
     - So the Sim eats a fresh coconut, and `Interaction - Eat` calls `Tutorial - Task Complete`, which is what step 11 waits for.
  3. **Put in hand** (parameter 0 = 0) needs **0x10 and hand object (0x100)**, else false. It runs the named check tree **`ct_fast_tool`** (`person_sg` STR# 0x12F, string 8) on the Sim with the new object as stack object. Tools have their own `ct_fast_tool` (for example `tool_spear_simple` 0x1009).
  4. **A coconut** has 0x10 but not 0x100, so it can be eaten from the inventory but not put in the hand. Gold, pearls and the other finds have no GUID, so neither action does anything for them.

## SetMotiveDelta (opcode 0x1D)

**Proven**: `cXObject::TrySetMotiveDelta` and `cXPerson::TrySetMotiveDelta` match the game byte for byte. 401 uses.

| Operand | Meaning |
|---|---|
| `+0` u8, `+4` s16 | Scope and data of the change |
| `+1` u8, `+6` s16 | Scope and data of the limit |
| `+2` u8 | The motive: 5 to 10, 12, 14 or 15. Anything else is error 0x1A |
| `+3` bit 0 | Clear all |
| `+3` bits 1, 2 | Influence on me, on the stack object. See below |
| `+3` bits 3 to 6 | Type, with 0 meaning 1 |

The change is the value times 1/1800, so it's per tick: a game hour is 1,800 ticks, so the value is the change per game hour (see "Time").

**On a non-Sim**, neither influence bit is -1 at once. With one set, it goes to `PassiveInfluenceSystem`: clear all calls `ClearInfluenceMotiveDeltas`, and otherwise `SetInfluenceMotiveDelta` records motive, change, limit and type for that object.

**On a Sim**, an influence bit means the non-Sim path. Without one, it's the Sim's own list at `+0x200`, 12 bytes an entry: motive, change, limit. Clear all empties it. It holds 16, and a full list quietly drops the new one. All paths without an error are **true**.

## Picking up: CanPlace into a hand

**Read on Wii, with PS2's error codes in the same order** (`cXObject::TestPlace`, PS2 `0x2104C8`, about 890 instructions). Likely. `Grab`'s two calls are `CanPlace(location, level, container, slot)` and `Place` with the same arguments, the container being me. `CanPlace` (`0x253938`) is `CanPlaceSingleTile` (`0x253958`): it builds a `PlacementSpec` and calls `TestPlace`.

**The `PlacementSpec`** (`0x255978`): `+0` a valid byte (1), `+4` the tile, `+0xC` the container, `+0x10` the slot (-1 without a container), `+0x14` the level. With a container whose slot isn't a TS2 slot, the tile becomes the container's own location.

**`TestPlace`** returns false at the first test that fails, with the reason in Wii's `gPlacementError`. Before the container tests it checks the tile: in bounds, tile flags and walls, grass for objects that need it or can't have it (0x13, 0x14), indoors or outdoors (9, 0xA), and without a container, `TestIntersection` (entry point 5). **With a container**, in order:

| Test | Fails with |
|---|---|
| The container has the slot | 0xD |
| The slot is empty, or holds this object | 0xB |
| No other slot of the container at the same height holds another object at the same offsets | 0xB |
| This object's variable 0x1B is no more than the container's variable 0x1C (size against room) | 0xE |
| This object isn't the container or anything that holds it | 0xB |

Then 0x18 and the end. So for the port: **a Sim can pick an object up when its hand slot exists and is empty, and the object's variable 0x1B fits the Sim's variable 0x1C**; the tile tests use the Sim's own tile, which a standing Sim passes.

## Motives over time

**Proven, PS2**, with Wii for the names. `cXPerson::Simulate` (PS2 `0x1AD8E0`) runs `SimMotives` (`0x1ADD48`) **only every 60 ticks** (when the tick count reaches `+0x214` + 60; this used to say every tick), that is every 2 game minutes. Every tick it adds one to person data 8 (it wraps to 0 at 0x8000) and applies the `SetMotiveDelta` list. See "Time" for how long a tick is.

**`SimMotives`** does nothing unless my variable 0x22 is 0 and I'm in the world. Then:

1. Person data 0xA set: `Motives::Cleanup` only. Otherwise `Motives::Sim` (`0x1FEA60`), which ends with `Cleanup`.
2. Unless I'm a chimp or a boar, while any motive is still locked (`GoalUnlock::AreAnyMotivesLocked`, vtable slot 18), **each locked motive is held at 0** every tick (`IsLocked(4, n)`, slot 3; this used to say "set", but the slot is Wii's `IsLocked`): 8 motive 5, 9 motive 6, 0xA motive 7, 0xB motive 9, 0xC motive 8, 0xD motive 15, 0xE motive 14, 0xF motive 13.
3. `CalcHappy` (`0x1ADF30`): mood, motive 3. See "Mood" below.

**`Motives::Sim`.** The `Motives` object is at Sim `+0x13C`; its `+0x44 + 4k` is motive k (Sim `+0x180`), and its first 16 floats are the decay rates. Each run (every 60 ticks), with `R(i)` = rate i / 720:

| Motive | Change per run |
|---|---|
| 5 | `R(s + 3)` |
| 6 | `R(s + 1)` × (1 + rain + wind). See "Motive 6 and the weather" below |
| 7 | `R(s)`. In family 1 (neighbour person data 0x28 is 1), at -90 or below it's a tenth of that (`/7200`). In any other family it never goes below -75 |
| 8 | `R(s + 4)` |
| 9 | `R(s + 2)` |
| 13 | Set to the current room's environment score (`RoomManager::GetRoomEnvironmentScore`), unless the room is 0xFFFB |
| 14 | `R(s + 5)` |
| 15 | `R(s + 6)` |
| 0 | Above -80, loses `aspiration decrement` (0.1) / 150 each run. See "Motive 0" below |

`s` is 0 normally and **8 while motive 11 is below 0**: a second set of rates, where motive 5 rises fast (+540), so motive 5 behaves like energy and a negative motive 11 like sleep. **`Cleanup` clamps every motive to -100..100.**

**The rates** (`Motives::InitDecayRates`, `0x1FEF80`), set from data in the executable:

> rate[i] = base[row][i] + Active[row][i] × pd2 / 1000 + Playful[row][i] × pd4 / 1000 + Outgoing[row][i] × pd5 / 1000 + Neat[row][i] × pd6 / 1000

where pdN is the neighbour's person data N and the trait names are Wii's (`sMotiveDecayBase`, `sMotiveDecayActiveMod`, ...). The row is by Sim type: 0, 1, 5 and others row 0; 2 row 1; 3 and 4 row 2. The tables are at `0x4B9690` (base), `0x4B9750`, `0x4B9810`, `0x4B98D0` and `0x4B9990`, each three rows of 16 floats. Row 0, which the castaways use:

| Rate | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Motive | 7 | 6 | 9 | 5 | 8 | 14 | 15 | 7 | 6 | 9 | 5 | 8 |
| Base | -184 | -75 | -75 | -64 | -50 | -75 | -75 | -30 | -75 | -150 | 540 | -50 |
| Active × pd2/1000 | -50 | | | +50 | | | | -20 | | | | |
| Playful × pd4/1000 | | | | | | | -100 | | | | | |
| Outgoing × pd5/1000 | | | | | | -100 | | | | | | |
| Neat × pd6/1000 | | | | | -75 | | | | | | | |

Rates 0 to 6 are the normal set, 8 to 12 the set while motive 11 is below 0 (13 and 14 are 0). So an average-personality castaway (traits 500) loses motive 7 at (-184 - 25) / 720 = 0.29 a run, so 8.7 a game hour (30 runs), or about 23 game hours from 100 to -100.

**Mood: `CalcHappy`** (`0x1ADF30`). **Proven, PS2.** Mood is motive 3 (Sim `+0x18C`). It's a **weighted mean of ten motives**, each weighted by a curve of its own value:

> mood = Σ (motive × weight(motive)) / Σ weight(motive)

- **Which curves, by Sim type** (`0x1DD428`): 0 uses the curve set at `0x5101C8`, 1 the set at `0x510298`, and 5 the set at `0x510368`. Types 2 to 4 reach no set, and above 5 mood isn't updated.
- **Where the curves come from.** `0x1A6F90` fills each set the first time:
  - the motive of curve k is entry k of `0x4B8858`: **5, 6, 7, 9, 8, 14, 15, 13, 10, 12**;
  - `MotiveCurveSet::LoadFromFile` (`0x1FF120`) reads the points from the global object's string set: **`0x1F6`** for type 0, **`0x1FC`** for type 1, **`0x1F8`** for type 5. String k is curve k's points, as text `(x;y) (x;y) …`, parsed by `PiecewiseFn::AddPointsFromText`.
- **The lookup** is linear between points. At or below the first x it's the first y, and above the last x it's the last y.

Set `0x1F6`, weight against the motive's value, from the disc:

| Curve | Motive | Points (value; weight) |
|---|---|---|
| 1 | 5 | (-100;0) (-60;5) (-40;3) (0;1) (100;1) |
| 2 | 6 | (-100;10) (-80;3) (-40;1) (100;1) |
| 3 | 7 | (-100;15) (-60;5) (-40;3) (0;1) (100;1) |
| 4 | 9 | (-100;10) (-80;3) (-60;1) (100;1) |
| 5 | 8 | (-100;5) (-80;3) (-40;1) (100;1) |
| 6 | 14 | (-100;5) (-40;2) (40;2) (100;3) |
| 7 | 15 | (-100;5) (-40;2) (40;2) (100;5) |
| 8 | 13 | (-100;2) (0;1) (100;2) |
| 9 | 10 | (-100;0) (100;0) |
| 10 | 12 | (-100;0) (100;0) |

So a motive weighs more the lower it gets: a starving motive 7 counts 15 times as much as a full one. Motives 10 and 12 don't count. **Sets `0x1F8` and `0x1FC`** are identical to each other. They give motive 5 no weight, and they change:
- motive 6: (-100;10) (-80;3) (-40;1) (0;1) (100;3);
- motive 8: (-100;12) (-80;3) (-40;1) (100;1);
- motive 14: (-100;7) (-40;2) (40;1) (100;3);
- motive 13: none;
- motive 10: (-100;10) (-80;3) (-60;1) (100;2).

Which type the castaways are is likely 0 (see the Sim's type above).

**Motive 6 and the weather** (`Motives::Sim`, `0x1FEC50` to `0x1FEEC4`). **Proven, PS2**, with Wii's names (`WeatherManager`, `cFixedWorld::HasRoof`, `cXPerson::GetRainCarry`):

1. Start with rain = 0 and wind = 0. If it's raining (`0x356908`, Wii `IsRaining`):
   - rain = the weather manager's (`0x50A6A4`) rain level `+0x9C`;
   - wind = its wind level `+0x80`, unless wind is disabled (`0x123C70`, or bit 0 of the flags at `+0x108`).
2. **In a room.** If either is above 0 and my variable 0x1D, my room, isn't 0, it looks up the room (`0x1DFEA0`, Wii `RoomManager::GetRoom`). If the room's first word is above 0:
   - its second word above 0 gives no rain and no wind;
   - otherwise the wind is halved.
   Likely these mean "enclosed" and "roofed".
3. **Under a roof.** If rain is still above 0 and my tile is inside the lot (x from 1 to width − 2, y likewise), a tile with a roof (`HasRoof`, bit 3 of the tile's flags) gives no rain.
4. **Carrying rain cover.** If rain is still above 0, `GetRainCarry` (Sim `+0x218`) = 1 multiplies it by 0.75, and 2 by 0.5.
5. Motives `+0x54` = rain, and **motive 6 changes by R(s + 1) × (1 + rain + wind)**.

**Where the levels come from.** They are whole numbers set by `SetRainLevel` and `SetWindLevel`, from the current weather state's sequence. On Wii, `SetWeatherState` takes the wind from the sequence entry's `+8` and the rain from its `+0xC`; the RoomEffect primitive can set them too. The states are quickdata table `WeatherState`: each row is a vector of 0x24-byte sequence entries, starting with a duration and a variance (likely game seconds). From the disc:

| State | Stages: duration ± variance, wind, rain |
|---|---|
| `clear` | 21600 ± 1800, 0, 0 |
| `rainy` | 3600 ± 1800, 1, 0 · 3600 ± 600, 1, 4 · 5400 ± 1800, 1, 1 |
| `stormy` | 14400 ± 2400, 2, 0 · 14400 ± 600, 2, 3 · 14400 ± 2400, 2, 1 |
| `hurricane` | 21600 ± 3600, 2, 0 · 21600 ± 10800, 3, 0 · 10800 ± 900, 4, 4 · 1800 ± 300, 0, 0 · 9900 ± 300, 4, 4 · 21600 ± 900, 3, 2 · 21600 ± 900, 2, 1 |

So an unsheltered castaway in a rainy state's peak (wind 1, rain 4) loses motive 6 six times as fast as in clear weather. How the next state is chosen (`ChooseNewWeatherState`) isn't read.

**Motive 0: the aspiration decrement** (`Motives::Sim`, `0x1FEF10`). **Proven, PS2.** Each run, if motive 0 is above -80 it loses `aspiration decrement` × 1/150 (`0x3BDA740F`).
- `aspiration decrement` is the float at `0x510960`. A tuning loader (`0x1FE380`, slot 2 of the vtable at `0x4BB1E8`) reads it by name with a default of 0.1.
- `SimsObjects` quickdata holds the same name with **0.1**, next to `aspiration mood bonus ratio` 0.3, `aspiration points per meter point` 12.5, `sleepiness multiplier` 7 and the other motive tunings. So it's 0.1 either way.
- **So motive 0 loses 0.1 / 150 ≈ 0.00067 a run**, about 0.02 a game hour, and stops at -80.

**The `SetMotiveDelta` list** is applied after `SimMotives`, entry by entry (motive, change, limit; the change is already per tick): a negative change lowers the motive only while it's above the limit, and stops at the limit; a positive one raises it only while it's below, and stops at the limit.

## Time: ticks, the game clock and game speed

**Proven, PS2**, with Wii for the names, unless marked likely.

**Real time to game time: `cSimulator::Simulate`** (`0x1BF008`, Wii `Simulate__10cSimulatorFv`) runs once a frame:

1. `UpdateSpeed` (`0x1E0C50`) sets the speed multiplier `+0x98`, below.
2. `ms = min(dt × 1000, 66.67)`, where `dt` is the global float at `0x49FDC4` (see "Frame time" below).
3. Unless paused, the accumulator `+0xA0` gets `ms × multiplier`. It's capped at **2,520**: the excess is taken back out of `ms`.
4. `dt` becomes `ms × 0.001`, and the true dt is kept at `+0xA4` (`RestoreTrueDt`, `0x1E0948`, puts it back).
5. Unless paused: **while the accumulator is at least 42, take 42 off it and run `SimulateOneTick`** (`0x1BEF10`). If that pauses the game, the accumulator is emptied.
6. Paused: `SimulateOneTick` runs once, for the objects allowed to run while paused, and no game time passes.

So **one tick is 42 ms of real time at 1×**: 1000 / 42 = **23.8 ticks a second**, whatever the frame rate.
- **Low frame rates:** below 15 frames a second, frames are capped at 66.67 ms, so the game slows down.
- **Backlog:** at most 60 ticks (2,520 ms) can be waiting.
- **At 60 frames a second**, a tick runs every 2 or 3 frames: 0.4 ticks a frame.

**Frame time.** `0x49FDC4` starts as 1/60. Each frame the engine's timer (`0x3E2E68` on) counts the vertical blanks since the last frame, from 1 to 15, and sets dt to that count × the frame period. That period is **1/60** (`0x3C888889`), or **1/50** (`0x3CA3D70A`) when the flag at `0x500638` is 1, likely the PAL setting. Another mode, chosen by a flag at the timer's `+8` that its constructor clears, uses a 3-frame average of the measured time instead. Likely the first mode is the one used. Either way **dt is real time**, so ticks follow the clock, not frames.

**A tick: `SimulateOneTick`** (`0x1BEF10`).
- **`TickAllObjects`** (`0x1BF1B8`) walks every object ID from a cursor kept at `+0xC4`.
  - **Asleep:** an object with idle status above 0 has it counted down (the Idle primitive's sleep).
  - **Otherwise:** the object's `Simulate` (vtable `+0x60`) runs, with the tick count.
  - **A failed `Simulate`:** stops the walk at once, returns false, and the next tick resumes at that object.
  - **While paused:** only objects flagged to run while paused are run, and it returns false.
- **If it returned true:** the tick count `+0x9C` goes up by 1, and `UpdateGameTime(2)` (`0x1BF480`) runs.
- **Then, every tick:** the passive influence check, `PlanManager::Update` and `ObjectModule::PostSim`.

**The game clock: `UpdateGameTime(2)`.** Each tick adds **2 game seconds** to global 6, unless the one-shot flag at `0x49240C` says to skip once.

| Global (simulator `+0x38` + 2n) | Unit | Rolls over at |
|---|---|---|
| 6 (`+0x44`) | seconds | 60 (`0x4B6710`) |
| 5 (`+0x42`) | minutes | 60 (`0x4B670E`) |
| 0 (`+0x38`) | hour | 24 (`0x4B670C`) |
| 1 (`+0x3A`) | day of the month, from 1 | above 28 (`0x4B6706`). Each new day also adds 1 to global 0x1C (`+0x70`) and calls `ObjectModule::DayChanged` |
| 7 (`+0x46`) | month, from 1 | above 12 (`0x4B6708`) |
| 8 (`+0x48`, likely: Wii adds a year after month 12) | year | |

So **a game minute is 30 ticks** (1.26 s at 1×), a game hour 1,800 ticks (75.6 s), and a day 43,200 ticks (30.24 minutes).

This agrees with the trees' own unit: SetMotiveDelta's change is the value × 1/1800 a tick, so its value is **per game hour**. The game starts at hour 9 (global 0x18).

**Time of day.** Likely (Wii, `GameTime::ComputeTimeOfDay` on the hour):

| Hour | Value |
|---|---|
| Below 6 | 2 (night) |
| 6 | 3 (dawn) |
| 7 to 17 | 0 (day) |
| 18 | 1 (dusk) |
| 19 on | 2 (night) |

I didn't find PS2's copy.

**Game speed** (`UpdateSpeed`, `0x1E0C50`, and `TranslateSpeedToFloat`, `0x1E0CD8`). The multiplier `+0x98` is:
- **0:** while paused (`+0xD0`, `+0xD8`, or the pause count `+0x5A`), or stopped (`+0xC4`) outside `Simulate`.
- **Otherwise,** by the speed setting `+0x58`:

| Speed | Multiplier | Ticks a second |
|---|---|---|
| 0 | 1.0 | 23.8 |
| -1 | 0.5 | 11.9 |
| -2 | 5.0 | 119 |
| -3 | 30.0 | 714 |
| Anything else | 1.0 | 23.8 |

**How the player sets it** (H2DTarget's speed input step, `0x2A0578`, request at HUD `+0x224`; it calls `UpdateSpeedControls`, `0x2A0408`, for the icon, as ui.md says):
- **-3 (30×)** when no family member is awake and visible (`0x2550D8`, Wii `IsFamilyMemberAwakeAndVisible`), unless a check on `0x514EF4` holds it at 0. **So time races while everyone sleeps.**
- **-2 (5×)** while controller command 0x28 is held, and only if fast-forward is enabled (`0x514F3C`).
- **-4** (command 0x27) pauses.
- **Otherwise 0.**

-1 isn't used there. The multiplier scales only how fast real time fills the accumulator. **A tick's effects are the same at every speed:** faster means more ticks a second. Locomotion's time multiplier (locomotion.md) likely reads the same value.

**Motives: every 60 ticks. Proven, PS2.** `cXPerson::Simulate` (`0x1AD8E0`) calls `SimMotives` only when the tick count has reached the Sim's `+0x214` + 60, and then sets `+0x214` to it. So motives decay **once every 2 game minutes** (2.52 s at 1×), 30 times a game hour. Everything else in `cXPerson::Simulate` runs every tick:
- person data 8 goes up by 1;
- the `SetMotiveDelta` list is applied;
- the neighbour's data words 8 to 0x11, the skills, are held at no more than 1,000.

**One Simulate tick = 42 ms of real time at 1× (23.8 a second; ×0.5, ×5 or ×30 at the other speeds); one game minute = 30 ticks (2 game seconds a tick).**

## Global 3: the selected Sim

**Proven, PS2.** Global 3 (simulator `+0x3E`) is the **selected Sim's object ID**, or 0 with none. Trees can read it but not write it: `GetGlobalRef` refuses index 3, as it does 0xB, 0x20 and 0x2E. Only `EGlobal::SetSelectedPerson` (`0x32AC78`) writes it. That also sets `_globals +0x174` (the player's Sim), the Sim's animator and flags, and the neighbourhood's word `+0x18` = the Sim's neighbour ID (person data 0xC). It runs whenever the selection changes: the player switching Sims (`SimInteractor::Update`), GenericSimCall 0x1F, and a few other callers. So a port that keeps the selected Sim's ID in global 3 is doing what the game does.

## Goals: what GenericSimCall 0x42 returns

**Proven, PS2**, with Wii's names. The `GoalUnlock` vtable is at `0x4BC0B0`; its slots are listed at the end. Each PS2 function below was compared with Wii's, and they use the same goal numbers, dialog keys, lot numbers and calls, in the same order. PS2 inlines `UIAUDIO::Instance` (a singleton at `0x49292C`), `EHouse::GetLotNum` (house `+0x3C`) and `cXPerson::GetNeighbor`.

| Function | PS2 | Wii |
|---|---|---|
| `ClearGoalsAndUnlocks` | `0x1EF7A8` | `8010FAD8` |
| `GetUnlockIndex(type, number)` | `0x1EFB40` | `8010FF44` |
| `TryEvaluateGoals` | `0x1EFC80` | `80110830` |
| `PlanMade` | `0x1EFD98` | `80110950` |
| `ResourceHarvested` | `0x1F0010` | `80110C20` |
| `CheckBook1` to `CheckBook6` | `0x1F03D0`, `0x1F0DF0`, `0x1F1698`, `0x1F1D90`, `0x1F22D8`, `0x1F2D90` | `80110FE0` on |
| `CheckMiscGoals` | `0x1F36C8` | `80113370` |

### The goal data in the save

These are the 128 bytes world.md describes under "`GoalUnlock`" (neighbourhood data, goal data `+4`):

| Bytes | Holds |
|---|---|
| 0 to 5 | Books 1 to 6 |
| 6 to 0x17 | Pages: book × 10 + page, pages 1 to 3 |
| 0x18 to 0x5F | Goals: book × 100 + page × 10 + goal, goals 1 to 4 |
| 0x60 to 0x67 | Unlocks 0 to 63, one bit each: unlock n is bit n % 8 of byte 0x60 + n / 8 |
| 0x68 to 0x7F | Goals 1000 to 1023, the "misc" goals. Byte 0x7F is also `DoStream`'s marker |

Each byte is a signed status:

| Status | Book or page | Goal |
|---|---|---|
| -1 | Not used | Not used |
| 0 | Locked. Every book and page starts here | Not used by the checks |
| 1 | Open | Open, not achieved. Every goal starts here |
| 2 | Not used | **Achieved**, reward not given yet |
| 3 | Finished | **Rewarded** |

- **`ClearGoalsAndUnlocks`** (new game) zeroes all 128 bytes. It sets every goal, 0x18 to 0x5F and 0x68 to 0x7E, to 1. Then it marks these as unused (-1): page 12, page 43, and goals 134, 213, 214, 233, 234, 324, 414, 423, 424, 532, 533, 534, 613, 614, 632, 633 and 634.
- **Unlocks all start clear.** So a new game has every motive locked (see "Motives over time").
- **`GrantUnlock(type, n)`** (slot 9):
  - a goal goes from 1 to 2 only;
  - a book or page goes from 0 to 1 only;
  - an unlock sets its bit.
- **`IsCompleted`** is status 3, or the bit set for an unlock. **`IsLocked`** is status 0, or the bit clear.

### Who marks a goal achieved (1 to 2)

Something outside the checks does it, and the checks only reward it. The callers found:

- **Trees**, through CompleteGoal operation 2 on type 2 (see "CompleteGoal"). Some examples:
  - `resources_sg` `Interaction - Eat` and `food_carry_sg` `Interaction - Eat SG`: goal 1014;
  - `bed_sg` `Interaction - Sleep`: goal 1012;
  - sitting in a chair (`Motives - Comfort Inc`), relaxing on a bed or sitting by a fire: goal 1010;
  - `social_sim2sim` `Greet - Person A`: goals 121 and 1011;
  - `art_painting_sg` `SG_I_View`: goal 1013;
  - `global` `Skill - Show Level Gain`: goal 1017;
  - `crafting_sg` `Repair - Rock Workbench`: goal 111;
  - the airplane radio: goals 131 and 132.
- **`PlanMade(plan)`** (slot 16), when a plan is made:
  - plans 0xD6 to 0xE6: goals 114 and 1010;
  - 0x6C: goal 231;
  - 0xC to 0x13: goal 222;
  - 0x49 to 0x55: goal 223;
  - 0x185 to 0x188: goal 623;
  - 0x15: goal 111;
  - 0x16: goals 111 and 1006;
  - 0x32: goal 622;
  - 0x38, 0x3C, 0x40 or 0x44: the first one sets unlock 27, and a second one, with unlock 27 already set, achieves goal 612;
  - plans 0x12C to 0x1CD whose token is clothing for body part 0x1E: goal 224.
- **`ResourceHarvested(token)`** (slot 17), when a resource is harvested:
  - 0x7B: goal 1001;
  - 0x9A: goal 1002;
  - 0x8F: goal 1003;
  - 0x6F: goal 1004;
  - 0x99: goal 1005;
  - 0x12C: goal 321;
  - 0x12E: goal 413;
  - 0xD9, 0x102, 0x10A or 0x10E: goal 323.
- **The checks themselves**, for goals whose condition is in code. They are listed per book below.

### `TryEvaluateGoals(book)`

1. No `PlanManager`: returns 0.
2. Book 0: `CheckMiscGoals`.
3. Books 1 to 6: returns 0 if the book's status is 0 or 3. Otherwise it runs `CheckBook1` to `CheckBook6`. Any other book returns 0.

**The value returned is a dialog key, not a goal number.** GenericSimCall 0x42 puts it in t1 and takes the true branch when it isn't 0. `person_sg` `Process Unlock Dialogs` (0x20A3) then picks the dialog from parameter 0, the book, and parameter 1, this key. One call gives at most one key, since each check returns as soon as it rewards something.

**Who calls it.** `person_sg` `Update Goals and Unlocks` (0x20A1) runs at the end of every action. Node 0 goes on only when my object variable 0xB equals global 3, the selected Sim's ID (likely: that variable is the Sim's own ID):

1. **Book 1 still locked:** if tutorial flag 0 (`House Complete`, unlock 0) is set, it opens book 1 (CompleteGoal operation 1).
2. **Each open book** (status 1), 1 to 6 in order: GenericSimCall 0x42 with t0 = the book. The first one that returns true goes straight to `Process Unlock Dialogs`.
3. **None did:** t0 = 0, the misc goals. True goes to `Process Unlock Dialogs`.

### How each check works

Every check walks its pages in order:

- **An open page** (status 1) walks its goals in order. The first goal at status 2 is rewarded:
  - `PlanManager::UnlockGroupPlan(goal number, 0)`;
  - if that moved no plan, sound `0xF72F0A3C` (`UnlockGroupPlan` plays its own when it does);
  - status 3;
  - **return the goal's dialog key at once.**
- **A page whose goals are all 3**, or unused, is finished the same way. Its plan group is the page number, it gets its own key, and its status becomes 3.
- **A book with no open page left** is set to 3, and the check returns 0.
- **Opening a page.** A locked page (0) is opened by its own condition, and that also returns a key.
- **Unused pages and goals** (-1) never block a page or a book.

**What a reward unlocks.** `UnlockGroupPlan(g)` (Wii `800D2D48`) moves every locked plan whose group (`Plan +0x10`) is g into the unlocked list and marks it unlocked. It plays the unlock sound `0x8335FCBB` unless its second argument is set, and returns whether any plan moved. **So a finished goal's plan group unlocks crafting plans, not motives.** Motives are unlocks 8 to 15, and only two things set them: a tree's CompleteGoal, and `CheckMiscGoals` (below).

**Book 1** (`CheckBook1`):

| Step | Condition and result | Key |
|---|---|---|
| Open page 11 | Page 11 locked. Opens 11 and 13, and 12 too if more than one person is on the islands (`GetNumberOfPeopleOnIslands`). Returns a key only on lot 1 or below, after sound `0xFC510AC7`. Otherwise the walk goes on | 0x14 |
| Goal 111 | Achieved by plans 0x15 and 0x16, or by `Repair - Rock Workbench` | 0x16 |
| Goal 112 | Achieved outside the check | 0x18 |
| Goal 113 | Achieved by the check **on lot 3** | 0x1E |
| Goal 114 | Achieved by plans 0xD6 to 0xE6 | 0x24 |
| Page 11 done | Plan group 11 | 0x19 |
| Goal 121 | The selected Sim's neighbour has a relationship of type 0 (`CountRelationshipsOfType(0, 1)` above 0), or `Greet` | 0x3A |
| Goal 122 | A relationship of type 6 | 0x3B |
| Goal 123 | The family has more than one member | 0x3C |
| Goal 124 | Type 0 relationships = people on the islands − 1 | 0x3D |
| Page 12 done | Plan group 12 | 0x3E |
| Goals 131, 132, 133 | Outside the check (131 and 132 are the airplane radio) | 0x40, 0x41, 0x42 |
| Page 13 done | Plan group 13 | 0x6D |

**Books 2 to 6** follow the same pattern. Each first opens its page 1 with a key, and that returns only up to a lot number: book 2 lot 5 or below (0x21), book 3 lot 7 (0x2F), book 4 lot 11 (0x3F), book 5 lot 17 (0x4E). Book 6 opens page 61 with key 0x5C at once. Then:

- **In-code goals:**
  - goal 211: `CountRelationshipsOfType(6, false)` is more than `CountRelationshipsOfType(6, true)`;
  - goal 232: on lot 7;
  - goals 311 to 314: on lots 8, 9, 11 and 14;
  - goal 422: on lot 17;
  - goals 512 to 514: on lots 22, 21 and 26;
  - goal 521: unlocks 20 to 23, the four statues, all set;
  - goal 531: on lot 16 with the selected Sim's motive 15 at or above a constant (likely: the float compare isn't read further);
  - goals 611 and 621: a room meeting a size condition (`RoomManager`, words `+0` and `+4`).
- **Pages that open on something other than the previous page:**
  - page 22 opens when goal 114 is rewarded (plan group 0x336);
  - page 23 on unlock 16 ("Unlock Page 23", group 0x337);
  - page 42 on unlock 19 ("Unlock Page 42", group 0x34A);
  - page 52 on the four statue unlocks, with lot checks against 21 and 24 that aren't read in detail;
  - page 53 on lot 16 or above (group 0x355);
  - pages 62 and 63 on the tribe's inventory.
- **What finishing page 11 of book 2 also does:** if page 12 is unused (a single castaway), it unlocks page 12's plan groups 0x79 to 0x7C and 0xC, so nothing is lost.

The keys run 0x21 to 0x2E and 0x36 to 0x38 (book 2), 0x2F to 0x34, 0x6F to 0x71, 0x79 (book 3), 0x3F to 0x4D (book 4), 0x4E to 0x5B, 0x72 (book 5) and 0x5C to 0x67 (book 6). This is **likely** for the per-goal detail of books 2 to 6. It's read on Wii, and PS2 was compared by its goal numbers, keys and calls, not line by line.

**The misc goals** (`CheckMiscGoals`, book 0) run every time:

1. **Opening books by lot.** It opens any still-locked book on reaching a lot:
   - book 1 above lot 1;
   - book 2 at lot 4 or above;
   - book 3 at lot 7;
   - book 4 at lot 11;
   - book 5 at lot 17;
   - book 6 on lot 18.
2. **Goal 1009** is achieved by the check when the selected Sim has a type 0 relationship (`CountRelationshipsOfType(0, 0)` above 0).
3. **Goals 1009 to 1014 unlock a motive each** when rewarded. These are the only goals that do. The reward is plan group 1009 to 1014, status 3, then `GrantUnlock(4, n)`. The key is returned only on lots up to 8; past that, the walk goes on.

| Goal | Achieved by | Unlock | Motive | Key |
|---|---|---|---|---|
| 1009 | A type 0 relationship (the check) | 14 Social | 14 | 0x7E |
| 1010 | Sitting, relaxing on a bed, sitting by a fire; plans 0xD6 to 0xE6 | 9 Comfort | 6 | 0x7F |
| 1011 | `Greet` | 12 Hygiene | 8 | 0x80 |
| 1012 | Sleeping in a bed | 13 Fun | 15 | 0x81 |
| 1013 | Viewing a painting | 15 Room | 13 | 0x82 |
| 1014 | Eating, after the tutorial; pulling toilet paper; inspecting the catamaran | 11 Bladder | 9 | 0x83 |

4. **Goals 1001 to 1008, 1015, 1016 and 1017** give only a plan group and a key: 0x75, 0x76, 0x77, 0x78, 0x7A, 0x7B, 0x7C, 0x7D, then 0x84, 0x85, 0x86. Goals 1007, 1008 and 1015 play sound `0xB22C2A90` themselves; they and 1016 and 1017 pass 1, so `UnlockGroupPlan` plays none.

The unlock names are the global constant table 0x116, `Tutorial Flags`: 8 energy, 9 comfort, 10 hunger, 11 bladder, 12 hygiene, 13 fun, 14 social, 15 room. The motive each one guards is `IsMotiveUnlocked`'s table (`0x804199B8` on Wii): motives 5 to 9 → 8, 9, 10, 12, 11, and 13, 14, 15 → 15, 14, 13.

### The eat goal and hunger

**Proven on the disc and PS2.** Hunger (motive 7, unlock 10) is **not** unlocked by a goal check. The tutorial object's own trees do it.

- **Where the state is kept.** The tutorial runs `Tutorial - Tutorial House` (0x1001) from `Function - Main`. Its state is **the tutorial object's attribute 1**, matched against global constant table 0x12C, `Tutorial State`: 10 is "Survival Basic 3: eat", 11 is "Basic - Eat".
- **How a step advances.** The tree picks the step's subtree by that number. When the subtree returns true, it adds 1 to attribute 1 and returns false.
- **Step 10 is `Basic - Survival Basic 3: eat`** (0x1009):
  1. `sub - Create Coconut`;
  2. a modal Dialog, string 0x18 (string 2 on Wii);
  3. **CompleteGoal operation 3, type 4, constant `0x4B0A`**, which is table 0x116 entry 10, so `GrantUnlock(4, 10)`: hunger unlocked;
  4. return true, so step 11, `Basic - Eat` (0x1046), starts.
- **So hunger unlocks when the player closes the eat step's message, before the Sim eats.** From then on `SimMotives` no longer holds motive 7 at 0.
- **The other motives' tutorial unlocks:**
  - step 12, `Survival Basic 4: sleep`, unlocks energy (8);
  - `Tutorial - End`, from node 0, unlocks energy, hunger and tutorial flag 0. Its nodes for hygiene (12) and bladder (11) can't be reached from node 0;
  - the rest come from the misc goals above.
- **When eating counts as goal 1014.** `Interaction - Eat` marks goal 1014 only when tutorial flag 0 (`House Complete`) is set: node 40 tests it, and node 41 sets goal 1014. During the tutorial, eating marks no goal. Bladder unlocks through goal 1014 after it, for example by eating.
- **For a port to do this on the beach:**
  - keep the tutorial object's attribute 1 as the step;
  - run step 10's three actions when it is reached: make a coconut, show the message, set unlock bit 10;
  - then advance to 11.
  - Book 1 stays locked while tutorial flag 0 is clear and the lot is 1. So `Update Goals and Unlocks` evaluates only book 0, the misc goals, until then.

### The vtable

PS2 `0x4BC0B0`, with 8-byte entries from `+8`. The slots are:

| Slots | Functions |
|---|---|
| 0 to 5 | `ClearGoalsAndUnlocks`, `GetUnlockCount`, `IsUsed`, `IsLocked`, `IsUnlocked`, `IsCompleted` |
| 6 to 8 | The same three by book, page and goal |
| 9 to 13 | `GrantUnlock`, `GetStatus`, `SetStatus`, `IsRecentlyUnlocked`, `SetRecentlyUnlocked` |
| 14 to 17 | `DoStream`, `TryEvaluateGoals` (`+0x80`), `PlanMade`, `ResourceHarvested` |
| 18 to 21 | `AreAnyMotivesLocked`, `IsMotiveUnlocked`, `OpenAllBooksAndPages`, the destructor |

`GetStatus` on an unlock returns its bit, and `SetStatus(4, n, 1)` grants it.

## CameraControl (opcode 0x28)

**Proven, PS2** (`0x232378`) for what it returns, with Wii's names (`cXObject::TryCameraControl`). The camera work itself (which animation, cuts, interpolation, letterbox) is **not read** in detail. 57 uses.

It's **presentation**: it drives the `CameraDirector` (`_globals +0x1D0`) and keeps its own progress in the frame word `+8`. What a tree sees:

| Case | Returns |
|---|---|
| `_globals +0x464` set (Wii `+0x3B4`; what sets it isn't read) | Frame `+8` = 0, **false** |
| The camera animation it asked for is still loading | 2: wait |
| **`+4` bit 6, the test:** the director isn't in control, or it's in its hold state (state `+0x2B0` 0, 2 or 8) | **true**, otherwise **false**. Nothing is started |
| The director's state `+0x2AC` is 2 | false |
| I'm the selected Sim, `+4` bit 7 set, `+6` bit 2 clear, and a player interactor exists | false |
| A target object (`+4` bit 4) that doesn't exist | Error 0x15, -1 |
| Started, **`+4` bit 5 set** | **true at once** |
| Started, bit 5 clear | 2 (wait) every tick until the director is done (state `+0x2B0` back to 0 or 8), then **true** |

`+4` bit 0 also enables cancelling (`CameraDirector::CancelEnable`).

**Who branches on it.** 12 of the 57 nodes have different true and false targets:

- **Four are the test** (`00 00 00 00 ef 01 20 00`), in `Intro Camera` (three scenarios) and `scenario_sg` `Release to Game Camera`. They loop on `idle` until the camera is free.
- **Seven are portal arrivals and the flood reaction.** They start a shot with bit 5 set and take a different path when it fails.
- **One is `Interaction - Intro Sequence` node 9** (below).

**For a port with no camera:** return true. The test then means "the camera is free", and every started shot counts as done. The tree's flow is the same as when the camera works, minus the waits.

## The opening on the beach

**Proven on the disc** (the trees as written, followed from node 0). The primitives are the ones documented in this file.

### The scenario: `scenario_first_playable`

**`F_Main` (0x1001):**

1. My attribute 4 = 0; `idle`.
2. **`Setup Start Sequence`** (0x1004). With the selected Sim (global 3) as the stack object, it calls `Hold Sim`, which pushes an action on it. Then `CheckObject`, and three `FamilyInventory` operations on the tribe.
3. If that returns true:
   - stack object = global 3, local 0 = it, local 1 = 0;
   - **PushAction** `21 00 01 03 01`: likely `Interaction - Intro Sequence` on the Sim;
   - then `Function - Main Loop`.

   If it returns false, it calls `Intro Camera` and then the main loop.
4. **`Function - Main Loop`** (`scenario_sg` 0x2029) is reached only at node 0, which calls `idle` forever. The rest of that tree (`Release to Game Camera`, `Set Selection Flags`) can't be reached.

**`Interaction - Intro Sequence` (0x100F), on the Sim:**

1. Node 0: if neighbourhood variable 6 bit 16 is set, return true at once. **The intro runs only once.**
2. Set that bit. Local 1 = my stack object.
3. `Animate` (`02 …`, then `01 …`). The second's false path loops through `idle`.
4. **`Sub - Go to Start Position`** (0x1000), the one CreateObject here:
   - SetToNext type 4 for GUID `0x50323DB5`, the start marker;
   - only if there's none, **CreateObject** it out of the world (place 6), then `FindGoodLocation`;
   - mark it (its variable 0x22 = 1) and `GotoRelative` to it.
5. My person data 0x22 bit 1. t0 = me. **GenericSimCall 0x1F** selects me.
6. **IdleAnimate** `01 00 00 00 00 10 00 00`. True goes on. **False is error 253**, which resets the Sim's action.
7. `Animate` (`02 … 80`). Its false path loops through `idle`.
8. **CameraControl node 9** (`00 00 09 01 2a 01 04 00`, bit 5 set): true calls `idle` once, false skips it.
9. **GenericSimCall 0x09** (`LoadFinishing`).
10. `Animate` (`02 00 …`). Its false target is itself.
11. **CameraControl node 10** (`03 00 09 00 0a 0a 00 00`, bit 5 clear) waits for the shot, then continues either way.
12. Stack object = local 1. **Its attribute 4 = 1**: this tells `Intro Camera` the intro is done. Return true.

Nodes 2 and 13 to 21 (a neighbour count and two Dialogs) can't be reached.

**`Intro Camera` (0x1003):**

1. Node 0: `idle` until my attribute 4 is 1, set by the Intro Sequence.
2. It counts the neighbours with Neighbor data 0x2C = constant 0x137[0] and data 0x28 from 1 to 6 (SetToNext type 5).
3. A **modal Dialog** (type 3, one OK button): message string 3 under title string 2 with more than one castaway, message 6 under title 5 otherwise. It waits for the player. The text is in "Dialog (opcode 0x24)" below.
4. **`Tutorial - Task Complete` (2)**.
5. **CompleteGoal op 3 on unlock 26** ("Start Tutorial", constant `0x4B20`).

Its CameraControl nodes 1 to 4 can't be reached from node 0.

### The tutorial object: `tutorial`

- **`Function - Init` (0x1015).** It kills any other tutorial object (GUID `0x1A692C11`). Unless attribute 5 (set by `Handle Reset`) is 1, it sets attribute 1, the step, to -1, and attribute 7 to 0.
- **`Function - Main` (0x1000):**
  1. `idle`, then `CheckObject`, then `Get Sim in Temp 0`.
  2. Tutorial flag 0 (`House Complete`) set: the tutorial is over, and it kills itself (a `KillObject`/`idle` loop).
  3. Otherwise it goes on only if global 0x1A or global 0xA is 1; if neither is, it kills itself.
  4. **It waits (`idle`) for unlock 26, "Start Tutorial"**, from `Intro Camera`.
  5. Then it calls `Tutorial - Tutorial House` again and again: **a false result calls it again in the same tick** (node 1's false target is node 1), and true goes to `Tutorial - End`.
- **`Tutorial - Tutorial House` (0x1001):**
  - **Node 0:** if unlock 1 ("Basic 1 - Environment") is set, return true, which ends the tutorial.
  - **Otherwise** it selects the step's tree by attribute 1, using constant table 0x12C.
  - **When the step's tree returns true**, it adds 1 to attribute 1 and returns false, so the next step starts in the same tick.
  - **When the step's tree returns false** (its false target 255), it returns false without moving on, so the same step runs again.

| Step | Tree | Reached from node 0 | Waits on |
|---|---|---|---|
| 0 | `Lock Everything` (0x1002) | Selected Sim's motives 9, 6, 5, 15, 7, 8, 14, 13 = 100. Then a neighbour's (`Get in Temp 0 - Neighbor's instance ID`) = 80, `FindGoodLocation` and a `PushAction` on it. Marks every object of five GUIDs, from SetToNext type 4: start marker attribute 0, and others' attributes 0x12, 8, 3, 0xD | Nothing |
| 1 | `Basic - Start` (0x1023) | If unlock 2 is set, true. Otherwise `Idle With Motive Floor` for constant 0x1001[2] × 24 ticks | That idle |
| 2 | `Basic - Find Survival Book` (0x1005) | **Only node 0**: CompleteGoal op 3 on unlock 2, true. The book-finding nodes can't be reached | Nothing |
| 3 | `Basic - Read Survival Book` (0x1006) | Unlock 2 set: modal Dialog 4 (type 4, yes and no). Button 1 sets unlock 1, which ends the tutorial on the next pass. Then true. Without unlock 2: `Sub - Create Survival Book` and `sub - Force Read Tutorial Book` (a PushAction) | The Dialog |
| 4 | `Basic - Teach Navigation` (0x1036) | PS2: **modeless** Dialog 5 (type 1, it doesn't wait), `Tutorial - Wait for Complete Idle Loop` (either result), GenericSimCall 0x2C (close modeless dialogs), true. The seven Dialogs are Wii's path | The wait |
| 5 | `Survival Basic 2: Gather Supplies` (0x1007) | **`sub - Create Coconut`**, then modal Dialog 7 (type 3; 0x20 on Wii), true | The Dialog |
| 6 | `Basic - Learn Harvesting` (0x102A) | **Modeless** Dialog 9 (type 1; 0x20 on Wii), then `Tutorial - Wait for Complete Idle Loop`. Done: `Idle With Motive Floor`, true. Timed out: **false**, so step 6 runs again | A harvest's `Task Complete` |
| 7 | `Basic - Inventory` (0x102E) | **Modeless** Dialog 0xA (type 1; 6 on Wii), `Tutorial - Idle - No input` (0x1011), true. **Nothing waits for the inventory screen**: from node 0, 0x1011 only sets t0 = constant 0x1001[4] (20, "Dialog Wait - No Reminder (seconds)") × 24 = 480 ticks, runs `Idle With Motive Floor` for that, and returns true. Its attribute-6 loop can't be reached | A fixed 480 ticks (about 20 s at 1×) |
| 8 | `Basic - Pause Menu` (0x1008) | **Only node 0**: `idle`, true | One idle |
| 9 | `Basic - Inventory Screen` (0x100A) | **Only node 0**: `idle`, true | One idle |
| 10 | `Survival Basic 3: eat` (0x1009) | **`sub - Create Coconut`**, modal Dialog 0x18 (type 3; 2 on Wii), **unlock 10, hunger**, true | The Dialog |
| 11 | `Basic - Eat` (0x1046) | `Get Sim in Temp 0`. If the Sim holds an object of one of five food GUIDs (coconut among them; TestObjectType on the object in its slot 0), it goes straight to `Tutorial - Wait for Complete Idle Loop`; otherwise **modeless** Dialog 0x19 (type 1) first. Done: true. Timed out: false, so step 11 runs again. Node 1's Dialog 0x16 can't be reached | Eating's `Task Complete` |

**How a step waits.** `Tutorial - Wait for Complete Idle Loop` (0x103C) and `Tutorial - Idle - No input` (0x1011) loop through `Idle With Motive Floor`, a countdown of 15-tick idles from constant 0x1001[3] or [4], doubled.

- **Done:** they stop when the tutorial's **attribute 6 is 1**. `Tutorial - Task Complete` (global 0x1F0) sets it on the tutorial object whose attribute 1 equals its parameter 0. Then GenericSimCall 0x2C closes modeless dialogs, attribute 6 = 0, and they return true.
- **Timed out:** false.
- **Who calls `Task Complete`:**
  - `Intro Camera`, with 2;
  - eating (`resources_sg` `Interaction - Eat`), putting a resource in the inventory, harvesting (`source_sg` `Harvest - Core`), sleeping on the ground and placing the mat, and building and lighting the fire pit;
  - each survival book's catch-up;
  - all with the caller's default parameters (not read here).

**What only the real UI or camera can answer:**

- **Every modal Dialog waits for a button**: Intro Camera's and steps 3, 5 and 10. Read Survival Book's Dialog 4 also branches on it: button 1 skips the tutorial. Steps 4, 6, 7 and 11 show a modeless message (type 1) that doesn't wait; the step's wait loop does the waiting, and GenericSimCall 0x2C closes the message.
- **The CameraControl waits** in the Intro Sequence.
- **Steps 6 and 11 finish only when the player does the action,** which calls `Task Complete`. Otherwise they time out and run again.

### The objects the opening creates

**Proven on the disc.** There are three CreateObject trees on this path, and each creates **one object, only when SetToNext type 4 finds none of that GUID**:

| Tree | Object (GUID) | Place | Reached from |
|---|---|---|---|
| `Sub - Go to Start Position` | Start marker `0x50323DB5` | 6, then `FindGoodLocation` | The Intro Sequence, once |
| `sub - Create Coconut` | Coconut `0xA78125FE` | 6, then `FindGoodLocation` | Steps 5 and 10 |
| `Sub - Create Survival Book` | Survival book `0x1FC2477B` | 6, then `FindGoodLocation` | Step 3, only without unlock 2 |

**SetToNext type 4 walks every object ID** (`GetObject(index)` for all of `GetNumObjects`), placed or not, and matches the selector. It doesn't test whether the object is in the world: `IsInteractionTile` is only checked with skipping on, and these nodes have it off. So an object made out of the world and never placed is still found next time.

**In the game, the opening makes at most one marker, one coconut (until it's eaten) and one book.** Nothing on this path creates objects in a loop.

**What would make a port create without stopping.** These things repeat:

- **`Function - Main` calls `Tutorial House` again in the same tick** each time it returns false. That's after every finished step, and after every step that returns false.
- **The only things that end a tick** on this path are an Idle primitive (`idle`, `Idle With Motive Floor`), a Dialog waiting (4), Animate, IdleAnimate or CameraControl waiting (2), and a CreateObject whose object isn't loaded yet.
- **25,000 nodes in one tick is error 0x44D.** An error resets the tutorial object. It has `Handle Reset` (0x1033), which sets attribute 5 so that `Function - Init` keeps the step. Likely, since which entry point that tree is isn't checked here.

So a port gets a new coconut or book on every pass if both of these hold:

1. its SetToNext type 4 misses objects it made itself, for example ones still out of the world, or ones not in the ID list the search walks;
2. a step keeps running again without waiting, for example because a Dialog returns at once instead of 4, or a CompleteGoal test of unlock 2 reads false so step 3 takes its book path.

Stubbing IdleAnimate to return true lets the Intro Sequence finish. That sets scenario attribute 4, and `Intro Camera` then grants unlock 26, which is what starts the tutorial loop. So stubbing IdleAnimate is likely what first let the port reach the creating steps.

## Identify: the "identified" marker

**Proven on the disc for the trees; type attributes proven by the matched code.** Identify on a resource is `Interaction - Taste` (`resources_sg` 0x200A; `source_sg` has its own at 0x2002).

- Node 0 puts the stack object's semi-global attribute 0, its resource token ID, in temp 0.
- Node 6 sets **the stack object's type attribute 0 to 2000**. The stack object is the object being tasted, the coconut: the coconut's Taste makes no object at all. (The palm's own Taste, `source_sg` 0x2002, plays harvest clips and has a CreateObject at node 16.)
- **`Test - Is Resource Identified?`** (0x2007, parameter 0 the token ID) tests bit 10 of column 8 of that token's `InventoryTokens` row. Clear means the resource needs no identifying, and the test is true. Set means it does, and the test is true only if **the stack object's type attribute 0 is 2000**.

**Whose type attributes.** They belong to the object's **GUID**: scope 0x25 reads `ObjSelector::GetTypeAttributes`, which looks up the folder's block for the definition's GUID (`+0xB8` is 0 on every definition; see "Type attributes" below). Every object of that GUID shares one array, so tasting one coconut marks every coconut of that GUID as identified. Nothing is written to the token or its row.

**Why a port may see no marker.** A GUID only has a block if its definition says so. `ObjectFolder::AddSelector` (PS2 reads it at `0x21C98C`) makes an `ObjectTypeAttrBlock(GUID, count)` when the definition's **`+0xB4`** is non-zero, and `GetTypeAttrBlock` only searches the existing blocks. A GUID without one makes Taste's write error 0x40, and nothing is marked. On the disc, 143 of the 1,335 definitions have a count, 140 of them 1, 1 each 2, 8 and 9. A port should give each of those GUIDs an array of that many zeros when the object type loads.

## Harvest: where the resource comes from

**Proven on the disc, from the trees.** Two paths, chosen by the source's animation height (table 0x2000 of `source_sg`, "Animation Heights"):

**1. Grab One: an animation event.** `Test - Does Height Support GrabOne?` (0x201D) is true only for heights 7 (Harvest Underground), 9 (Harvest Tree - Ground), 10 (Harvest Tree Low) and 11 (Harvest Tree Med).

- `Sub - Play Harvest Anim` (0x201E) plays each stage's clip with its events going to local 0, and passes each event to `Sub - Handle Events` (0x201F) with the stage. The stages are the "Animation Indices" table 0x2004: 0 Start, 1 to 4 Loop 1 to 4, 5 Stop, 6 Grab One.
- In the Grab One stage, **event 101** runs `Events - Grab One` (0x203D). If the source's semi-global attribute 8 (which resource, 0 to 2; see "the accrual rate" below) is above 0, so not for the first resource, it takes the resource's token and GUID (`Get in Temp 1&2 - Resource GUID`, token columns 0xB and 0xA) and **CreateObject**s it.
- Event 101 in the Start stage creates the tool prop (`Events - Start`). Event 501 in the loops only plays sounds (`Events - Loop`). Event 102 at Stop puts the prop away (`Events - Stop`).

**2. Accrual: tree logic on a timer.** `Harvest - Core` (0x2007) calls `Sub - Start Resource Accrual` (0x2020) at node 1 and `Sub - Stop Resource Accrual` at node 14, so likely for every height. Start sets the source's semi-global attribute 8 to the resource index and 9 to the **harvesting Sim's neighbour ID** (its person data 0xC; this used to say 1), and notifies it; Stop clears them.

- While attribute 9 is set, the source's own `Function - Main SG` (0x2001) loop counts down from `Get in Temp 0 - Accrual Rate` (0x2026), which depends on the resource, the harvester's skill and tool; see "Harvest: the accrual rate".
- Each time the count runs out and the source's resource count can be decremented (`Sub - Decrement Resource Count`), it calls `Resource - Add To Inventory` (0x2004).
- That **adds one of the resource's token straight to the family inventory** (FamilyInventory op 0, token category, count 1), shows a balloon (icon group 0xD), updates the displayed count, runs `NotifyPlans`, and gives special resources a chance at extras (`Sub - Process Special Resources`).

So **climbing a palm** (heights 4 Knife High Climb, 12 Harvest Tree High Knife, 14 Knife Not-So-High Climb) yields only through accrual: tokens in the inventory over time, no object and no event. A low tree uses both: a fruit in hand on event 101 in the Grab One stage, and tokens over time.

## Harvest: the accrual rate

**Proven on the disc, from the trees; the skill numbers are likely (Wii).** This is how long the source's `Function - Main SG` waits between tokens while a Sim harvests it.

**The countdown** (`Function - Main SG`, 0x2001). While the source's semi-global attribute 9 is above 0, the loop calls global `idle` (0x118) with 1, likely one tick, and adds 1 to local 1. When local 0 is 0, it first sets local 0 = `Get in Temp 0 - Accrual Rate` and local 1 = 1. When local 1 reaches local 0, it runs `Sub - Decrement Resource Count` and, if that succeeds, `Resource - Add To Inventory`, and then sets local 0 back to 0. So **the rate is worked out again for every token**, and a skill gained mid-harvest counts from the next one.

**What it reads** (`Get in Temp 0 - Accrual Rate`, `source_sg` 0x2026), with the source as "me":

- The **resource index** is my semi-global attribute 8, 0 to 2 (`Get in Temp 0 - Resource ID by Index`, 0x2016, maps 0, 1, 2 to my semi-global attributes 0, 1, 2, the three token IDs).
- The **base time** is my private table 0x1000 ("Resource Tuning") at index 6 + resource index: "Resource 1/2/3 Base Ticks to Harvest".
- The **harvester** is my semi-global attribute 9: `Harvest - Core` (0x2007) passes **my person data 0xC, the Sim's neighbour ID**, as parameter 1 of `Sub - Start Resource Accrual`. (This corrects "9 to 1" in the section above.)
- The harvester's **skill**: if bit 3 of column 8 of the token's `InventoryTokens` row is set, neighbour data word 9 at 15% a point ("Time discount % per Food skill point"); otherwise neighbour data word 0x10 at 5% a point ("per Body skill point"). Wii's `PersistentData::IsSkill` treats person data 8 to 0x11 as skills, so both are skills; which is food and which body is from the tuning labels. A point is 100 of the stored value.
- The **tool**: `Get in Temp 0 - Current Tool Level` (0x2028) minus `Get in Temp 0 - Minimum Tool Level` (0x2027). The minimum is column 4 of the resource's token row. The current level is column 1 of the best tool token the family has for that resource's tool type (column 3), found by global `Get in Temp 0 - Best Tool Resource ID` (0x1F5); no tool is level 0.

**The sum**, all integer arithmetic, with the "Harvest/Resource Tuning" table 0x2003 of `source_sg`:

```
skill    = min((skill word / 100) × 15 or 5, 75)        ; 0x2003[1] or [0], cap [3]
extra    = current tool level − minimum tool level
tool     = extra >= 0 ? min(20 × extra, 40) : 20        ; [2], cap [4]
discount = min(skill + tool, 90)                        ; cap [5]
rate     = base × (100 − discount) / 100
```

The `: 20` is what the tree does: with a tool below the minimum, the per-level 20 is left unmultiplied. Likely nothing harvests then, but a port copying the tree gets the same number.

**Motives don't change it.** Nothing in 0x2026, 0x2027 or 0x2028 reads a motive. Only the harvester's skill and tool do. (Motives do change *during* a harvest, through `Harvest - Core`'s SetMotiveDelta nodes, but nothing feeds them back into the rate.)

**The coconut palm** (`source_tree_coconutpalm`), its Resource Tuning row:

| Index | Token | Max count | Base ticks |
|---|---|---|---|
| 0 | 174 | 5 | 35 |
| 1 | 152 | 4 | 40 |
| 2 | 140 | 5 | 90 |

Shed resource index 1, energy delta -60 an hour, stop at -90. So resource 0 with no skill and the minimum tool is a token every 35 ticks; at 15% a point, a Sim with 3 points takes 35 × 55 / 100 = 19; the 90% cap gives 3. Most trees use 30 to 40 base ticks for resource 1 and 90 for the others; palms use 40 for resource 2.

## Dialog (opcode 0x24)

**Proven on PS2:** `cXObject::TryDialog` (`0x2306E0`), its operand helper (`0x230658`), `EdithDialogPrimitive::SetDialog` (`0x1EDBC8`), `StringSet::Load` and `GetLocString` (`0x263A40`, `0x263848`), read call by call, with Wii for the names. The strings below were read from the disc. 505 uses.

**Correction.** This section used to say the dialog type is `+5` bits 4 to 7. That's the Wii's layout. The type is a 4-bit bitfield, and a big-endian compiler puts the first bitfield in a byte's high bits, a little-endian one in its low bits. The Wii reads `(+5 >> 4) & 0xF`, the PS2 **`+5 & 0xF`**, and the PS2 disc's data is laid out for the PS2: its 505 Dialogs have low nibbles 0 to 5 (51, 168, 21, 226, 37 and 2 of them), and 470 have high nibble 0. Nothing in TryDialog or SetDialog reads the high nibble. `+7`'s fields below are the PS2's.

### Operands

| Operand | Meaning |
|---|---|
| `+0` u8 | Third button's label (Cancel) |
| `+1` u8 | Icon number, for `+7` icon types 2 to 4 |
| `+2` u8 | **Message** |
| `+3` u8 | First button's label (OK or Yes). For types 1, 2 and 5, a line under the message |
| `+4` u8 | Second button's label (No) |
| `+5` bits 0-3 | **Type**, below |
| `+6` u8 | **Title** |
| `+7` bits 1-3 | Icon type: 0 and 1 none, 2 a neighbour's picture, 3 an object's thumbnail, 4 row `+1` of the `DialogIconTable` quickdata table. For 2 and 3, the neighbour or object ID is my local `+1` |
| `+7` bits 4-6 | The temp that gets the button pressed |
| `+7` bit 7 | Take strings from my semi-global row instead of my own |

**String numbers.** Every string operand is a number in `STR#` 0x12D, counting from 1.
- **0 means the default**: a button's built-in label, or nothing for the message and title.
- **0xFE means empty.**
- **0xFF means "from a temp".** Before anything else, TryDialog replaces a 0xFF in `+2` with temp 0 + 1, `+6` with temp 1 + 1, `+3` temp 2 + 1, `+4` temp 3 + 1 and `+0` temp 4 + 1. 16 Dialogs on the disc do this, all in `+2` or `+0`.
- A number past the set's end, or a string of length 0, also gives the default.

### Where the text comes from

1. **The string set is always `STR#` 0x12D** (301, the dialog strings). SetDialog asks the smart-data factory for a `StringSet` (type `0x6BC37BC5`) and loads ID 0x12D.
2. **Whose:** the namespace of the stack frame's `Behavior` (`StackElem +0x10`).
   - That's its `ObjSelector +4`, my own `ResFile` row, or `+8`, my semi-global row, when `+7` bit 7 is set.
   - Subroutine calls keep the caller's Behavior (`TreeSim::Gosub` copies it when it's given none). So a Dialog in a global or semi-global tree still reads the strings of the object whose tree started the stack.
   - For the tutorial's steps that's the `tutorial` row; for `Intro Camera`, `scenario_first_playable`.
3. **Where in the row:** `StringSet::Load` ignores the namespace it's handed. It asks the object folder (`0x21D8E8`) for the namespace's `ResFile` row: row number × 0x48 in the `ResFile` quickdata table, or the global row for the global namespace.
   - It then finds the ID in **the row's `+0x14` vector**. Its 12-byte entries are like `+0x10`'s: a `VECTOR` of string pointers, a tag, and the ID in the low 16 bits.
   - `+0x10` holds the other sets: 0x100 to 0x104, 0x12C, 0x12F and 0x1F5 to 0x1FC. **0x12D is only ever in `+0x14`.**
   - On the disc, `tutorial`'s `+0x14` holds just 0x12D, with 47 strings; `scenario_first_playable`'s holds 20.
4. **The string:** `GetLocString(n)` returns entry n − 1, or a null string for n ≤ 0 or past the end. An entry points into the **language image** (see world.md's QUICKDAT.ARC), and that slot points to the UTF-16 text, so a reader follows two pointers.
5. **The language: always 0.**
   - `ESimsApp::Init` (PS2, around `0x2FD510`) stores 0 at the quickdata manager's `+0xD38`, right after `InitManagerArcOffsets1`.
   - That's the Wii's `SetCurrentLanguage(GetDefaultLanguage())` inlined. The Wii's `GetDefaultLanguage` returns 0, and its manager keeps the language at `+0xD44`.
   - `EDatasetManager::AllocateAndLoadResource` (`0x4114F8`) passes `+0xD38` to every load, and no other code writes it.
   - This settles world.md's open question: **language image 0 is the US English text**.
6. **Tokens:** if the text isn't empty, `cXObject::ParseUIString` (`0x227200`) fills in its `$` tokens (below).
   - The `<COLOR_…>`, `<BUTTON_xx>` and `<FONT_…>` tags aren't tokens: `AptParagraph::Build` (`0x3045D8`) reads them when the text is laid out.
   - What each `<BUTTON_xx>` draws, and how far `<COLOR_>` reaches, are in ui.md, "Text markup".

### Buttons and types

**Default labels** come from the `NewUIStrings` quickdata table, by key (`EGlobal::GetNewUIString`): `ok` "OK", `yes` "Yes", `no` "No", `cancel` "Cancel". Type 6 and up shows the title "WARNING: DEPRECATED DIALOG" and no message.

| Type | Uses on the disc | Shown by (Wii names) | Buttons | TryDialog returns |
|---|---|---|---|---|
| 0 | 51 | `UIDialog::SpawnDialog`, modal | 1 (OK); 2 if `+4` isn't 0xFE; 3 if `+0` isn't 0xFE either. So `+4` and `+0` left at 0 give Yes, No and Cancel | **4 until answered** |
| 1 | 168 | `H2DTarget::SpawnModelessDialog` (the `MDI` screen, ui.md) | None. SetDialog builds a line from `+3`, but the modeless record doesn't copy it, so it's never shown | 1, at once |
| 2 | 21 | The same, with no timeout (`+0x18` = −1) | None | 1, at once |
| 3 | 226 | `UIDialog::SpawnDialog`, modal | 1: `+3`, default "OK" | **4 until answered** |
| 4 | 37 | `UIDialog::SpawnDialog`, modal | 2: `+3` default "Yes", `+4` default "No" | **4 until answered** |
| 5 | 2 | `H2DTarget::SpawnIntroThoughtBalloon`, timeout 10,000 | None | 1, at once |

**What TryDialog does**, in order:

1. Replace the 0xFF operands from temps.
2. **The player has answered** (the answer code at `0x4928A8` isn't 4):
   - the button pressed goes into the `+7` result temp;
   - the primitive returns the answer code, and the code goes back to 4;
   - the UI's `EndDialog(bool)` sets the code to 1 or 0, so OK or Yes takes the true branch and No the false.
3. A flow request is pending: return 1.
4. **Skipped dialogs.** It first looks for a family member who is awake and visible.
   - No one is, or the camera director's letterbox is up: type 4, and type 0 unless `+0` and `+3` are both 0xFE, set temp 0 to 0 and return 1.
   - Types 1, 2 and 5 return 1 when no one is.
5. Types 0 to 5 return 1 without showing anything when a global flag (`+0x37C` on the object at `0x50A6A8`) is set.
6. Otherwise SetDialog shows it.
   - Types 0, 3 and 4 **return 4**, and the node runs again every tick until the player answers (step 2).
   - Types 1, 2 and 5 return 1.
   - A SetDialog that fails returns 1.

**For a port:**
- Read the type as `+5 & 0xF`.
- Show types 0, 3 and 4 as a box with buttons that waits.
- Show types 1, 2 and 5 as a message that stays up while the tree carries on. It goes when GenericSimCall 0x2C (close modeless dialogs) runs or the player closes it.

### Tokens

`ParseUIString`'s tokens come from the PS2 string table at `0x4BA3E0`. What each becomes is read on Wii (`ParseUIString` and its helper `ParseOneString`), so it's likely.
- A token can be followed by `:N`, and then by `:M` or `,M`. These are decimal numbers, −1 when absent.
- "Local N" is the stack frame's local N.

| Token | Becomes |
|---|---|
| `$Me` | My name: the Sim's first name, or the object's catalogue name |
| `$Object` | The stack object's name, the same way |
| `$Neighbor` | A neighbour's name, from the neighbourhood's selector |
| `$NameLocal:N`, `$LastNameLocal:N` | The first or last name of the object whose ID is local N |
| `$Local:N` | Local N as a number |
| `$MoneyLocal:N`, `$PlayerFunds` | Local N, or the household's funds, as money |
| `$TokenNameLocal:N` | The name of inventory token local N |
| `$FoodName` | A food name, from two temps (`get_food_replacement_string`) |
| `$LocationName`, `$LocationNameLocal:N` | A lot's name from `HouseData`: the current one, or number local N |
| `$PlayerName` | The selected neighbour's name |
| `$TimeLocal` | A time of day (`GetTimeString`) |
| `$GS…` | A key's male or female form (`EGlobal::GetGenderString`), by whether the Sim is male |
| `$JobName`, `$JobDesc`, `$JobTrackName` | Placeholders, such as "Temp name for Job.  Add code support." |

None of the opening's dialog strings uses a token.

### The opening's dialogs

**Proven on the disc.** The text is exact; `\r\n` is the string's own line break.

| Tree, node | `+2` message, `+6` title, type | Text |
|---|---|---|
| `Intro Camera` (scenario) n11, one castaway | 6, title 5, type 3 | Title "Your Sim has shipwrecked!"; message "Learn to survive.\r\nFind your way home." |
| `Intro Camera` n10, more than one | 3, title 2, type 3 | Title "The Sims have shipwrecked!"; message "Find and unite your lost crew.\r\nHelp them get home." |
| Step 3 `Read Survival Book` n1 | 4, type 4 | "It's <COLOR_FF0B0077> Moneywell's\r\n<COLOR_FF0B0077> Big_Book_of_Survival! This will help your Sim survive. \r\n\r\nDo you want to read it and play the tutorial?" |
| Step 4 `Teach Navigation` n1 | 5, type 1 | "Survival Tip 1: Survey your Environment\r\n\r\nLook around using <BUTTON_04>. This will move the camera" |
| Step 5 `Gather Supplies` n2 | 7, type 3 | "Survival Tip 2: Gather Supplies. Nature is bountiful. Find and harvest anything you can on the beach or off nearby trees." |
| Step 6 `Learn Harvesting` n1 | 9, type 1 | "Gather supplies! Walk up to something and press <BUTTON_00>. If there is more than one action, use the <BUTTON_03>  to select one." |
| Step 7 `Inventory` n1 | 0xA, type 1 | "See what your sim has gathered. Press <BUTTON_05> and choose "Inventory"" |
| **Step 10 `Survival Basic 3: eat` n2** | **0x18, type 3** | **"Survival Tip 3: Eat! This is your Hunger motive. \r\n\r\nEating will keep it filled. If it reaches empty, the Sim will starve. Check your Hunger by pressing  <BUTTON_06> then find something to eat."** |
| Harvesting a clam (step 6 or later): `source_sg` 0x203F `Sub - Process Special Resources` n8, n11, n15, n19, n25 | 1 to 5, type 1, `+7` 0x80 (the clam's semi-global strings, `source_sg`'s own `+0x14` set) | A random find, one of "You have found gold inside one of the clams!", "…a pearl…", "…a piece of jade…", "…a moonstone…", each with a FamilyInventory add; and once, "You've Found a piece to a Treasure Map!" |
| Step 11 `Basic - Eat` n5 | 0x19, type 1 | "Eat something. Try walking up to a coconut and selecting EAT, or go to the Inventory and see what food you already have." |

- **Buttons.** All of these leave `+0`, `+3` and `+4` at 0, and `+6` too except for Intro Camera's titles. So the buttons are the defaults: OK for type 3, and Yes and No for step 3's type 4.
- **The Wii-path twins** (step 10 n4 string 2, step 5 n3 string 0x20 and so on) say "point the Wii_Remote" instead.
- **Later tips:** the tutorial's 47 strings also include them, for example sleep (0xC), the fire pit (0x10) and "Basic Survival Complete!" (0x14).

## SetBalloon (opcode 0x29)

**Read on Wii** (`cXObject::TrySetBalloon`, PS2 `0x2262B0`). Likely. 336 uses. It sets a sprite slot over me or the stack object (the balloon over a Sim's head) and **returns 1 at once**; it doesn't wait. Two error paths return -1. Operands:

| Operand | Meaning |
|---|---|
| `+0` bit 0 | Over the stack object, not me. Higher bits give a local number (Wii `GetLocalNum`) for showing that local's object |
| `+2` | Low byte the icon index, high byte the icon group (`IconGroup::GetSpriteID`) |
| `+4` | Likely the duration, in ticks or loops |
| `+6` | Low byte the balloon type (`IconGroup::GetBalloonSpriteID`). Bits 8 show when inactive, 9 the "not" sign over it, 10 reverse, 11 the duration is in loops, 12 offset by temp 0, 13 blink |

It can show the stack object's sprite, a neighbour's, a local's object's or an icon; the balloon then stays for the duration (`ActivateForTicks`, `ActivateForLoops`, or forever) and ages in `cXObject::Simulate`'s upkeep. `Sub - Add Resources`' balloon (`00 00 00 0d 1e 00 03 01`) is icon group 0xD index 0, type 3, 30 ticks, shown when inactive.

## Routing

What's here comes from the PS2 code where it gives an address, and the names come from the Wii build. **Proven** means read on PS2 or tested on the disc. **Likely** means read on Wii only, with PS2's function found by its calls but not compared line by line. Below `TryGotoRoutingSlot` and `TryGotoRelative`, 19 of the 20 handlers match byte for byte: all but `WaitingForStand`, which is parked. So do `GotoRoutingSlot` itself (`0x1AA008`) and `XRoute`'s constructor, copy constructor and `Construct`; `vector<XRoute>::DoInsertValue` (`0x1D16D0`) is not matched yet. PS2 emits most of the short ones together after `Routing_Failed`, in Wii's order. `RoutingSlot`'s small methods match too: `WriteSlot`, `AllowDirection`, `SetFacingDirection`, `SetDistances`, `SetTileDistances`, `GetXOffset` and `GetYOffset`. Its constructor isn't: it writes a vtable at `0x4B7FC0`, and the name in `symbols.txt` is on `0x4B8020`, which needs sorting out with the `Slot` base class first.

| Function | PS2 | Wii |
|---|---|---|
| `cXPerson::GotoRoutingSlot` | `0x1AA008` | `800C57xx` |
| `Routing_Initial` | `0x1AA6D0` | `800C5D08` |
| `Routing_WaitingForPortal` | `0x1AA980` | `800C5F78` |
| `Routing_Walking` | `0x1AAAC8` | `800C60DC` |
| `Routing_WaitingForStand` | `0x1AABE0` | |
| `Routing_Trapped` | `0x1AAC70` | `800C6320` |
| `Routing_WaitingForSit` | `0x1DC6A0` | |
| `Routing_Failed` | `0x1DC708` | `800C64F4` |
| `Routing_WalkingSucceeded`, `WalkingFailed`, `WalkTerminated` | `0x1DC778`, `0x1DC7C8`, `0x1DC818` | |
| `Routing_FoundPath`, `RouteInvalidatedWhileWalking` | `0x1DC868`, `0x1DC8D0` | |
| `Routing_WaitingForFailureFeedback`, `ObjectDeleted`, `ObjectDeletedWhileWalking` | `0x1DC920`, `0x1DC960`, `0x1DC970` | |
| `Routing_Succeeded` | `0x1AAE78` | `800C657C` |
| `Routing_Waiting` | `0x1AB150` | `800C6900` |
| `Routing_WaitingForPersonToMove` | `0x1AB278` | `800C6A38` |
| `Routing_NeedFailureFeedback` | `0x1AB450` | `800C6CB0` |
| `Routing_FoundGoals` | `0x1AB5B0` | `800C6E68` |
| `CleanUpAfterFinishingRoute` | `0x1AB758` | `800C7000` |
| `AskOthersToMove` | `0x1AB988` | `800C71C0` |
| `MoveOutOfWay` | `0x1AC038`, `0x1AC2A0` | `800C78EC` |
| `GetCurrentRoute` | `0x1DCA20` | `800C7E5C` |
| `InitRoute` | `0x1ACA30` | `800C7EA4` |
| `TILEROUTABLE` | `0x1CA948` | `800EB4A0` |
| `BuildRoomPartition` | `0x1CB3B4` | `800EC1CC` |
| `XRoute::BuildGoalList` | `0x1CC218` | `800EC920` |
| `XRoute::FindPath` | `0x1CC350` | `800ECA04` |
| `XRoute::ChooseStartingPoint` | `0x1CD978` | `800ED7C8` |
| `XRoute::ConstructGoals` | `0x1CDBE0` | `800ED934` |
| `XRoute::EvalTileForGoal` | `0x1CAF08` | `800EBA30` |

### What a route reads

| Data | From | Used for |
|---|---|---|
| Routing slots | `ResFile +0x28`, below | Where to stand. Global ones are set 100 in the `global` row |
| Tile flags | `Arry`, bit 0x10 | `TILEROUTABLE`: set means no. **Proven on PS2** |
| Room ID per tile | `cFixedWorld::GetRoom`, worked out by `ComputeRooms` from walls | A tile in another room isn't routable, unless its ID is 0xFFFB (a split diagonal), or a portal on it leads to this room |
| Walls | `Arry`: `HasWall`, `HasDiagonal`, style | `BuildRoomPartition` makes rectangles of them. Door-like styles let a Sim through. On the disc, lots have one wall edge at most |
| Portals | Objects of type 8 on the tile | Entry point 15 and its other side join rooms in `TILEROUTABLE` |
| Objects | The object list: `GetRect` and footprint, variable 0xA, `GetWallBlockFlags` (variable 0x35 turned by variable 1) | Penalty rectangles for the path, and obstacles for the goals |
| Other Sims | The object list, by `ShouldIgnore` | Blocking, unless ignored |
| Sim's width | `GetPersonWidth` | How far rectangles grow |

Heights and the terrain mesh aren't read here. Levels only matter through the tile's level.

**Routing slots. Proven on the disc.** `ResFile +0x28` is a VECTOR of 8-byte entries: a pointer to a VECTOR of 0x48-byte slot descriptors, then the set's ID. An object's set is definition `+0x28` as s16 (`cXObject::Initialize`, `0x20F258`): 0 is none, below 1000 is in its own row, and 1000 or more in its semi-global row. 1,011 definitions find their set this way, and the control word finds none. 59 name a set that isn't there, mostly test objects, and `SlotLoader` then loads nothing. `tools/bhav.py` reruns this, and `read_slots` returns them.

The first s16 of a descriptor is its type. Type 0 becomes an `ObjectSlot`, 1 a `SpriteSlot` and 3 a `RoutingSlot`. The disc has 178, 9 and 1,413. Fields, from `RoutingSlot::WriteSlot` (`0x1E1658`):

| Descriptor | `RoutingSlot` | Meaning |
|---|---|---|
| `+0x04`, `+0x08`, `+0x0C` f32 | `+0x04` | Offset from the object: x, y, z |
| `+0x1C`, `+0x20`, `+0x24` s32 | `+0x10` | Multipliers, one for each vertical position |
| `+0x28` s32 | `+0x1C` | Flags, below |
| `+0x2C` s32 | `+0x20` | Object slot to snap to, -1 none |
| `+0x30` s32 | `+0x24` | Minimum distance, in 1/16 tile |
| `+0x34` s32 | `+0x28` | Maximum distance |
| `+0x38` s32 | `+0x2C` | Optimal distance |
| `+0x3C` f32 | `+0x30` | Gradient, how fast the score falls away from optimal. 0.1875 by default |
| `+0x40` s32 | `+0x34` | Facing: 0 to 7, -2 towards the object, -3 any |
| `+0x44` s32 | `+0x38` | Resolution, in 1/16 tile. 16 by default |

**A slot's offset depends on the Sim. Proven: `GetXOffset` and `GetYOffset` match.** `RoutingSlot` `+0x34`, the facing, picks an extra offset by sim type, and both functions add it to the slot's own x or y. `kStraightOffset` is at `0x4B66E0`, in 1/16 tile: 0, 0, 2.944, 6.08, 2.944, 0 for sim types 0 to 5. `kCornerOffset` at `0x510568` is worked out by a static initialiser (`0x1D8400`): entries 0 and 1 are 0, entries 2 to 4 are `sqrt(pow(straight, 2) * 0.5)`, and entry 5 stays 0. So that's the straight offset divided by the square root of 2: 0, 0, 2.0817, 4.2992, 2.0817, 0.

| Facing | Added to x | Added to y |
|---|---|---|
| 0 | Nothing | Straight |
| 1 | -Corner | Corner |
| 2 | -Straight | Nothing |
| 3 | -Corner | -Corner |
| 4 | Nothing | -Straight |
| 5 | Corner | -Corner |
| 6 | Straight | Nothing |
| 7 | Corner | Corner |
| Below 0 | Nothing | Nothing |

Flags: the low byte is the allowed directions, one bit each, and none means on top of the object. Bit 11 ignores rooms (a ghost's route sets it). Bit 9 ignores the target's direction, bit 13 is random scoring, bit 16 uses the object's average location. Bit 14 allows failure trees, bit 15 a different level, and bit 17 marks a TS2 slot.

### The route

`XRoute`, 0xA8 bytes on PS2, lives in a vector at Sim `+0x5D0`, and the last one is current. The path is a vector of 8-byte tile points at Sim `+0x59C`.

| Offset | Meaning |
|---|---|
| `+0x00` | Goals, a vector of 16-byte `RouteGoal`s: `+0x0C` s16 object ID, `+0x0E` s16 direction, which goes to person data 0x12 |
| `+0x10` | A copy of the routing slot, 0x3C bytes |
| `+0x4C` | Target object |
| `+0x50` | Me |
| `+0x54` | Current goal, -1 for none |
| `+0x58` | Best score, -1 for none |
| `+0x70` | Start tile |
| `+0x78` | Exit direction flag |
| `+0x8C` | Result |
| `+0x90` | ID of the object in the way |
| `+0xA0` | Goal limit |
| `+0x6C` | Current portal |
| `+0x7C` | Valid. Clear means the route was invalidated: walking restarts through state 0xF |

**`GotoRoutingSlot`, proven on PS2.** With state 0 or 0x15:

1. A TS2 slot on a multi-tile object looks for a part with definition `+0x3A` above 0, and uses that part as the target.
2. No target, or one out of the world: 0 at once, the false branch.
3. It builds the `XRoute`. A ghost gets bit 11 on its slot.
4. If other routes are queued, the new one takes on a cleared failure-tree bit from the first.
5. With no routes queued and variable 0x11 clear, a start equal to the destination with a slot facing the object returns 1 at once.
6. It pushes the route, and the state becomes 1.

Then each tick, with any state: no current route is 0, and a route with no destination pops itself and returns 2. A target gone from the world moves to state 0x13, or 0x14 from Walking. An invalid route goes to state 0xF if it was walking, after which it's valid again. Then the state loop runs as in GotoRoutingSlot's section.

**Finding a path. Proven on PS2, read line by line, except the inside of `Path`, which is compared at call level.** The steps below are the PS2 code.

**Corrections to earlier versions of this section. Read these first:**

- **`EvalTileForGoal` returns 5 for a usable tile, not 0.** 0 means out of bounds or a placement error, 1 an object, 2 another room, 3 a wall, and 4 a Sim.
- **Rooms aren't optional.** `FindPath` gives up unless the Sim's room ID is below 0xFFFB and has a `Room`, and unless the target is in the same room (ghosts skip the second test).
- **0xFFFB isn't "outside".** It's a diagonal tile holding two rooms. An open lot is one real room, **ID 0**, covering every tile. See Rooms, below.
- **Penalties:** an object's rectangle costs `INT_MAX`, or 10 if its variable 0xA is set. 10,000 is only for portal walls, and for a solid object overlapping the Sim. 1000 is only for a Sim overlapping the Sim.
- **`+0xA0` is the goal limit, not a radius:** 48, or 8 in a room above 250 tiles.
- **Random scoring adds -10 to +9**, not up to 20.
- **`+0x4C` is the target and `+0x50` the Sim.** They were the wrong way round.
- **The ring of tiles at the lot's edge is out of bounds:** x and y must be above 0 and below size - 1.

Wii differs from PS2 in only two ways here. Wii takes the square root with `fastsqrtf` where PS2 uses `sqrt.s`, which can change a score by 1. And Wii calls `SetPersonData` where PS2 writes person data 0x21 directly. Offsets from `+0x98` up are 4 higher on PS2.

1. **`BuildGoalList`** (`0x1CC218`). `ChooseStartingPoint`, then the goal limit. Person data 0x21 is set to `+0x9C` while `ConstructGoals` runs, then cleared, and my direction is put back afterwards.
2. **`ChooseStartingPoint`** (`0x1CD978`). The start is my tile, with exit flag 0. If I'm in a chair (a container with tree entry 0x1A), it tries each direction in definition `+0x84`, turned by the chair's direction, one tile out, and keeps the tile nearest the target that scores 4 or 5.
3. **`ConstructGoals`** (`0x1CDBE0`). The base direction is the target's, or 0 with slot flag bit 9. The origin is the target's tile, or its average location with bit 16.
   - **On top** (no direction bits): one spot at the slot's offset. If it's on another whole tile, the direction to it is passed to the wall test. A 5 adds a goal scored 100. Otherwise the blockers give the result.
   - **Otherwise** distances must fit: minimum 0 to 512, maximum 1 to 512, and minimum, optimal and maximum in order. If not, there are no goals and no result.
   - The step is 16 if the same-level multiplier (`+0x14`) is non-zero, or else the slot's resolution.
   - A wall on the target's tile in direction n blocks direction n and marks n-1, n and n+1 for scoring.
   - For each allowed direction, base + n: a blocked one counts as a wall and is skipped. Otherwise it walks rows from the minimum distance out to the maximum. A row is y, in steps, and x runs from -y to +y, so each direction covers a 90-degree wedge. Offsets are turned by `TransformToWorldCoords`.
   - Each spot is tried up to 10 times: `fp` from -1 to 8. -1 or 8 is standing on the spot, and which one counts depends on the multipliers. 0 to 7 look for a chair next to it, facing it, when `+0x14` is non-zero.
   - A spot must be from minimum to maximum distance away (squared values compared). Its score is 100 - (int)(|(int)sqrt(x² + y²) - optimal| x gradient) + multiplier x 10. Random scoring adds rand % 20 - 10. A direction not marked by a wall adds 25, and the Sim's start tile adds 20.
   - `EvalTileForGoal` with the direction. 5 adds the goal unless slot flag bit 13 finds a door there. 3 ends this direction after its row. Others try the next `fp`.
   - Above the goal limit, chair goals stay and the rest are picked at random.
   - No goals: the result is 9 if a chair was taken, 0xD for a Sim, 8 for an object, 0xB for a wall or room, 0xA if the blockers were built for level-only slots, and otherwise 0.
4. **`EvalTileForGoal`** (`0x1CAF08`). Out of bounds is 0. Unless the slot ignores rooms: a tile in another room is 2, and `GetWallBlockFlagsAtTile` for direction + 4 gives 3 on bit 0, or for a diagonal on 0x82. Then the Sim's `CanPlace` (slot `+0x130`) with `gPlacementConflict` cleared: true is 5. False depends on `gPlacementError`: 7 is 3, 0xB is a conflict, and anything else 0. A conflict with no object or with the goal object is 5. Otherwise the ID goes in `+0x90`, and it's 4 for a Sim and 1 for anything else.
5. **`FindPath`** (`0x1CC350`).
   - No goals: result 1 becomes 7, and it returns 0.
   - My room must be the target's unless I'm a ghost, and it must have a `Room`. If not, it clears the current goal and returns 0.
   - Each other object in my room that `ShouldIgnore` doesn't skip makes a rectangle. Portals add their walls. A Sim's grows by half my width, and an `INT_MAX` rectangle by half my width.
   - A rectangle holding my start tile isn't used as it is. A Sim's or an `INT_MAX` one shrinks back. If that no longer touches my own rectangle, it's kept, and that object is ignored from then on (Sim `+0x5E0`). If it does touch, it's kept at 1000 for a Sim or 10,000 otherwise, grown again, and ignored too. A rectangle of penalty 10 or 10,000 holding the start tile is dropped.
   - `CollapsePenaltyRects` merges them. The room's partition is built if it's empty.
   - Goals become 1-tile rectangles, with penalty (best score - score) x 16. Goals with a score of 0 or less aren't used.
   - The iteration limit is the smaller of the caller's limit and goal count x area + 50.
   - In a room above 256 tiles, a first pass without smoothing tests that a path exists: none gives result 3 and returns 0.
6. **`Path`.** Proven on PS2 for the stages, which `FindPath` has inlined: state 1 runs `OpenANode` (`0x1BC8A0`) until it's done, then state 2 runs `DoOneSmooth` (`0x1BD0B8`) if smoothing is on, and state 0 is finished. Each step counts against the limit. **Compared with PS2 at the level of calls and fields, with no difference found, not line by line.** `OpenANode` uses the same sequence and node fields on both: `+0x1C` parent, `+0x20` f, `+0x24` g, `+0x28` h, with an open and a closed list. `BuildRoomPartition` and `SpacePartition::Init` make the same calls in the same order, and PS2 has `IsDoorlikeStyle` inlined. It's A* over the nodes of a `SpacePartition`, which are rectangles, not tiles: `BuildSpatialSuccessorList` grows free rectangles with `ExpandRect` on demand, inside the room's partition and clear of penalty rectangles. `DoOneSmooth` moves each point along the edge between two rectangles to shorten the path, 100 at a time. The estimate to a goal is the straight distance from the node's entry point to the centre of the goal's rectangle. A step between rectangles costs the straight distance from the entry point to `FindInterfacePoint`. Stepping into a goal costs that goal's penalty, so a poorer goal is only taken if it's nearer by more than the score gap x 16. When the open list empties, the path is read back through the parents as interface points, and the goal's centre is added if smoothing is off.
7. **`CanPlace` (`0x253938`) for a Sim.** It builds a `PlacementSpec` and calls `TestPlace` (`0x2104C8`). **Proven on PS2** for this first part, where a failure sets `gPlacementError`:
   - Out of bounds: 1.
   - The tile has flag 0x10, I'm not a ghost and Sim `+0x120` is clear: I'm allowed on if I'm standing on a 0x10 tile already. Otherwise chimps and boars are refused, any Sim but the player (`0x514EA4`) is refused, and the player needs `CollisionMap::IsLocationGood` (`0x3157A8`) to pass. A refusal is 1, and so is my own tile being out of bounds.
   - The tile has flag 0x80, and I'm a chimp or a boar: refused unless my own tile has 0x80 or is out of bounds.
   - Level outside 1 to 3: 2.

   **Proven on PS2 for the rest, compared with Wii in order.** 0xB for a roof or a slot conflict, 0x1B and 0x1C for walls, 0x13 if variable 0x2A bit 0 is clear and the tile isn't grass, 0x14 if bit 1 is clear and it is on level 1, 9 and 0xA from variable 0x1E (must be outside or inside), then `TestIntersection`. With nothing to sit on, height 1 is used, and variable 4 needs that bit, or it's 0x18. Placing onto a container adds slot tests, a weight test (its variable 0x1C against my 0x1B, error 0xE), and a loop check through containers (0xB). No behaviour differs from Wii: PS2 reads the two weight variables the other way round and has `GetContainer` and `GetHeightMask` inlined.

   `TestIntersection` (`0x211C50`), compared at call level, returns true at once for a ghost. Otherwise it looks at objects on the 3 x 3 tiles around, testing rectangles, and a Sim only clashes with a Sim it isn't ignoring.

### Rooms

**Proven on PS2** (`cFixedWorld::ComputeRooms`, `0x1C64E8`). `RoomManager::ComputeRooms` only gathers statistics.

1. Every tile's room starts at 0xFFFF, and tile flag bits 1 and 2 are cleared.
2. The border ring gets room 0 and flag bit 2.
3. Rooms are numbered from 0. It scans x from 1 to width - 2 and y from 1 to height - 2. Each tile not yet in a room starts a flood fill with the next ID, stopped by walls.
4. Diagonal tiles use 0xFFFE, 0xFFFD and 0xFFFC while filling. A tile that ends up with a room on each side is 0xFFFB.

So on a lot with no closed walls the fill from (1, 1) reaches every tile, and everything, the border too, is room 0. `RoomManager` has a `Room` for 0, because only IDs above 0xFFFA are refused. One wall edge can't close anything off. Objects copy their tile's room into variable 0x1D in `SetLocation`.

**For a port:** treat a lot with no closed walls as one room, ID 0, with a `Room`. Set variable 0x1D to 0 on everything, and it passes every room test above. Then the goals are the slot's wedge search, and a plain A* around object rectangles should give the same result on open ground. `CanPlace` decides the last step, and for a Sim on open ground the tests that matter are tile flag 0x10, grass (variable 0x2A) and rectangles.

## GotoRelative (opcode 0x1B)

**Proven** for the primitive: `cXPerson::TryGotoRelative` (`0x1AD320`) matches the game byte for byte. 157 uses. It's Sim-only. It builds a routing slot around the stack object, then runs the same route as GotoRoutingSlot, so that section's states and results apply.

| Operand | Meaning |
|---|---|
| `+2` s8 | Where, below |
| `+3` s8 | Facing: -2 towards the object, -1 any, 0 to 7 a direction. Anything else is **false** |
| `+6` bit 1 | Clear means failure trees may run |
| `+6` bit 2 | Allow a different level |

| `+2` | Slot |
|---|---|
| -2 | On top of the object |
| -1, and anything past 7 | All of 0, 2, 4, 6, from 1 to 2 tiles away |
| 0, 2, 4, 6 | That direction, 1 tile away |
| 1, 3, 5, 7 | The two directions either side: 0 and 2, 2 and 4, 4 and 6, 6 and 0 |

The Sim's model not being loaded returns 2. With frame `+8` set, the route already under way carries on.

## IdleForInput (opcode 0x11)

**Proven**: `cXPerson::TryIdleForInput` (`0x1AEEC0`) matches the game byte for byte. 3 uses, Sim-only. Its callee `executeInterruptOnIdlePrimitve` isn't read.

`+0` s16 is the parameter holding a countdown, and past the frame's count is error 8. `+2` s16 non-zero allows the interruption.

1. If `ShouldInterrupt` says so, it returns what `executeInterruptOnIdlePrimitve` does.
2. Otherwise, with the countdown at 0, it clears bit 6 of variable 8 and returns 1.
3. Otherwise it takes 1 off the countdown and returns 2.

NotifyStackObject ends the wait by setting the countdown to 0.

## MakeActionString (opcode 0x32)

**Proven in all but two swapped words**: the C++ for `0x2284F8` is parked under `NON_MATCHING`, 162 of 164. 320 uses.

| Operand | Meaning |
|---|---|
| `+0` s16 | `STR#` set, in the running tree's private namespace |
| `+2` bit 0 | The string number comes from `+5` scope and `+6` data, plus 1. Clear: it's `+4` |
| `+4` u8 | String number, counting from 1 |

It does nothing, and returns 1, unless a menu is being built or this is the main simulation. A string number below 0 or past the set's count is error 0x62. `ParseUIString` fills in the string's tokens. Then:

- **Building a menu:** `ObjTestSim::MakeNewMenuItem` adds the text, with variable 0x32 and the first object the string named, or the stack object if it named none.
- **Main simulation:** on a Sim, it becomes the current action's text.

It returns 1 in every case but the error, so a port with no menus can return 1.

## Snap (opcode 0x2E)

**Proven for the flow**, compared with PS2 call by call (`0x238B88`, helper `0x2396E0`). Not matched: it's 4,416 bytes with its helper, and `TrySnap` is written out in `cXObjectPrims.cpp`, parked at 610 of 726 words on one register choice. The helper isn't written. 115 uses.

It moves me to a place relative to the stack object, which must exist or it's error 0x15.

| Operand | Meaning |
|---|---|
| `+0` s16 | The slot's number, or the parameter holding it |
| `+2` s16 | Where, below. Anything else is error 0x24 |
| `+4` bit 0 | Origin only: keep the offset between me and my first tile |
| `+4` bit 1 | Ask a Sim in the way to move |
| `+4` bit 2 | Use the footprint |

| `+2` | Place |
|---|---|
| 0 | Routing slot in the stack object's list, its number in parameter `+0`. A bad parameter is error 8, a bad slot error 0x1B |
| 1 | The stack object's tile, keeping my direction |
| 2 | One tile ahead of the stack object, keeping my direction |
| 3 | Routing slot `+0` |
| 4 | Global routing slot `+0` |

A slot can give a direction and a facing, added to the stack object's direction, and it can name an object slot to snap to: a missing one is error 0x20.

PS2 does the same thing as Wii, with these inlined or different:

- Place 2 adds one tile by the stack object's direction, in 1/16 tile, from a jump table: direction 0 is y-1, 1 is x+1 and y-1, 2 is x+1, 3 is x+1 and y+1, 4 is y+1, 5 is x-1 and y+1, 6 is x-1, and 7 is x-1 and y-1.
- `SnapsToDirection` is slot flag 0x1000, and `GetSnapDirection` is the lowest set bit of the flags, below bit 7. It's added to the stack object's direction, and the sum is masked to 0 to 7.
- `GetObjectSlot` is inline. Entries are 0x24 bytes at `+0xF0`, and entry 0 isn't used: slot n is entry n + 1.
- A TS2 object slot is flag bit 17 of the object slot. For a Sim, its offset is turned by the object's direction (`0x2622F8`, `0x2620E8`, and a matrix multiply written out in full), then added to the tile.
- A TS2 routing slot on an object with a master first moves to that object's lead part, as GotoRoutingSlot does (slots `+0x18` and `+0x1D8`).
- The slot's offset needs the Sim's type: `GetSimType` (`0x1DD428`), then `GetYOffset` and `GetXOffset` (`0x1E1760`, `0x1E17F0`).
- Ignore rooms is slot flag 0x800.

Afterwards variable 0x34 says how it went: 0 by default, 1 if a Sim was asked to move, 2 if the place was out of bounds. Variable 0x36 is the ID of the object in the way, or 0. It returns whether the placement worked.

## FindGoodLocation (opcode 0x10)

**Proven: read line by line on PS2** (`TryFindGoodLocation` `0x224D60`, `cXObject::FindGoodLocation` `0x2139D8`). No difference from Wii was found, apart from inlined helpers. Not matched. 219 uses.

It places the **stack object**, which must exist or it's error 0x15. It returns true if it was placed, false if not.

| Operand | Meaning |
|---|---|
| `+0` u8 | Mode, below |
| `+1` u8 | Local holding the relative object's ID. A bad index is error 0x34, a missing object 0x17 |
| `+2` bit 0 | Start at the relative object. Without it, the start is tile (1, 1), level 1, and there's no relative object |
| `+2` bit 1 | Don't prefer empty tiles |
| `+2` bit 2 | Only tiles that may be edited: tile flag 0x20 **clear**. (This row used to say the flag was needed; it's the other way round, proven below) |
| `+2` bit 3 | The object's direction comes from temp 0. Anything but a Sim loses bit 0 of it |
| `+2` bit 4 | Prefer under a roof: placement tests also want a roof, error 0xA without |

| Mode | What it does | Fails with |
|---|---|---|
| 0, 9 and up | **Search out from the start**, no direction. If that fails and bit 4 was set, it tries again without it, then again with stay-in-room off | False |
| 1 | **Take it out of the world:** `Pickup`, after leaving a dynamic multi-tile object. Tile and level are given back. For a Sim, its sprite slot is deactivated, and the player's menu closes | Never |
| 2 | **Same tile as the relative object, or me:** the object turns to that one's direction + 5, then `CanPlace` and `Place` there | False, or error 0x52 if it didn't turn |
| 3 | **Search along the relative object's direction** | 0x3D with no relative object |
| 4 | **The same, direction + 2** | 0x3D |
| 5 | **Multi-tile link:** the object turns to match the relative object, and goes where the two leaders' part offsets (definition `+0x16`) say | 0x3D, 0x15 for no leader |
| 6 | **Random:** 10 tries at random tiles, below | False |
| 7 | **Search along temp 1 as the direction**, modulo 8 | 0x3D |
| 8 | **Multi-tile, every turn:** it tries directions 0, 2, 4 and 6 and keeps the nearest good tile. It tries with bit 4 if set, then without, then with stay-in-room off | False |

Directions must be even or -1, or it's error 0x5F. Modes 3, 4 and 7 use `CanPlace` again after the search, then `Place`. After a good placement the function writes back an action (temps aren't touched): 1 for mode 2, 2 for mode 1, 3 for mode 0, 4 for modes 3, 4, 6, 7 and 8, 5 for mode 5.

**The search** (`FindGoodLocation`). It starts at the given tile, or the cursor without one, kept to 1 and size - 2, on level 1 if none is given. Stay in room is on, and it's the start tile's room. From offset (0, 0) it tests each tile:

1. Out of bounds: not good, and it counts. It stops once (width + height) x 4 tiles in a row are out of bounds. Going past -0x40 on either axis also stops it.
2. Editable only: tile flag 0x20 must be **clear** (`0x1E47C0`: editable is `!(flags & 0x20)`, or every tile when the object at `0x50A6A8` has `+0x414` set). **Proven, PS2.** This used to say the flag was needed.
3. Prefer empty: a tile with an object isn't good, unless the object is type 5, type 0xE, or a type 8 without entry point 15.
4. A tile with a diagonal wall isn't good.
5. Stay in room: the tile must be in the start's room.
6. `CanPlace` on the middle of the tile (x x 16 + 8).

The first good tile wins. With no direction, the offset moves in a square spiral:

```
if (y <= x && y <= -x) x -= 1;
else if (y <= x) y -= 1;
else if (y >= -x) x += 1;
else y += 1;
```

With direction 0 or 4 the offset goes 0, +1, -1, +2, -2 and so on along y. With 2 or 6 it does that along x.

**Mode 6, the random tile. Proven, PS2** (`0x2255A8` to `0x225880` in `TryFindGoodLocation`, read instruction by instruction). It doesn't use the search. It makes **exactly 10 tries**, and each try is:

1. **x first:** `x = GetNextRandomNumber() % xsize`, where xsize is `cFixedWorld::GetXSize` (`gFixedWorld +0x10`). If xsize is 0, x is 0 and no number is drawn. **Then y:** the same with `GetYSize` (`+0x14`). Both use unsigned modulo, so each is 0 to size - 1. Unlike the search, there's no clamping to 1..size-2, so edge tiles can be picked. x and y are kept as signed bytes.
2. The point is the middle of the tile, (x × 16 + 8, y × 16 + 8). The level is the start's: the relative object's with `+2` bit 0, otherwise 1.
3. **With `+2` bit 2 (editable only):** the try fails if the tile's flag 0x20 is **set**. The flag is the byte `flags[x][y]` of the table at `gFixedWorld +0x20` (its `+0xC` is the row pointers). Unlike the search, the "edit anywhere" override at `0x50A6A8 +0x414` isn't consulted.
4. **Unless `+2` bit 1 (prefer empty, the default):** the try fails if the object-ID grid (`gObjectModule +0x30`, s16 at `[y × 64 + x]`) holds any object there. There are none of the search's exceptions for types 5, 0xE and 8. Outside 0 to 0x3F on either axis it counts as empty. This grid isn't per level.
5. `CanPlace(point, level, 0, 0)` (vtable slot 37). False fails the try.
6. Otherwise `Place` (slot 38) with the same arguments, and the mode succeeds. With `+2` bit 3, the direction from temp 0 is written after the place (bit 0 cleared for anything but a Sim).

There's no other test: no wall check, no room check and no roof check. A failed try leaves nothing behind. After 10 failed tries the result is false.

**The random numbers. Proven, PS2.** `GetNextRandomNumber` (`0x262AF0`) is the one generator. The Random primitive draws from it through `SRand` (`limit ? GetNextRandomNumber() % limit : 0`, the same shape mode 6 inlines), as do the game's other callers (39 call sites in all, mode 6's two among them). They all share one 32-bit state at `0x5112E8`:

```
state = state * 0x41C64E6D + 0x3039            (mod 2^32)
result = ((u64)old_state * 0x41C64E6D + 0x3039) >> 16, low 32 bits
```

So the number it returns is bits 16 to 47 of the 64-bit sum, not the new state. I checked this formula against a simulation of the instructions on 100,000 random states. Each try draws two numbers when both sizes are non-zero, so 10 failed tries move the state on by 20. `GetSRandSeed` (`0x262AD0`) and `SetSRandSeed` (`0x262AE0`) read and write the state. A recon stream saves it and restores it (`0x1BED6C` to `0x1BED88`). Likely: one other caller (`0x1EF52C`) seeds it from a call result when the simulator starts.

## Update (opcode 0x07)

**Proven: matched byte for byte** (`0x230CC0`). `+0` picks the object: 0 is me, 1 the stack object (missing is error 0x15), and anything else is error 3. It returns true.

| `+2` | What it does |
|---|---|
| 0 | Nothing |
| 1 | `GlobalDispatch(0x8D, object ID)`, to every `Commander` |
| 2 | If the object's room isn't 0xFFFB: `RoomManager::RoomScoreChanged` for that room |
| 3 | Refreshes the object's sim instance, if it has one |
| 4 | `House::ComputeAndStoreLotData`, if there's a house |
| Others | Error 3 |

## CompleteGoal (opcode 0x38)

**Proven for the logic, read from PS2 (`0x232018`) and checked on Wii. Not matched:** 2 words of 216 differ, and it's parked under `NON_MATCHING`.

If `+6` has bit 0 or 1 set it's the old primitive, which does nothing and returns true. `+7` is the type: 0, 1 and 2 are goals and 4 is an unlock, and anything else is error 0x6E. The goal's number comes from scope `+5` and data `+2`. `+0` is the operation, and status comes from `GoalAndUnlockData`: -1 returns false.

| Operation | Type | Needs status | Does |
|---|---|---|---|
| 1 | 0, 1 | 0 | Sets status 1 |
| 2 | 2 | 1 or less | Sets status 2 |
| 2 | 4 | | Unlocks |
| 3 | 2 | 1 or 2 | Sets status 3 |
| 3 | 0, 1 | 1 | Sets status 3 |
| 3 | Others | | Unlocks |

Then bit 2 of `+6` puts the status in temp 0. The result is true if any of these holds:

| `+6` bit | True when |
|---|---|
| 3 | Status 0 (locked) |
| 4 | Status 1, or 0 for type 4 (open) |
| 5 | Type 2 with status 2 (satisfied) |
| 6 | Status 3, or 1 for type 4 (completed) |

## AddAction

**Proven: matched byte for byte** (`cXPerson::AddAction`, `0x1AE2E0`). The queue is a ring of 12 actions at Sim `+0x21C`, each 0x40 bytes, with head `+0x300` and tail `+0x304`. Sim `+0x538` says the first entry is running.

1. No tree table entry returns 0. An immediate entry runs at once, and `RunImmediateAction`'s result is returned.
2. An entry that doesn't allow repeats returns 0 if the queue's last action has the same icon object and tree table index.
3. A continuation, push to head or run as sub goes to the front. A continuation that carries its name over takes the current action's name.
4. `CountVisible` counts visible actions, and actions next to each other with the same `+0x30` count once. Visible means it has an icon object and isn't hidden, and a continuation only counts at the head.
5. With 8 or more visible, a normal action looks at how many visible ones it could push out: those of lower priority that needn't run. If that leaves fewer than 8, there's room. If not, or if it's going to the front, an action that needn't run returns 0, and one that must is hidden.
6. Run as sub inserts at 0. Going to the front inserts after the running action.
7. Anything else cancels each waiting action of lower priority that needn't run (`ActionSkipped`), then goes on the end. If an immediate action is running on this Sim, it's overwritten with this one first.

Flags at `+0x34`: bit 1 continuation, 4 hidden, 5 push to head, 6 carry name over, 10 must run, 15 run as sub. Priority is `+0x28`.

## GotoRoutingSlot (opcode 0x2D)

It's Sim-only: `cXPerson::TryGotoRoutingSlot` (`0x1A8CA8`). 418 uses. **Proven** for the primitive, which matches the game byte for byte, and **likely** for the route states, read from the Wii build.

| Operand | Meaning |
|---|---|
| `+0` s16 | The slot's number, or the parameter holding it |
| `+2` s16 | 0: `+0` is a parameter number, and that parameter is the slot. 1: `+0` is the slot. 2: `+0` is a global slot |
| `+4` bit 0 | Clear means failure trees may run: `RoutingSlot` bit 14 at `+0x1C` is set |

The object to go to is the stack object. Slots are 0x3C bytes, in the object's list at `+0x100` or ObjectModule's global list at `+0x206C` on Wii and `+0x2070` on PS2.

The primitive keeps its state in frame `+8`, the word this page calls waiting elsewhere, so a node runs again each tick with the same state:

1. The Sim's model isn't loaded: return 2.
2. State 0: turn on smooth routing and find the slot. A missing stack object is error 0x35, a parameter past the frame's count is error 8, a bad slot number or `+2` value is error 0x25.
3. Any other state: no slot is looked up, and the route already under way carries on.

`GotoRoutingSlot` (`0x1AA008`) then runs. **Proven: it matches the game byte for byte.** State 0 or 0x15 starts a route:

1. A TS2 slot (flag bit 17) on a multi-tile target moves the target to the first part, along the part list, whose definition has `+0x3A` above 0.
2. No target, or one that isn't in the world (either tile coordinate below 1), returns 0, false.
3. A new route (`XRoute`, 0xA8 bytes on PS2) is made from the Sim, the target and a copy of the slot.
4. A ghost's copy gets flag 0x800, ignore rooms.
5. If the Sim already has a route that doesn't allow failure trees, the new one doesn't either.
6. Otherwise, with no route under way, variable 0x11 at 0, the target being the Sim itself and the slot's facing -2: it turns to the target (`CalcShortDistance`) and returns 1 at once.
7. The route goes on the end of the Sim's list (PS2 `+0x5D0`), which clears animator byte `+0x664` if it was the first. The state becomes 1.

Then, every tick:

1. With no route on the list it returns 0.
2. A route whose target is gone is popped: state 0, return 2.
3. A target that left the world gives state 0x13, or 0x14 if walking (state 3).
4. A route marked invalid (`+0x7C` 0) in state 2 drops its portal, and in state 3 becomes state 0xF. Then it's valid again.
5. The handler for the state runs, and returns the next state, until one sets done. A state number of 0x15 or more becomes 7.
6. The tree's code is returned. If it isn't 2, person `+0x5E0` is cleared.

**What the code means to the tree.** The primitive returns it unchanged, so these are `TryElement`'s values:

| Code | Tree |
|---|---|
| 0 | False branch: the route failed. Person data 0x1C has the reason |
| 1 | True branch: the Sim is at the slot |
| 2 | Wait: the node runs again next tick, in the same state (frame `+8`) |
| 3 | A handler pushed a tree (a waiting, ask-to-move, stand-up or failure-feedback animation). The pushed tree runs now. When it returns, frame `+8` keeps the node, so routing carries on |
| -1 | An error stopped the tree |

Handlers return the next state and set the code. It starts at 2.

| State | Handler | What it does |
|---|---|---|
| 1 | Initial | **Proven.** Variable 8 without bit 3 is state 7. A Sim that isn't a ghost (person data 0xC with bit 2 of the neighbour's word 0x2A) and isn't in the target's room goes to state 2, or state 7 if the target is a portal (type 8) one of its routes already uses. Interrupted is result 4, state 7. Then it builds the goals. Not sitting (person data 0 isn't 1) is state 0x12. Sitting on the goal chair is state 8. Sitting on a chair (function table entry 0x1A) runs its entry point 0x1B with the route's exit direction (`+0x78`) as parameter 0: person data 0x12 = that direction, 0x20 = 0, code 3, state 5. A chair without that tree is state 7, and sitting on something that isn't a chair is tree error 0x35, code -1, state 7 |
| 2 | WaitingForPortal | **Proven.** Interrupted is result 4, state 7. In the destination's room (data 0x1D): person data 0x20 = route `+0x9C`, state 1. Otherwise a current portal with no last result runs `cXPortal::FailedPortalTree`, then room routing: found is code 3, state 2 again; not found is result 2, state 0x10 |
| 3 | Walking | **Proven.** Interrupted (result 4) or no route or path is state 0xB. Otherwise `FollowOneStep` on the animator, which is also the tree's code: 1 is state 9, 2 is done this tick and stay, anything else state 0xA |
| 4, 5 | WaitingForSit, WaitingForStand | **Proven.** Sit: a last transition is state 8, else route result = person data 0x1C (5 if 0), state 0x10. Stand: person data 0x20 = route `+0x9C`, then a last transition with the Sim idling (person data 0 is 0 or 3) is state 1, else result 6, state 0x10 |
| 6 | Trapped | **Proven.** Waits for a routing turn like state 0x12, code 2. Then by the trap count (`+0x5C`): 0 compares where the Sim stands with where it was last time (`+0x64`), and moving more than 0x40 in squared sixteenths of a tile (half a tile) is count 0, state 1, otherwise count 1, stay. 1 is count 2 and `AskOthersToMove`: state 0xE if it asked someone, else 0x10. 2 is count 3, state 1. 3 is state 0x10, anything else state 7 |
| 7 | Failed | Result 1 if none was set, cleans up, **returns 0**, the false branch |
| 8 | Succeeded | **Proven.** Person data 0x20, if set, is copied to 0x21, and cleared again if the Sim can't be placed. `CanPlace` where it stands: then `Place`, and with two or more path points and variable 9 at 0, variable 1 = the direction of the last step (eighths, 0 to 7) and `ForceLocation`. Result 0, code 1, true. Can't place: an idle Sim (person data 0 is 0) is result 1, state 7; otherwise result 0, code 1. Then cleans up |
| 9 | WalkingSucceeded | **Proven.** State 8 once `EndFollow` is true, else done this tick |
| 0xA | WalkingFailed | State 6 once the animator agrees |
| 0xB | WalkTerminated | State 7 once the animator agrees |
| 0xC | FoundPath | **Proven.** `BeginFollow` with `CalcShortDistance` to the target, done this tick, state 3 |
| 0xD | Waiting | **Proven.** Interrupted is result 4, state 7. For 30 ticks from route `+0x60` (simulator ticks, `+0x9C`) it runs the Sim's own `sg_route waiting animation` tree: code 3, stay. After that, or with no such tree, it clears the wait and goes back to state 1 |
| 0xE | WaitingForPersonToMove | **Proven.** Interrupted is result 4, state 7. When the other Sim (`+0x84`) has gone: clear the wait, and the trap count too if the move worked (`+0x88`), state 1. On the first tick it runs its own `sg_route ask to move animation` tree with the other Sim as stack object, code 3, stay. Then up to 300 ticks of `sg_route waiting animation`. After that, or with no tree, state 6 |
| 0xF | RouteInvalidatedWhileWalking | Replans |
| 0x10, 0x11 | NeedFailureFeedback, WaitingForFailureFeedback | **Proven for 0x10.** Result 1 if none, copied to person data 0x1C. If the routing slot has flag bit 14, the target's `gl_route failure feedback` tree runs on the target with parameters (result, blocking object ID from route `+0x90`, 0, 0): code 3, state 0x11. Otherwise, or with no such tree, state 7 |
| 0x12 | FoundGoals | **Proven.** One Sim routes a frame: it may when nobody has yet and the routing queue (24 Sims) is empty or has it first. Otherwise it joins the queue once (PS2 flag `+0x5E8`), code 2, stay. Then `InitRoute` gives the code: 0 is state 6, 1 state 8, 2 state 0xC, 3 is person data 0x20 = 0 and state 4, others state 7. (An earlier version of this row said 3 waits for a portal: it's state 4, WaitingForSit.) |
| 0x13, 0x14 | ObjectDeleted, ObjectDeletedWhileWalking | The target went away: state 7 |

The tree's code, the second handler argument, is a 64-bit `long` on PS2. Handlers set it to 3 while they wait, and Failed sets 0.

Person data 0x1C ends up holding the route's result: 0 worked, 1 failed, 4 interrupted. If the target leaves the world part way, the state changes to 0x13 or, while walking, 0x14.

**For a port:** no pathfinding is needed for trees to run. Wait a tick or more, put the Sim on the slot, and return 1. Return 0 for a slot that can't be reached.

### How a slot becomes goals

`XRoute::ConstructGoals` (PS2 `0x1CDBE0`, Wii `800ED934`) turns the slot into goal tiles. Positions are `FTilePt`s, in sixteenths of a tile. **The rotation and the on-top case are proven on PS2**; the directional search is **likely**, read on Wii, with PS2's version found by its calls.

**The frame.** The object's direction is its data 1, in eighths, or 0 if the slot is absolute. The origin is the object's location. With flag bit 16 it's the object's average location instead (for a multi-tile object, its anchor).

**The rotation, `TransformToWorldCoords`. Proven, PS2 `0x1CA758`.** An offset (x, y) in the slot's frame becomes origin + the offset turned by the direction. Each sum is cut to an integer:

| Direction | Added to the origin |
|---|---|
| 0 | (x, y) |
| 2 | (−y, x) |
| 4 | (−x, −y) |
| 6 | (y, −x) |
| Odd | The offset turned by the object's rotation matrix, through world coordinates |

**On top of the object** (no direction bits in the flags' low byte). **Proven, PS2.**

1. The goal is the rotation of the slot's own offset, `GetXOffset` and `GetYOffset`, which add the Sim type's extra (see the facing table above).
2. The Sim must face the direction from the origin to that tile, or any direction if it's the origin's own tile.
3. If the tile can be stood on, it's the only goal, with score 100. Otherwise the route's result comes from what blocked it.

**Around the object** (likely, Wii):

1. **The slot must make sense:** minimum 0 to 0x200, maximum 1 to 0x200, minimum ≤ optimal ≤ maximum, all in sixteenths of a tile. Otherwise there are no goals.
2. **Levels:** the score bonus is multiplier 1 × 10 on the Sim's level. Otherwise it's the larger of multipliers 0 and 2, × 10.
3. **Walls:** unless rooms are ignored (flag 0x800), a wall next to the object's tile blocks the directions it covers and the ones beside them.
4. **The search:**
   - The step is the slot's resolution, or 16 (a whole tile) when there's a level multiplier.
   - For each allowed direction d (flag bits 0 to 7, turned by the object's direction), it walks outward one step at a time.
   - It keeps points whose distance from the origin is between minimum and maximum.
   - The Sim faces the object: direction (object direction + d + 4) mod 8.
5. **The score:**
   - 100 − round(|distance − optimal| × gradient), plus the level bonus.
   - ± up to 10 at random with flag bit 13.
   - 25 more in one case not yet named, and 20 more if the tile is where the Sim already stands.
6. **A chair** on the tile, facing the right way and allowing that side (definition `+0x84`), is a goal to sit on, with its ID. If someone else is in it, that's recorded as the blocking object.
7. **Each point** is checked like the on-top goal, and the usable ones are added. With random scoring, a door there rules the tile out.

The route then walks to the best goal it can reach. **For a port:** the on-top case, with the rotation above, gives the exact tile. For the others, the point at the optimal distance in the best allowed direction, facing the object, is a good stand-in.

### Which way is x, and how directions turn

**Proven, PS2.** `TransformToWorldCoords` (`0x1CA758`) and `cXMTObject::AssignOffsets` (`0x1FFC90`) match the game byte for byte. `OffsetCoordsInDirection` is inlined in `ConstructGoals` (`0x1CE898`) and was read there, the same as Wii's.

**The axes.** Everything here is in the game's own tile axes. x is the first word of an `FTilePt`: an object's location at `+0xC4`, which `IsInWorld` tests along with y at `+0xC8`. Both are in sixteenths of a tile. **A routing slot's first offset is that x**:

- Descriptor `+0x04` is `RoutingSlot +0x04`.
- `GetXOffset` returns it, with the Sim type's extra added.
- `ConstructGoals` passes that as x to `TransformToWorldCoords`, which adds it to the origin's first word.
- The second offset (`+0x08`) is y in the same way. The third (`+0x0C`) isn't used for the tile.

So if a port's data has x and y swapped from these axes, it has to swap a slot's offsets and object locations the same way before applying any of the rules below.

**Directions** are in eighths, and all three functions agree on which way they turn:

| Direction | `OffsetCoordsInDirection`: one step | `TransformToWorldCoords`: offset (x, y) becomes | `AssignOffsets`: part offset (x, y) becomes |
|---|---|---|---|
| 0 | y − 1 | (x, y) | (x, y) |
| 2 | x + 1 | (−y, x) | (−y, x) |
| 4 | y + 1 | (−x, −y) | (−x, −y) |
| 6 | x − 1 | (y, −x) | (y, −x) |
| Odd | Both: 1, 3, 5, 7 are (+1, −1), (+1, +1), (−1, +1), (−1, −1) | The object's rotation matrix, through world coordinates | (0, 0) |

**Checking it:** turning direction 0's step, (0, −1), by direction 2 gives (+1, 0), which is direction 2's step. Turning it by 4 gives (0, +1) and by 6 gives (−1, 0). So "direction d" means the same thing in all three: it faces −y at 0 and +x at 2.

- **`TransformToWorldCoords`**, in full: the offset is turned as above, then **x = origin x + x and y = origin y + y**, each cut to an integer. For directions 2 and 6, the old x is cut to an integer before it's moved to the other axis.
- **`AssignOffsets`:**
  1. The first part (`+0x140`, or the object itself) gives the base: position words `+0x144` and `+0x148`, plus a third at `+0x14C`, and the first part's data 1 as the direction.
  2. For each part along `+0x13C`, it stores (part − base) at `+0x150`, `+0x154` and `+0x158`.
  3. Then it turns the first two by that direction, **the same way as `TransformToWorldCoords`, not the reverse**.
  4. Odd directions store (0, 0).

  Each part's own data 1 is also read in the loop, and not used.

## PushAction (opcode 0x0D)

**Proven** for the primitive: `TryPushAction` (`0x228788`) matches the game byte for byte. `AddAction` is **likely**, from the Wii build. 334 uses.

It puts an interaction on a Sim's queue. The Sim is the stack object, and the interaction belongs to another object.

| Operand | Meaning |
|---|---|
| `+0` u8 | Interaction number in the object's `TTAB`, or the temp holding it with bit 6 of `+3` |
| `+1` u8 | The parameter holding the object's ID, or the local with bit 1 of `+3` |
| `+2` u8 | Priority, below |
| `+3` bit 0 | Icon: the object in local `+4` shows on the queue |
| `+3` bit 1 | `+1` is a local |
| `+3` bit 2 | Continuation of the current interaction |
| `+3` bit 3 | Carry the name over from the current interaction, for a continuation |
| `+3` bit 4 | Push to the head of the queue |
| `+3` bit 5 | Run as a sub-action |
| `+3` bit 6 | `+0` is a temp |
| `+4` u8 | With bit 0, the local holding the icon object |

| `+2` | Priority |
|---|---|
| 0 | Inherit: person data 0xE if I'm a Sim, otherwise 1 |
| 1 | 100 |
| 2 | 2 |
| 3 | 50 |
| Others | Error 0x2E |

In order:

1. Read the object's ID. A local past the count is error 0x34, a parameter past it error 8. Each test allows one past the end.
2. A missing object is error 0x17. A multi-tile object means its lead tile.
3. A temp number of 8 or more is error 2.
4. The object has to have a `TTAB`, or it's error 0x1F, and the interaction has to be in it, or error 0x16.
5. A missing stack object is error 0x15, and one that isn't a Sim is error 0x1C.
6. A bad icon local is error 0x34. A continuation on an idle Sim is error 0x4B.
7. `AddAction` decides. It returns 1 if the action went on, the true branch, and 0 if not.

`AddAction` returns 0 if the interaction has no `TTAB` entry, or if the last thing on the queue is the same interaction on the same icon object, unless the entry allows repeats. A queue holds 8 visible actions: once full, a new one replaces the lowest priority action below its own, unless that one must run. An entry can run at once instead, through `RunImmediateAction`.

**For a port:** returning 1 and keeping a list per Sim is enough until Sims run their queues.

## Relationship2 (opcode 0x1A)

**Likely**, read from both builds. The C++ for `0x228ED0` is parked under `NON_MATCHING`, 563 of 732 words. 358 uses.

It reads or writes one field of one relationship between two things.

| Operand | Meaning |
|---|---|
| `+0` u8 | The field |
| `+1` u8 | Who to whom, below |
| `+2` bit 0 | Clear: create the relationship if it's missing |
| `+2` bit 1 | The IDs are neighbour IDs, not object IDs |
| `+2` bit 2 | Set: write the field from the value. Clear: read the field into the value |
| `+2` bit 3 | Pet ownership: use an object's own relationship list |
| `+3` u8 | The local, for `+1` 2 and 3 |
| `+4` u8 | The value's scope |
| `+6` s16 | The value's data |

| `+1` | From | To |
|---|---|---|
| 0 | Me | Stack object |
| 1 | Stack object | Me |
| 2 | Stack object | Object in local `+3` |
| 3 | Object in local `+3` | Stack object |
| Others | Nothing, so error 0x15 | |

A local past the frame's count is error 0x34.

- **With neighbour IDs**, `+1` 0 and 1 need me to be a Sim, or it's error 0x1C, and I'm my neighbour. A missing neighbour is error 0x15, and field 2 is error 0x56. The list used is the first neighbour's, keyed by the second's ID.
- **With object IDs**, a missing object is error 0x15. The list is the first object's `RelMatrix` at `+0xBC`, keyed by the second object's ID. **But if both objects are Sims, it switches to their neighbours**: the list is the first neighbour's at `+0x10`, keyed by the second neighbour's ID, and field 2 is error 0x56. So relationships between Sims live on neighbours, however the tree names them.
- **Pet ownership** has the object first and a person second. `+1` 1 with neighbour IDs is error 0x5B, and a second party that's missing or not a Sim is error 0x5C.

Then, with the list and the key:

1. If the relationship is shorter than `field + 1`, then with create off it's **false**. With it on, it grows, and it's **true** if it's still too short.
2. Read the value through `+4` and `+6`.
3. With assign, write the field. If the first party is the player's neighbour, the change also goes to the goals system. On neighbours, assign also marks both neighbours and their families as changed.
4. Otherwise the value needs an address, or it's error 0x13, and the field is written to it.
5. **True.**

## CallNamedTree (opcode 0x1C)

**Proven** in all but name: the C++ for `0x226948` has 410 of its 411 words, and the other is a branch that behaves the same. It's parked under `NON_MATCHING`. 356 uses.

It finds a tree by name and runs it on the stack object.

| Operand | Meaning |
|---|---|
| `+0` s16 | `STR#` set holding the name |
| `+2` s16 | Where to look: 1 global, 2 semi-global, 3 private, others by the running tree's class |
| `+4` u8 | The string's number |
| `+5` u8 | How to run it, below |

If `+2` isn't 1 to 3, a private tree looks in its private namespace and then the semi-global one, and a semi-global tree the other way round. A global tree has nowhere to look: error 0x2D. An empty or missing name is **false**.

The stack object has to exist, or it's error 0x15.

| `+5` | Runs on | How | Result |
|---|---|---|---|
| 0 | Me | `RunCheckTree` to the end, stack object as its object | Its result |
| 1 | Stack object | `RunCheckTree` to the end, itself as its object | Its result |
| 2 | Me | A subroutine: the tree is pushed and runs normally | Below |
| Others | | Error 0x2D | |

The tree comes from the stack object's `Behavior`, and `EREdithTreeSet::GetTreeIDByName` compares names with `strcmpi`, ignoring case. A name it doesn't have is **false**.

`+4` counts from 1. The port's control proved it: only that reading finds a string for all 356 uses. Of those, 234 names match a tree exactly, 15 more match ignoring case, and 107 name no tree on the disc.

For `+5` 2, the primitive sets frame `+8` to 1 and returns 3, so the same node runs again when the called tree returns. Then frame `+8` is 1, and the result is `GetLastResult`: the true or false that the tree returned with. If I'm not the object doing the running, it's false.

## GenericSimCall (opcode 1)

**Proven for the numbering**: every call was compared with the PS2 function (`0x22D518`), and all 67 entries of its switch on `+0` do the same job as on Wii. No number differs. Calls 9 to 0x12, 0x16, 0x18, 0x1F, 0x25, 0x29, 0x2C, 0x2D, 0x31, 0x34, 0x36 and 0x38 were read line by line on PS2. The rest are compared by what they call, so their details are **likely**. 290 uses. Past 0x42, and at 0x1B to 0x1E, 0x20 and 0x23, is error 0x2A.

**Proven, the source order:** the jump table at `0x4BA900` sorted by address gives the order the cases were written in, since GCC lays them out in source order: 0, 1, 3, 2, 4, 5, 6, 8, 9, 0x25, 0xA, 0xB, 0xD, 0xE, 0xF, 0x10, 0x15, 0x18, 0x19, 0x1A, 0x1F, 0x21, 0x22, 0x26, 0x28 to 0x2D, 0x30, 0x32 to 0x37, 0x39, 0x3A, 0x12, 0x3B, 0x3E, 0x3F, 0x3C, 0x3D, 0x40, 0x41, 0x42, 0x38, 0x11, 7, 0x13, 0x17, 0x14, 0xC, 0x24, 0x27, then the error cases and the shared `return 1` of 0x16, 0x2E, 0x2F and 0x31. `TryGenericSimCall` in `cXObjectPrims.cpp` is started in that order, under `NON_MATCHING`.

`+0` s16 is the call. Everything else comes from temps, written t0 to t7 here. Unless it says otherwise, a call returns 1. Calls not listed aren't used by any tree on the disc.

| Call | Uses | What it does |
|---|---|---|
| 0 | 0 | Starts the `hurricane` weather state (`SetWeatherState("hurricane", 2, false)`). **Proven, PS2.** No tree uses it |
| 1 | 0 | **Swaps me and the stack object**, both Sims (not a Sim is error 0x1C, no stack object 0x15). Each is picked up and placed (if `CanPlace` allows) on the object the other holds in object variable 2, at tile (-1, -1), level 1, and takes the other's direction (variable 1). Each animator then gets an offset from that object's first slot, turned by its direction. **Quirks:** the offsets add up, so the second Sim's offset includes the first's; and only my object is checked for being missing (error 0x15), as on Wii. **Proven, PS2.** No tree uses it |
| 2 | 2 | Stack object is the icon of my current action, if the action has an ID. Missing is error 0xA, me not a Sim error 0x1C. **Proven, PS2** |
| 3 | 1 | If the stack object is the selected Sim, asks the game flow for 0x20, otherwise error 0x14. Not a Sim is error 0x1C. **Proven, PS2** |
| 4 | 8 | Moves the neighbour in the stack object into family t1, or the selected family if t1 is 0 or less. Makes family t1 if it doesn't exist. Person data 0x2D gets the old family (0x28). If removing it from its old family or adding it fails, false. Then, if neighbour t0 exists and the moved Sim is of type 0, it takes t0's last name (selector and sim description). No neighbour is error 0x38. **Proven, PS2** |
| 5 | 1 | Merges family t0 into the selected family: t0's funds are added to the funds, then the selected family's inventory takes t0's (`MergeInventories`). False if either family is missing. **Proven, PS2** |
| 6 | 6 | Removes the neighbour in the stack object from its family. No neighbour is error 0x38. False if that fails. **Proven, PS2** |
| 7 | 1 | Goal and unlock data call |
| 8 | 11 | Loading screen. t0 non-zero starts one and returns 1. Otherwise it returns 4 until the iris transition is in, then 1 |
| 9 | 24 | Finishes loading: clears `_gd +0x9C`'s word `+0x10`, then `LoadFinishing` |
| 0xA, 0xB | 4, 5 | Adds me to, or removes me from, ObjectModule's list at `+0x2030`. Buy and build are off while anything is on it |
| 0xC | 1 | Tribe gets all tools |
| 0xD | 2 | `CleanupPeople` on the stack object, inlined: every person but the stack object gets `Cleanup(stack object)`, vtable `+0xE0`. Missing is error 0xA |
| 0xE | 3 | Global 0x1F = t0. Globals are s16s from simulator `+0x38` |
| 0xF | 52 | Needs me to be a Sim, or error 0x1C. Person data 0x20 = t0, 0x21 = 0. With t0 of 0, also `ComputeRect` from my location **Proven, PS2.** |
| 0x10 | 6 | Game sequences, by t0: 0 starts sequence 2, 1 sequence 3, -1 and -2 wait for them, 2 is game over. Waiting returns 4 **Proven, PS2.** |
| 0x11 | 8 | Sets the harvest count to t0 |
| 0x12 | 12 | True if in build or buy mode |
| 0x13, 0x14 | 1, 1 | Tribe gets t0 of every food, or every resource. **Proven, PS2:** it walks the `InventoryTokens` table and adds a row whose flags word (`+4`) has bit 2 for food. For resources, bits 1 and 2 must be exactly 2, and IDs 0x32 to 0x35 are skipped. No player Sim does nothing |
| 0x15 | 0 | Global 0x27 = t0. **Proven, PS2.** No tree uses it |
| 0x16, 0x31 | 7, 10 | Nothing: returns 1 |
| 0x17 | 2 | Unlocks everything. **Proven, PS2:** sets bit 0 (visited) on all 0x1B lots and bits 0 to 2 on all 0x48 transitions in `LotManager` |
| 0x18 | 2 | Global 0x29 = 1 if t0 is 0, else 0 |
| 0x19 | 0 | Global 0x28 = t0. **Proven, PS2.** No tree uses it |
| 0x1A | 1 | Changes the selected Sim |
| 0x1F | 6 | Selects Sim t0 through `EGlobal::SetSelectedPerson`, then true if it's now `_globals +0x174`, the player's Sim. Not a Sim, or missing, is false |
| 0x21 | 1 | Shows all object dialogs |
| 0x22 | 5 | **Opens Create-a-Sim**, not a lot change (this row used to say "starts a lot change to lot t2"). Sets the UI values `CAS_mode` 1, `CAS_neighborID` t2, `CAS_CurrentGameTime` the game time, `CAS_lotnumber` t1 and `PlayerNumber` 0, asks the game flow for state 8, and remembers the selected Sim's neighbour ID to select afterwards. Returns 2 (and does nothing) while the HUD's transition target is 3. **Proven, PS2** |
| 0x24 | 1 | Replenishes the tribe, t0 |
| 0x25 | 5 | A flag = t0 != 0: `_globals +0x4C4` on PS2, a byte at `+0x3FB` on Wii |
| 0x26 | 2 | t1 = 1 if object t0 is in the world, else 0. False if there's no object **Proven, PS2.** |
| 0x27 | 1 | `AutonomousFoodSelection` on t1, t2, t3, through a new `AutoFood`. True if it chose something. **Proven, PS2** |
| 0x29 | 4 | Only checks: t0 above 1 is error 0x14, and not being a Sim is error 0x4C **Proven, PS2.** |
| 0x2C | 17 | Closes modeless dialogs **Proven, PS2.** |
| 0x2D | 30 | Controller rumble, for the player's Sim only. **Proven, PS2; corrects an earlier reading.** The source is the stack object, not an operand: none does nothing, and so does a Sim other than the player's. t0 of -1 stops the rumble; anything else starts one from the stack object, through the player interactor. (This row used to say `+4` u16 was the effect and t0 an object: `+4` is the frame's stack object.) |
| 0x2A | 0 | t0 = 1 unless the camera director (`_globals +0x1D0`) is in control: its state at `+0x2B0` isn't 0 or 8. **Proven, PS2.** No tree uses it |
| 0x2B | 0 | t0 = the number of modeless dialogs open. **Proven, PS2.** No tree uses it |
| 0x30 | 0 | Adds t0 to the aspiration points, which are neighbourhood variables 0xC (below 10,000) and 0xD (tens of thousands), up to 999,999. **Proven, PS2.** No tree uses it |
| 0x32 | 0 | t7 = 1 if music genre t7 is enabled in the options (`_globals +0x1F8`), else 0. **Proven, PS2.** No tree uses it |
| 0x33 | 0 | **Blocks routing over a rectangle:** tile flag 0x10 is set (t0 non-zero) or cleared (t0 0) on t1 by t2 tiles from the stack object's tile, t1 along its direction (object variable 1: 0 is +x, 2 is +y, 4 is -x, 6 is -y) and t2 to its side. t1 or t2 not above 0 is error 0x14, no stack object 0x33. A bounds test first: the object's tile above 0 on both axes, and the far corner within 0 to the world's size, or error 0x5E. **Quirk:** for directions 2 and 6 that test swaps the corner's x and y, so it can pass or fail wrongly; the tiles changed are the right ones. Other directions change only the object's own tile. **Proven, PS2.** No tree uses it |
| 0x34 | 1 | t2 = 0 |
| 0x35 | 0 | t0 = the stack object's GUID modulo 0xF08A. Missing is error 0x33. **Proven, PS2.** No tree uses it |
| 0x36 | 4 | Returns 0 whatever t7 holds |
| 0x37 | 1 | Moves neighbour t0 into the first free family, and t1 = its number. Free: numbers from 1 up to the number of modifiable families (`NghResFile`), the first that doesn't exist (it's made) or whose family slot 7 is 0. None free: false, and t1 is left alone (this row used to say -1). No neighbour is error 0x38. **Proven, PS2** |
| 0x38 | 1 | Returns 0 |
| 0x39 | 9 | Clears cached catalog resources: the stack object's (`+0x138`) and its master selector's (`+0xB0`). Every object pointing at either loses the pointer, then both are deleted (once, if they're the same). There's no test for a missing stack object. **Proven, PS2** |
| 0x3A | 4 | Adds (t2 = 1) or removes (t2 = 0) the wall between the tiles of objects t0 and t1 (`TileWalls::GetWallSegsBetweenTiles`; no such wall segment is error 0x60, any other t2 0x61, a missing object 0x33). **Adding:** false if t0's tile already has that wall. Otherwise the wall goes on both tiles' sides, styled 3 if t0 is a door, 0x11 if a gate (type 8, definition word 0x1C 2 or 1, with function 0xF), else 1, pattern 0, then the walls are committed. **Removing:** false if there's no wall; otherwise `ERoom::DeleteWallAtTile` on t0's tile and a commit. **Proven, PS2** |
| 0x3B, 0x3E, 0x3F | 1 each | Plans: toggles all in the UI, ignores requirements, forces unlock |
| 0x3C | 12 | t1 = the first word of the stack object's plan (`GetPlanByGUID`; scope 0x43's field 0), or 0 with no plan. Whether that word means "unlocked" isn't shown by this code. A multi-tile object tries its master's GUID, then its interaction leader's, then each part's in order. Missing is error 0x17. **Proven, PS2** |
| 0x3D | 0 | t1 = the first word of the plan made from token t0. No such plan is error 0x6B. **Proven, PS2.** No tree uses it |
| 0x40 | 2 | Neighbour t0's way here (`LotManager::CanSimGetToLot`). t1 = 0 first. Already on this lot, or no lot (-1): true. Otherwise false unless the lot graph exists, this lot has been visited and a route is found. The route is up to 30 lot numbers, and t1 = the last one; a route of 0 or 30 lots is false. No neighbour is error 0x38. **Proven, PS2** |
| 0x41 | 3 | **Proven, PS2 and Wii; corrects an earlier reading.** True if there's a player tribe inventory and t0 is an ID in the `InventoryTokens` table. It doesn't check that the tribe has the token. No player Sim is false |
| 0x42 | 7 | **Evaluates goal book t0** (`GoalUnlock::TryEvaluateGoals`, vtable slot 15; PS2 calls `+0x80`). If a goal or page was just rewarded, t1 = its dialog key and true; false if none. **Proven, PS2.** See "Goals: what GenericSimCall 0x42 returns" |

## Grab (opcode 4)

**Read on PS2** (`cXObject::TryGrab`, `0x230ED0`), likely. 31 uses. The stack object goes into my hand. No stack object is false. The object is asked two virtuals, `+0x130` and `+0x138` on its vtable, with my location and me as arguments: the first can refuse, false, and the second does the pick-up. They are cXObject vtable slots 37 and 38, **`CanPlace` and `Place`**, with me as the container: a pick-up is placing the object into my hand. See "Picking up: CanPlace into a hand" below. With a model (`+0x14`), the object's render flags get bits `0xC0` set, or `0x100` as well when `+0` bit 0 is set, and everything it contains gets the same, walking the container chain. Always true after that.

## Tree tables and the action queue

**Proven for the layouts: read from the PS2 getters, which are one instruction each**, and the tables on the disc decode to names like "Eat" and "Harvest Coconuts" with sensible tree IDs. The rest is read on PS2 and likely.

A `ResFile` row's `+0x24` vector holds its **tree tables**, the interactions a pie menu offers for objects of the row. Each record is 12 bytes:

| Offset | Meaning |
|---|---|
| `+0` | Vector of entries, 0x24 bytes each |
| `+4` s16 | Motive ad count |
| `+6` s16 | Table ID. `ObjDefinition` word 13 (`+0x1A`) names the one an object uses; 725 of the 868 definitions with a table agree, and the rest, mostly 0, get the row's first |
| `+8` s16 | Prefix check tree, run before any entry's own unless the entry ignores it |

Each entry (`TreeTableEntryQuickData`, `0x25CFD0` on):

| Offset | Meaning |
|---|---|
| `+0` s16 | Check tree |
| `+2` s16 | Action tree |
| `+4` | 16 ads of 8 bytes: personality variable, minimum, range (`GetPersonalityVarRef`, `GetMinRef`, `GetRangeRef`) |
| `+8` f32 | Custom attenuation |
| `+0xC` u16 | Attenuation |
| `+0x10` u32 | Flags, below |
| `+0x12` s16 | Bit 0: allow on self |
| `+0x14` s16 | Index: what `PushAction` and the queue name an interaction by (`GetEntryByIndex`), not its position; 242 entries sit out of position |
| `+0x16` s16 | Autonomy threshold; 100 or more means manual only |
| `+0x18` s16 | Join index |
| `+0x1A` s16 | Order index, the menu order |
| `+0x1C` | Name, a `BString2`: a pointer to UTF-16 text. A name starting with `!` is debug-only too |
| `+0x20` u32 | Activation: subtile in bits 0 to 3 and 4 to 7, extents in four 6-bit fields from bit 8 |

Flag bits: 0 available to visitors, 1 can join, 2 immediate, 3 allow consecutive, 4 ghosts, 5 large dogs, **6 not for adults**, 7 debug only, 8 auto first select, 9 visiting players, 10 ignore prefix tree, 11 must run, 12 custom activation, 13 cats, 14 small dogs, 15 demo, 17 dynamic ads, 18 chimps, 19 boars. 900 of the 1,565 entries on the disc are `0x201`.

`ObjSelector::GetTreeTab` (`0x1DA440`) caches the object's table at selector `+0x3C`, made by `ObjectFolder::GetTreeTable` (`0x21F700`). On First Beach a coconut offers Eat, Identify and Put in Inventory (check trees `0x200D`, `0x200E`, `0x200B` and action trees `0x200C`, `0x200A`, `0x2008` in `resources_sg`); a palm offers three harvests and Identify.

**The queue.** `PushAction` and the pie menu put an `Interaction` on the Sim's ring of 12 at `+0x21C` (`AddAction`, above). `Interaction +0x34` holds its flags: bit 1 continuation, 5 push to head, 10 must run, 15 run as a sub-action. `+0x28` is its priority and `+0x38` its action tree. Sim `+0x538` says the first entry is running, `+0x564` is the running priority, and `+0x586` non-zero stops every interrupt.

**How the player cancels an action. Proven on PS2** (with Wii for the names).

1. **The input.** `H2DTarget::Update` (`0x29F8D8` on) watches **controller command 6**, and acts when it's **released**. It needs a selected Sim (`_globals +0x174`) and the HUD's ActionMenu (HUD `+0x90`). Then it calls the Sim's virtual **`cXPerson::CancelLastAction`** (`0x1AE928`, vtable `+0x1C8`).
   - On success it sets the menu's backdrop, plays sound `0x48AE94F`, and tells the player interactor (`0x176698`).
   - Which pad button command 6 is isn't read here. The tutorial's "To stop press `<BUTTON_01>`. This will cancel any action." makes it **Triangle**, likely.
   - The Wii's `SimInteractor::OnCommandPressed` does the same on its command 4.
2. **Which action** (`CancelLastAction`):
   - **the newest waiting action**, the last in the ring that isn't running;
   - with none waiting, **the running one**;
   - if the action before the newest runs as a sub-action (flag bit 15), nothing, and it returns 0. A sub-action can't be split from its parent.
   - It passes the action's ID (`+0x30`) to `cXPerson::RemoveAction`.
3. **`cXPerson::RemoveAction(id)`** (`0x1AE7C8`) finds the ID in the ring; an unknown ID returns 0.
   - **A waiting action:**
     - flag bit 10 (must run) set: it only gets bit 8 (cancelled), stays, and runs anyway;
     - otherwise **`ActionSkipped`**: removed from the ring, and the object's **entry point 4** runs at once, as for a refused check tree (below).
   - **The running action** (the head, while `+0x538` says it's running):
     - it gets **bit 8, cancelled** (Wii `Interaction::SetCancelled`), with bit 10 set it stops there and runs on;
     - otherwise person data 0xE goes to 0, and `ShouldInterrupt` is asked.
   - **Not removed, no entry point 4.** The running action isn't removed, and entry point 4 doesn't run. Its tree is **interrupted**: `ShouldInterrupt` is yes for a running head with bit 8 and not bit 10, before its other tests. Every routing state then ends with **result 4 (interrupted)**, and `IdleForInput` takes its interrupt. The tree ends through its own interrupted paths, and the queue moves on.
   - **Interrupts locked** (`+0x586` set, so `ShouldInterrupt` says no): it queues a new interaction on the Sim itself (`Interaction(me, me, 0x21, 0x32)` then `AddAction`). What that one is isn't read.
4. **The action queue's cancel mode isn't reachable.** `ACTTarget` has a cursor (`+0x68C`; Wii `CancelCurrentObject`, `ExitCancelMode`). `H2D_SELECT` cancels the item under it (PS2 `0x296668`, through the same `RemoveAction`). But nothing ever sets the cursor to an item, on PS2 or Wii. The only stores advance an existing cursor or clear it. So the PS2 player can't pick a queued icon to cancel. Proven by the absence of any other store.

**How a queued action starts. Read on PS2, likely.** The Sim's main tree idles in `Wait For Notify` (global `0x119`), on `IdleForInput`. Each tick that primitive first asks `cXPerson::ShouldInterrupt` (`0x1AEAD0`): yes when the next action waiting, after the running one if any, has a higher priority than the running priority or is a continuation; when any of the motives at Sim `+0x194`, `+0x19C` or `+0x1A4` is below -99; and otherwise only for a head that runs as a sub-action. Then `executeInterruptOnIdlePrimitve` (`0x1AEFD0`):

1. Sets bit 6 (value 0x40) of my variable 8, which `Wait For Notify` tests and clears as "notified".
2. Nothing queued: returns 1. Back on the same node after an action that wasn't a continuation, the frame's word `+8` is still set: returns 1, so the tree moves on and the main loop looks again.
3. A head that doesn't run as a sub-action needs the primitive's `+2` operand, or it returns 1; with an action already running it is error 0x51.
4. A head whose object is gone leaves the queue, and the primitive returns 2.
5. `0x19F548` prepares it: my variable 0x32 goes to 0; the object's variable 8 bit 9 or an entry flag against the Sim's kind refuses it; then the table's prefix check tree, unless the entry ignores it, and the entry's check tree run on me with the object as the stack object. Interaction flag bit 9 passes whatever they said. A failure is `ActionSkipped` (`0x1DD310`), the queue drops the head, and the primitive returns 2. **`ActionSkipped(action)`, proven, PS2:** it takes the action's stack object (`Interaction::GetStackObject`, `0x24D720`), finds the action in the ring and removes it (`ActionQueue::Remove`, `0x1A69B0`), and then, if there's an object, runs that object's **entry point 4** tree to the end at once (`cXObject::RunTree`, `0x253FD8`: the object's function table gives the tree, `HasFunction`, and vtable slot 9 `RunCheckTree` runs it) with my ID as the stack object and no parameters. An object without an entry point 4 tree gets nothing. Wii's `ActionSkipped` is the same.

**The 8-visible rule is proven:** `AddAction` byte-matches, and its step 5 above is the rule.
6. `cXObject::GosubObjectTree` (`0x255D30`) clears my idle status and pushes the action tree, from the object's own behaviour, onto my stack with the object as the new frame's stack object.
7. `+0x538` is set, the countdown operand goes to 0, the frame's `+8` is set, variable 8 bit 6 again, and the primitive returns 3: the node reruns straight into the action.

**How it ends.** `cXPerson::StackJustPopped` (`0x1B0978`) keeps a list at `+0x528` of 12-byte records, each an object and the stack size its action's tree was pushed at. A pop that takes the stack below that depth removes the record and takes 1 off the object's variable 0x3E, and with no record left `0x1DD210` finishes the action: the head leaves the queue, and `+0x538` stays set only for a head that ran as a sub-action, never with an empty queue.

**GosubFoundAction (opcode 0x1E)**, Sim-only, 5 uses, all in the autonomy trees after `FindBestAction`. **Read on PS2** (`0x1A85E0`), likely: no stack object is error 0x15; the stack object's table entry whose index is my variable 0x14 is the action, no table is false and no entry error 0x16; its action tree goes on my stack as `GosubObjectTree` does, a new `Interaction` of priority 2 goes to the head of the queue marked running, and the primitive returns 3. A push that fails is -1.

**For a port:** `scripts/bhav/x_object.gd` in the Godot project has all of this, and `tests/interaction_test.gd` there runs Eat on a First Beach coconut through the Sim's own main tree: idle, interrupt, route, the grab on the reach clip's event, the bites, and `KillObject`.

## What a placed object starts with

`ObjM` is the saved state of a running lot, so a placed object arrives with almost everything a tree reads. `docs/formats/world.md` has the layout, and `tools/placed.py` reads it. **Proven** on all 2,611 objects: each ends exactly on its stream mark, and `placed.py check` has the controls. An earlier reader was misaligned from the attribute lists on, which is why the variables and attributes seemed to be missing.

- **Variable 0xB is the object's own ID.** `Reset` and `Initialize` both copy `mID` (`+0xC0`) into it, and it equals the ID on all 2,611 placed objects. So "my variable 0xB" is "me", and a tree that gives it to `TestObjectType` is testing itself.
- **Attributes are in `ObjM`.** Make the arrays from the counts below, then copy the saved lists over the start of them. The rest stay 0. Non-zero attributes are on 168 objects, semi-global on 1,488, static on 407. Most objects then reset on load, which clears all but the static ones: see "How an object starts running".
- **Variables, temps, object slots and the idle status** are all there. A variable the object doesn't list takes its starting value from the table below.
- **The stack holds one frame,** at node 0 of the object's main tree, with its locals and parameters. So a loaded object's next tick starts main.

### Object list order

**Likely, from Wii:** `DoStream` adds each object with `AddObject(object, id, true)`, which appends, so the list is in `ObjM`'s order after loading. It has to be: the same walk writes the IDs and then the objects, and the reader matches them up by walking its own list. Everything added later goes through `AddObject(object, id)`, which **prepends**. So a Sim or a created object comes before every placed object, newest first. Objects of type 9 (GUID 0x437 on these lots) are swapped for a placeholder and killed at the end of the load, which removes them without changing the others' order.

## Entry points

**Proven for the table, from callers for the names.** An object's trees for game events are a table, `ObjFnData`. A `ResFile` row's `+0x2C` vector holds 0x90-byte records:

| Offset | Meaning |
|---|---|
| `+0` | ID. A definition's record is the one matching `ObjDefinition +0xBC` (1,315 of 1,335 definitions have one) |
| `+4` | 35 s16: the tree for each entry point. 0 means none |
| `+0x4A` | 35 s16: a check tree for each entry point |

With no record, entry point 0 is global tree 0x10F, entry point 1 is global tree 0x187, and the rest are 0.

| Entry point | Used by | What it's for |
|---|---|---|
| 0 | `Reset`, `PostLoad` | Init |
| 1 | `Reset`, `Cleanup` | Main: the tree a reset stack starts on |
| 2 | `PostLoad` | Load |
| 3 | `Cleanup` | Cleanup |
| 4 | `cXPerson::ActionSkipped` | An action was skipped |
| 5 | `TestIntersection`, routing | May something overlap this? |
| 6 | `UpdateWallAdjacency` | Walls next to it changed |
| 7 | `UpdateRooms` | Rooms changed |
| 8 | `UpdateDynAdjacency` | Multi-tile adjacency |
| 9 | `Place`, `Pickup` | Placed |
| 10 | `Pickup` | Picked up |
| 11 | `UserPlace` | The player placed it |
| 12 | `UserPickup` | The player picked it up |
| 13 | `PassiveInfluence` | Applying an influence |
| 15 | `IsDoor`, `AddObject`, routing | Portal. An object with this tree is a portal |
| 17 | `KillAllObjects` | Everything is being removed |
| 25 | `UpdateChairFacing` | Chair facing |
| 26, 27 | `InitRoute`, `Routing_Initial` | Routing |
| 30, 31 | `InventoryItems` | Added to and removed from an inventory |
| 32 | `ApplyDamage` | Damage |
| 33 | `TryCallFunctionalTree`, `CheckForEvent` | Functional tree |
| 34 | `NotifyBuildModeObjects` | Build mode |

The purposes are read from the callers' names, so treat them as **likely**. 14, 16, 18 to 24, 28 and 29 have no caller by number. Some may be reached through a variable.

### How an object starts running

**Proven for `PostLoad`, read on both builds, and likely for the rest.** Loading a lot:

1. `ObjectModule::DoStream` makes each object, runs `Initialize`, and reads its saved state over the top.
2. `cXObject::PostLoad` sets its location.
3. If its selector needs a reset, it runs `CleanupPeople`, `Cleanup(0)` and then the init tree, entry point 0.
4. Unless the game is in author mode, it runs the load tree, entry point 2, with parameter 0 set to the transition type.
5. Its main tree carries on from the stack.

A selector needs a reset when `objt`'s saved init or main tree version isn't the current one. A version is the root tree's header byte 8 in the high 16 bits, plus the sum of byte 8 over the root and every tree it can reach by calls. **Proven:** that gives exactly the saved value on 504 of the 1,134 type entries, and byte 7 gives none. The other 630 differ by small amounts, because the trees on the disc are newer than the saved houses.

**So most placed objects reset on load.** 2,027 of the 2,610 with a definition are of a type that needs it, and only 583 keep their state as saved. Of the 2,027, 108 have a cleanup tree and 1,919 don't. Without one, `Cleanup(0)` goes through `Reset(true)`, which zeroes the attributes, semi-global attributes and temps and puts main on the stack. It doesn't zero static attributes. So for those objects the saved attributes don't survive, and what matters is what the init and load trees write. A port should run this sequence, not just copy `ObjM`. 1,985 of the objects have a load tree.

A newly created object has no saved frame: `Reset` runs init and puts main on the stack.

**After an error, `Cleanup(0)` with no cleanup tree** picks the object up, calls `Reset(true)` unless misc flag 0x40 is set, and if it was placed, puts it back where it was with `Place`. So an object with no entry point 3 still restarts.

## Loading an object

What a port needs to make a real object from the disc. **Proven** where it says so: each has been tested on the disc, with a wrong offset failing. The rest is from `cXObject`'s constructor and `Initialize` on Wii, and the offsets are in the data so they're the same on PS2.

The tables are `ObjDefinition`, 196 bytes or 98 s16 words, which is why scope 0x15's limit is 0x62, and `ResFile`. A `ResFile` row's `+0` vector points to its `ObjDefinition` rows.

| `ObjDefinition` offset | Meaning |
|---|---|
| `+4` | Stack size for its `TreeSim` |
| `+0x12` | Object type, word 9. 2 is a Sim |
| `+0x14` | Non-zero means multi-tile |
| `+0x1C` | GUID. **Proven** |
| `+0x28` | Slot set ID. Below 1000 it's in the object's own `ResFile`, from 1000 in its semi-global row |
| `+0x74` | Attribute count; 0 means 8. **Proven:** all 1,127 uses of scope 0 fit under it, and other offsets give 169 to 294 overruns. Every placed object's saved list has this length too |
| `+0x76` | Static attribute count (scopes 0x40, 0x41) |
| `+0x82` | Size of the array for scope 0x1B. Only 13 definitions have one (18 or 3) |
| `+0xBC` | ID of its `ObjFnData` record. See "Entry points" |

**Attributes.** A new array of the count, all 0, then a placed object's saved list over the start of it. Static attributes and scope 0x1B's array are made the same way. Scope 0x1B has no use in any expression.

**Semi-global attributes.** The count is the number of strings in `STR#` set 0x100 in the object's semi-global row, and the array starts at 0. **Proven:** all 420 uses of scope 2 fit, and sets 0x101 and 0x102 overrun 202 and 332 times. `tools/bhav.py` reruns this test and the attribute count's. The strings are the attributes' names, like `Bed Type`.

**The `ResFile` row's vectors**, as far as they're known:

| Offset | Holds |
|---|---|
| `+0` | Pointers to `ObjDefinition` rows |
| `+4` | Lists of animation names, 8-byte entries: a vector of pointers to `AnimRefName` rows, and an s16 set ID. See "Which clip Animate's number means" |
| `+0x10` | String sets, 12-byte entries: vector of strings, tag `STR#`, ID in the low 16 bits |
| `+0x1C` | Catalogue text |
| `+0x20` | Constant tables |
| `+0x24` | Tree tables (`VECTOR<TreeTable>`) |
| `+0x28` | Slot sets, by ID |
| `+0x2C` | `ObjFnData`, the entry point tables |
| `+0x34` | Pointer to the semi-global row |

**Variables** (scopes 3 and 4). There are 72 numbers. Six don't exist and are error 7: 0x16, 0x2C, 0x39, 0x45, 0x46 and 0x47. Everything else exists on every object. `Initialize` gives these starting values, and all others start at 0:

| Variable | Starts as |
|---|---|
| 3, 0xE | -1 |
| 0xB | The object's ID |
| 0x17 | 2 |
| 0x18 | 9, the game's start hour |
| 0x2D, 0x2E, 0x2F | 2000, 1, 1: the start year, month and day |
| 0x31 | 50 |
| 0x43 | The number of object slots, not counting slot 0 |

The game saves memory by keeping unchanged variables in a table shared by each object type, and writing a value equal to the shared one removes the object's own copy. That's invisible to a tree, so a port can give each object a plain array of 72. Reading variable 0x19 also clears the idle status, as scope 3 says.

**What else `Initialize` does:** it sets the stack up with the temps as default parameters, loads the slot set, moves the object off the map to (-1, -1), and works out its room score.

## Load details

Each of these is labelled. Offsets are PS2 unless they say Wii.

**Parameter 0 of the load tree. Proven for the route, likely for the names.** It's the transition type, and it goes `EGlobal` to `BGExec_LoadHouse`, then `Neighborhood::LoadHouse`, `ObjectModule::PostLoad` and `cXObject::PostLoad` (`0x2532B8`), which stores it as the first of four s16 parameters and leaves the rest 0. The values are named here from who sets them:

| Value | Set by | Reads as |
|---|---|---|
| 0 | `MMUTarget`'s constructor | Start of the main menu, the default |
| 1 | `CAFTarget::GotoGame` | Into the game from Create-a-Family |
| 2 | `CASTargetTSC6`, `MMUTarget::LaunchSavedGame`, `LSMTarget`'s constructor | From a saved game |
| 3 | `TryLotTransition`, `BGExec_SetCurHouse` | Lot to lot |
| 4 | `LSMTarget::SetVariable` | Keeps the time of the last transition |
| 5 | `UrbzModeNewGameState::Reset` | New game |

`EGlobal::IsTransitionLoad` is true for 3 and 4. In `LoadHouse`, 0, 1, 2 and 5 keep the game's start time, 4 uses the last transition's time, and 3 and 6 use the globals.

**Sims during `PostLoad`. Proven for the disc.** `ObjectModule::PostLoad` only walks the object list, and `ObjM` builds that list. No `ObjM` on the disc holds a Sim: the 2,611 placed objects are types 1, 4, 7, 9 and 11. So a lot straight off the disc has no Sims while its objects load. **Likely:** a saved lot with Sims in its stream has them in the list, and they run `cXPerson::PostLoad` in list order. In author mode, `LoadHouse` calls `CreateFamilyInLot` after `PostLoad`. The Neighbourhood section says where a normal game's Sims come from.

**Misc flag 0x40. Proven.** The word is object `+0xE4`. Only `ObjectModule::KillOutOfWorldObject` sets the flag, and `ReconStream` doesn't save it, so it's clear on every loaded object. It marks an object being killed. During a load, `Cleanup` always goes on to place the object.

**Object IDs. Proven for the rule, likely that 2 never happens on PS2.** `AddObject` (`0x222EA0`), given no ID, starts at 0x401 if the app's game state (`_pApp +0x14C`) is 2, and otherwise at 1. It takes the first free slot from there. The state starts at 0 in `EApp`'s constructor. The only writer is the virtual `SetGameState` (`0x343ED0` and `0x3E3E90`, slot `+0x90`), and the only call to it found on PS2 is in `InLevelBaseState::Update` (`0x265200`), passing 0. So a port starting IDs at 1 is right. The search was for that slot's call pattern, so a call written another way could be missed. Placed objects on the disc have IDs from 1 to 257.

**`Reset`. Proven on PS2 for the variables, likely for the order.** `cXObject::Reset` (`0x20F478`) copies the variables, clears them all, and puts back 0x1D, 1, 0x29, 0x2D, 0x2E, 0x2F, 0x18, 0x15 and 0x43. It sets 0xB to my ID and 0x3A to definition `+0x62`, and later 2 and 3 from my container. On a single tile only, it then runs init, and if its argument is true, it isn't author mode and I have a main tree, one `Simulate`. A multi-tile object doesn't run init here.

So an object with no cleanup tree runs init twice when it resets on load, on a single tile: once in `Reset`, and once in `PostLoad`.

**`Cleanup(0)`. Proven on PS2 for the entry points.** With a cleanup tree (entry point 3), it runs that on me, or on each part of a multi-tile object, then clears variable 0x3E and bit 5 of variable 8, and puts main back on the stack. Without one, it saves my place, calls `Pickup`, then `Reset(true)` unless flag 0x40 is set, then `Place` if I was placed. `Pickup` runs entry point 10 and `Place` entry point 9, so those run during such a cleanup too.

**Type attributes. Proven on the disc.** `GetTypeAttributes` asks `ObjectFolder` for a block by definition `+0xB8`, or by GUID if that's 0. `+0xB8` is 0 on all 1,335 definitions, so blocks are per GUID. They're shared by all objects of that type, and they aren't in `ObjM`.

**With no block. Proven: `CountTypeAttributes` (`0x1DA3D0`) and `GetTypeAttributes` (`0x1DA408`) match byte for byte.** A definition with no block has a count of 0 and a null array. Scopes 0x24 and 0x25 check the index against the count first, so every read or write is error 0x40 and the null array is never touched. A new Sim from `template_person`, with no block, gets error 0x40 for any type attribute.

## Lot transitions

**Proven for the data, likely for the rules.** `LotToLotTransition::Initialize` (`0x1FD60C`) fills a table of 72 transitions, once, from tree constants in the semi-global row of the object with GUID 0xF85F. That's `portal_basic`, so the row is `portal_sg`. The code loads ID 0x2000 to 0x2004 and 0x2006 to 0x2009, and each table has a value per transition, with number 0 unused. The titles are on the disc:

| Table | Title | Field | PS2 entry, 0x18 bytes |
|---|---|---|---|
| 0x2000 | Source Lot ID | Source lot | `+0x0E` u8 |
| 0x2001 | Destination Lot ID | Destination lot | `+0x0F` u8 |
| 0x2002 | Time Cost | Time | `+0x00` |
| 0x2003 | Hygiene Cost | Hygiene | `+0x02` |
| 0x2004 | Energy Cost | Energy | `+0x04` |
| 0x2006 | Energy Requirement | Energy needed | `+0x06` |
| 0x2007 | Body Skill Requirement | Body needed | `+0x08` |
| 0x2008 | Min Body For Discount | Body for the discount | `+0x0A` |
| 0x2009 | Energy Cost Discount | Discount | `+0x0C` |

`+0x10` u8 is the transition's number. Tables 0x2005, 0x200A to 0x200C aren't loaded here, but they're on the disc too: Body Skill Gain, Energy Requirement Discount, Default Locked State and Lock Bit Lookup. 0x200D is the service NPCs' neighbour IDs.

`GetTransitionCost` gives time, hygiene and energy for a transition, and the energy drops by the discount if person data 0x10 is at least the body value. `GetTransitionEnd` gives the destination lot. Whether one's open comes from `IsTransitionOpen` and `HasTransitionBeenUsed`, which aren't read here.

So a port can build this table from the `portal_sg` constants, with no stand-in.

## Neighbourhood

**Proven for the layouts.** Kind 1 of `DEFAULT.NGH` holds `NGBH`, `NBRS` and 6 `FAMI` chunks. They're written by `DoStream` functions, version 0x82, and read here from `Neighbor::DoStream` (`0x201A90`) and `FamilyImpl::DoStream` (`0x1EEBB0`) on PS2, and the Wii code for the rest. Read this way, all 8 chunks are used up to the last byte, and forgetting the string padding breaks that. `tools/bhav.py` reruns it, and `read_neighborhood_chunks` returns the fields.

Each chunk starts with u32 0, u32 version and the tag reversed. Everything is little-endian with no alignment, apart from strings:

- Narrow: bytes, a zero, then padded to an even offset. The pad byte is junk (0xCD).
- Wide: aligned to 2, the bytes FE FF, UTF-16 units, and a zero unit.

**`NGBH`, the neighbourhood:**

| Type | Meaning |
|---|---|
| 36 x s16 | Neighbourhood variables. `Load` reads 0x23 |
| Wide string | Name |
| 2 x s32 | `+0x150` and `+0x154` |
| 128 bytes | Goal and unlock data, `GoalUnlock` |
| s16 count, s16s | Unlocked plans (`PlanManager`) |
| s16 count, then s16 and s32 each | `LotManager`, one per lot, 27 |
| s16 | `LotManager +0xD8` |
| s16 count, bytes | `LotManager +0xDA`, one per transition, 72 (**likely** their state) |
| 100 x (s32, s16, s8, s8) | Weather events |
| s32, s32, 4 x f32 | Weather state |
| s16 count, 2 x s32 each | `LotHelper` |

**`NBRS`, the neighbours:** s32 slots, 88 on the disc. Each is an s32 that's non-zero if a neighbour follows. Each neighbour:

| Type | `Neighbor` | Meaning |
|---|---|---|
| Narrow string | `+0x24` | Name, 64 bytes at most |
| s32 | `+0x14` | 0 on the disc |
| s32 | `+0x00` | Non-zero means person data follows. **Open:** what its value, 5 to 9, means |
| 49 x s16 | `+0x6C` | Person data, only with the word above. The rest of the time it stays at its defaults |
| 16 x f32 | `+0x114` | Motives |
| 16 x f32 | `+0x154` | A second set of motives |
| s16 | `+0x04` | Neighbour ID |
| u32 | `+0x08` | GUID of the Sim's object |
| s32 -1, s32 count | `+0x10` | Relationships: per entry, an s32 that's non-zero if one follows, s32 key, s32 count, s32 values. Empty on the disc |
| Wide string | | Only for chimps, sim type (person data 0x2C) 1: the user's name for them |

A service NPC's sim type comes from its `ObjSelector`, not from the file, so a neighbour with no person data can still be a chimp. Reading the wide marker works, because the next word is a slot's 0 or 1.

The disc has 13 neighbours in the 88 slots, all NPCs, and 7 have person data. Each ID is one more than its slot. All 11 IDs in the service NPC table 0x200D are here, and 40 and 41 as well.

**`FAMI`, the families.** The chunk's ID is the family's number. Bit 0x4000 makes it a `PlayerFamilyImpl` and is then cleared, so 0x4001 is family 1.

| Type | `FamilyImpl` | Meaning |
|---|---|---|
| s32 | `+0x0C` | House number |
| s32 | `+0x10` | Creation order. `Load` sorts by it |
| s32 | `+0x14` | Funds |
| s32 | `+0x18` | Pet points |
| s32 | `+0x1C` | House value |
| s32 | `+0x20` | Friends |
| s32 | `+0x38` | Flags: 1 phone, 2 baby, 4 just moved in, 8 free play, 0x10 finished Create-a-Family |
| s32 count, u32s | `+0x28` | Members, by GUID |
| Wide string | `+0x04` | Name. Replaced when read: `GetNghFamilyName` for a free play lot, otherwise `GetNghHouseName` |
| s16 count, s8 | `+0x3C` | Inventory, then its items. None on the disc |
| s32 count, s32s | | Inventory's last vector |
| 25 x s16 | `+0x5C` | Player families only: stats |

Inventory items, read on Wii only and not tested, because the disc has none, start with a type byte. 0 is an object: s32 GUID, s8, s16 count of s16s, then 3 x s16. 1 is the same, then s16 count of s16 pairs for owners. 2 is a token: 2 x s16. 3 is clothing: s16, then 5 x s8.

### Where the family's Sims come from

**Proven: not from the disc.** The player's family, 0x4001, has no members in `DEFAULT.NGH`. The disc's three families with members hold NPCs only, 7 of them. No Sim the player controls is stored there, and none in `ObjM`, as Load details says.

**Likely, from Wii only.** PS2's functions for this aren't named yet. In a normal game, `UrbzModeNewGameState::Update` does this:

1. `BGCall_LoadDefaultNeighborhood` loads the file above.
2. It takes the family at the start lot, or makes one with `MakeNewFreePlayFamily`, and moves it in with `HoodManager::MoveInFamily`.
3. `Neighborhood::AddNewCharacter` makes the Sim: `ObjectFolder::CreateNewUserSelector` gives a new object definition, `AddNewNeighbor` a new `Neighbor`, and `InitDefaultValues` fills it.
4. `AddToFamily` adds the GUID to the family, and it sets person data 0x25 to 0x1B and sets 0x2B.
5. `SaveCASDataToNeighborhood` stores what Create-a-Sim made. `Neighborhood::Save` then writes the save.
6. `PlayerEnterHouse` and `GotoLiveMode` start play.

Then each load is the path in Load details: `FamilyImpl::CreateFamilyInLot` calls `ObjectModule::CreateNeighbor` for each member, found by `FindNeighborByGUID`. `Load` drops members with no neighbour, and neighbours with no selector. It also adds a neighbour for any person definition without one.

So a port with no Create-a-Sim needs a stand-in Sim: one new definition and `Neighbor`, added to family 1 and marked with data 0x25 and 0x2B. That's the code's own new-game path, without the appearance.

### Where a Sim starts on a lot

**Proven from the trees on the disc and the PS2 code that runs them.** No C++ places a Sim in a normal game. `CreateFamilyInLot` only runs in author mode, and `LaunchNewGame` turns that off. `generator_visit`'s `Function - Load` looks like it would do it, but node 0 returns on both branches, so `Load - Place Family` never runs.

The real path is on the scenario object, `generator_scenario_sp`, with shared trees in `scenario_sg`. Its load tree (entry point 2, parameter 0 the transition type):

1. It stores parameter 0 in semi-global attribute 0xD. That's the **Lot Load Type**, constant table 319: 1 new game from Create-a-Family, 2 save game, 3 lot to lot portal, 4 map screen, 5 dev menu, 6 autotest.
2. `Get in Temp 0 - Lead Transitioner`, which is LotTransition with `+2` set. If there's nobody, it stops there.
3. It stores the lead's ID in attribute 0xC, the lead's neighbour data word 0x2E in 0xE and word 0x2B in 0xF.
4. `Sub - Transition Onto Lot` (`0x203B`).

**`Sub - Transition Onto Lot`:**

- Save game: nothing is placed, unless global 0xA is 1 and neighbourhood variable 6 is 0x10. The Sims are in the saved lot.
- Otherwise `Sub - Establish Geographic Verisimilitude` runs first, for type 1 and all others, then `Sub - Get Transitioner Spawn Point`, then `Sub - Place Neighbor`.

**`Sub - Get Transitioner Spawn Point` (`0x2028`)** leaves the spawn object in temp 0. Objects in a room with a negative ID are skipped, which excludes split diagonals. By load type:

| Type | Order tried |
|---|---|
| 3, 4 (portal, map) | If the lead's word 0x2E is above 0: the `transition_triggerplate` (GUID 0xA09E02A9) whose attribute 1 equals it. Then any plate as below |
| 5, 6 (dev, autotest) | Any plate with attribute 1 above 0, and on a second pass the first whose lot transition flag test (scope 0x44 data 0) is non-zero. Then the first plate in a room |
| 1, 2 (new game, save) | The first `portal_green_arrow` (GUID 0x50323DB5), then the plates as above |

No spawn object at all returns false, and the tree stops. Before returning it clears the spot: if the spawn is a `transition_portal` with variable 0x3E set, it removes any object there that `CallNamedTree` picks out.

**`Sub - Place Neighbor` (`0x2031`).** A Sim with no object is made by CreateObject, as a neighbour, out of the world. Then, with the spawn object as parameter 0:

| Case | Placement |
|---|---|
| No spawn object | FindGoodLocation mode 6, editable tiles only: a random tile |
| Load type 1 or 2 | Not placed here. **Open:** what puts a new game's lead in the world after this |
| Load type 5 with global 0xA at 1 | Not placed here |
| The spawn's category (variable 0x3B) is 0x23, `transition_portal` | PushAction on the Sim: interaction 1 on the portal, so it walks in through `Interaction - Arrive` |
| Otherwise | FindGoodLocation mode 0, flags 3, from the spawn object: the spiral search, not preferring empty tiles |

If placing fails the tree ends with an error. Afterwards it sets person data 0xF to 1 and 0xD to Person Type 0, Family Member.

**`Sub - Establish Geographic Verisimilitude` (`0x202A`)** walks every neighbour, skipping the lead. A neighbour who has an object and isn't on this lot is removed. A family member whose word 0x2F is this lot (global 0xA) and who has no object gets one. It then looks for a Harvestable Source object (category 0x44) with a room, skipping `source_marine_fish` and `source_marine_clamrock`, and tries FindGoodLocation mode 0 from it, then mode 6. If both fail, the Sim is removed.

**For a port:** on arrival by lot transition, put the lead on the `transition_triggerplate` whose attribute 1 is the transition number, using the spiral search from it. That's proven. For a new game the trees only choose the first `portal_green_arrow`, then plates, and don't place the lead themselves, so using that object's tile is a reasonable stand-in, not the game's proven behaviour. The rest of the family use the harvestable source rule.

## LotTransition (opcode 0x45)

**Proven: read line by line on PS2** (`0x23A448`), and `LotManager` compared on both builds where marked. Not matched: 2 words short, parked under `NON_MATCHING`. 11 uses.

The primitive never reads the 72-entry table. It picks who's going and where, and the costs come later.

`+0` and `+1` are small operand codes, the same scheme for both:

| Byte | Reads |
|---|---|
| 0 | My object variable 0xB |
| 1 | The stack object ID |
| 2 to 9 | Temp 0 to 7 |
| 0xA to 0x11 | Local 0 to 7 |
| 0x12 to 0x19 | Parameter 0 to 7 |
| Others | Literal, the byte - 0x7E |

**With `+2` non-zero it only lists.** Temps 0 to 5 are cleared. If the transition type (`EGlobal +0x200`) is 4, temp 0 is the transitioning neighbour (`+0x4C8`). Otherwise temps 0 to 5 get the IDs of the first six neighbours with person data 0x2B set. That's how `Lead Transitioner` finds the lead.

**With `+2` zero it transitions:**

1. `+0` gives the destination lot.
2. The lead is the neighbour whose ID is in my temp 0. With none it's the player's neighbour, which also sets the initial Sim selection. With no player Sim the code calls `GetPrimaryNeighbor` and drops the result, so the lead stays null and the next read would crash: a port should use the primary neighbour there.
3. Person data 0x2B is cleared on every neighbour but the lead.
4. `+1` gives a type. The trees mostly pass literal 5.
5. Globals 0, 1, 5, 7 and 8 are saved in neighbourhood variables 0x15 to 0x19.
6. The transition type becomes 3, and the current lot is saved as the previous lot.
7. `TransitionToLot(destination, type, the lead's word 0x2E)`.

`TransitionToLot` does nothing unless the type is 5. It clears 0x2B on everyone, then sets it to the current lot's number on each ID in my temps 0 to 5. So **0x2B is the lot a Sim left from**, and non-zero means it's in transit. It records the destination and word 0x2E as the transition ID, and moves the family's house number. **Word 0x2E is the transition number**, which is what picks the plate on arrival.

**Costs. Likely, Wii.** `LotManager::PerformLotToLotTransition` is where the table is read. With a transition ID above 0 it calls `GetTransitionCost`, and otherwise `BuildTransitionList` finds a route through the lots. Time then passes in the game clock in steps, with `WeatherManager::AdvanceTime`, and `ApplyDamageOverTimeOnLotLoad` runs. See Lot transitions for the fields.

**The flags.** `LotManager +0xDA` is one byte per transition. Scope 0x44 reads them.

| Bit | Meaning |
|---|---|
| 0 | **Open.** Set at reset where table 0x200B, Default Locked State, is 0, and by events |
| 1 | **Toolkit's `second`.** Likely: Sims other than the player may use it |
| 2 | Used (`TransitionUsed`, `HasTransitionBeenUsed`) |

`IsTransitionOpen(id, both)`: IDs 1 to 0x47 only, and false otherwise. With `both` clear it's bit 0, and with `both` set it's bits 0 and 1 together.

`Reset` sets bit 1 on every transition except 0xF, 0x15, 0x16, 0x19, 0x1A, 0x1F, 0x20, 0x23, 0x24, 0x31, 0x32, 0x39, 0x3A, 0x47 and 0x48, and PS2's list is the same (`0x1FC830`). The table's labels name those as the raft, secret passage, Skull Rock, a basic path, the chasm, the steam vent, the cliff dive and the catamaran. `UpdateAvailability` passes `both` set for a neighbour with a person who isn't the player, and otherwise leaves the table as it is. That's why bit 1 looks like an NPC permission. Some events in `EventHappened` set bits 0 and 1 together.

## Passive influence

**Proven on Wii, and on PS2 no stream code was found:** the map is **not** read back during a load, and it isn't saved.

- `PassiveInfluenceSystem::DoStream` only calls `IsReading`, from version 0x5C, and stores nothing.
- `cFixedWorld::Load` deletes the old `InfluenceMap` and `PassiveInfluenceMap` and makes new empty ones.
- `KillAllObjects` calls `FlushSystem`, which empties the map.

So it's rebuilt by behaviour. Most uses of primitive 0x41 that set one up are in `Function - Load`, entry point 2, which runs in `PostLoad`, with some in `Function - Main`. After reading objects, `ObjectModule::DoStream` calls `MoveObjectInWorld` on each one in the world, to refresh items already there. **For a port:** start with an empty map on each lot load, and let load trees fill it.

## What a RAM snapshot would settle

The code answers most of this, but three things are worth reading from a snapshot taken just after a lot has loaded, before much has run. `tools/livecheck.py snapshot` saves the memory. All offsets are PS2.

To find the objects: scan for a word equal to `0x4BC630` (`cXObject`'s vtable) at object `+0x50`. Any object's `+0xB4` is `ObjectModule`, whose `+0x28` is the first object in the list, and each object's `+0xB8` is the next.

| Question | Read | What it shows |
|---|---|---|
| Is the list in `ObjM` order, with Sims first? | Each object's ID at `+0xC0`, walking the list | Compare with `placed.py list`. Sims and created objects should come first |
| Do selectors really need a reset? | Selector pointer at object `+0xEC`, then bit 1 of the word at selector `+0xAC` | Set on the types `objt` says differ. It stays set while the lot is loaded |
| Did the saved attributes survive? | Pointer and count at `+0x54`, `+0x58` (attributes), `+0x5C`, `+0x60` (semi-global), `+0x64`, `+0x68` (static) | Clam Shell is a good case: a type that resets, with a saved semi-global list starting 94. If the list is all 0 or differs, `Reset` ran. Static lists should still equal `ObjM` |

## What's left

**Open:**
- The bodies of the primitives, except what's above. Each is a function at the address in the table.
- What scope 0x18's virtual call returns, and what reads the flag `0x49DDA4`.
- What `+4` of an expression is for. No code reads it.
- Header byte 7 and type 0x16. Byte 8 is the tree's version, used only to decide resets.
- Which way variable 1 turns an object.
