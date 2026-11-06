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
| -1, 4, other | Failed |

- A node index of 0xFE or 0xFF pops the frame, sets the last result and takes the caller's true or false target, so a call looks like a primitive to its caller. Popping the last frame is error 0x3E9.

**Proven**, from the matched code, for a port's edge cases:

- A new frame's locals aren't cleared. `StackElem::Setup` copies parameters only, so locals hold whatever was in stack memory. A tree that reads a local before writing it is reading old data; a port should zero them.
- A root tree under `Simulate` can't return, because popping the last frame is error 0x3E9. Only `RunCheckTree`'s marker frame makes a return safe.
- `RunCheckTree` leaves the stack and iteration count alone when it fails, and `HandleError` cleans up. A primitive that fails without setting an error, such as 0x26 on a Sim, becomes 0x44F there.

A frame also has a word at `+8`. When a callee returns and it's set, the caller stays on the same node instead of taking a target. **Likely** that's how a primitive that starts a subtree gets run again afterwards; `mWaiting` is our name. Moving to a new node clears it.

`RunCheckTree` runs a whole tree inside one call, with its own iteration count, and returns the tree's true or false. Check trees use it to answer a question without waiting. It pushes a marker frame with no behaviour, so it knows when the tree has returned.

`Behavior::GetNodeRef` keeps recently used trees in `s_cachedTrees`, most recent first; a miss reuses the last slot. A port with everything in memory doesn't need this.

**What an error does. Likely, from both builds:** `cXObject::Error` sets bit 0x100 of the object's flags (`+0xE4`) and a flag in `ObjectModule`. It doesn't stop anything itself; the tree stops because the primitive returns -1. `cXObject::HandleError` runs later: if the flag is set and an error is recorded, it clears the flag, calls `ObjectModule::CleanupPeople` and then the object's `Cleanup(0)`. If the object has a tree for entry point 3, `Cleanup` runs it, sets object data 0x3E to 0, clears bit 5 of data 8, and resets the stack to the tree for entry point 1, on every tile of a multi-tile object. With no such tree it picks the object up, resets it and puts it back. Both paths then set the idle status and quiet the object's sounds. So one error resets that object rather than ending the game.

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

Room data (0x17). Room 0xFFFB is outside; it gives 0, 1, 0, 0 for data 0 to 3 and has no room to look up otherwise.

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
| CallNamedTree | `+2`: 1 global, 2 semi-global, 3 private |
| CreateObject | `+5` flags: bit 0 no duplicate, 1 pass my ID and stack object ID, 2 neighbour in stack object, 3 empty tiles only, 4 pass temp 0, 5 use stack object's direction, 6 use GUID |
| Dialog | `+1` icon index; `+7` bits 1-3 icon type, bits 4-6 result temp, bit 7 semi-global strings |
| DirectionTo, DistanceTo | A flag byte (`+4`, `+2`), bit 0 set: the "from" value is the scope in the next byte and the data after it. Clear: scope 3, data 0xB. DistanceTo bit 1 returns sixteenths of a tile |
| FindGoodLocation | `+2` flags: bit 0 start at relative object, 1 not prefer empty tiles, 2 editable tiles only, 3 orientation in temp 0, 4 prefer under roof |
| Grab | `+0` bit 0 parenting prop bone |
| IdleAnimate | `+2` low byte decrement parameter, high byte sync object; `+5` flags; `+6` behaviour; `+7` literal loop count |
| KillObject | `+2` bit 0 return immediately, bit 1 clear means clean up all |
| PlaySound | `+4` bit 0 looped, bit 1 stack object is source, bit 7 sound in temp 0 |
| PushAction | `+3` flags: bit 0 special icon, 1 use local, 2 continue, 3 carry name over, 4 push to head, 5 run as sub, 6 use temp |
| Relationship, Relationship2 | Flag byte (`+4`, `+2`): bit 0 clear means create, bit 1 use neighbours; Relationship2 also bit 2 assign, bit 3 pet ownership |
| SetBalloon | `+0` bit 0 over stack object; `+2` index, group; `+6` type, then flags |
| SetMotiveDelta | `+3`: bit 0 clear all, bit 1 influence on me, bit 2 influence on stack object |
| SetToNext | `+4` low 4 bits search type, bit 5 GUID from temps, bit 6 skip non-leads; bit 7 means `+5` and `+7` give target owner and data, otherwise it's the stack object |
| Snap | `+4` bit 0 origin only, 1 ask person to move, 2 use footprint |
| TestObjectType | Read from the PS2 code. `+0` s32 GUID; `+4` s16 data and `+6` u8 scope give the object to test, a missing object being error 0x17; `+7` flags. Bit 2: true if its GUID is the one in temps 1 and 2 (high, low). Else bit 0: true if the GUID's selector has the same semi-global namespace. Else bit 1: store the GUID in temps 1 and 2, true. Else: true if its GUID or its master selector's GUID is the one given. An object with no selector is false |
| TreeBreak | `+4` bit 0 log only, bit 1 break all |

The full list is `*Param` in the Wii symbols. `tools/bhav.py` doesn't test any of this table.

`cXObject::TryTestObjectType`'s C++ is in `cXObjectPrims.cpp` under `NON_MATCHING`.

## SetToNext (opcode 0x1F)

**Likely**, but strongly: read from the PS2 function (`0x22BF10`) and the Wii build, which agree in structure. It isn't matched yet. 1,248 uses.

It steps a value through a list of things. The value is the target; `+4` is the flags.

| Operand | Meaning |
|---|---|
| `+0` s32 | GUID for search types 4 and 7 |
| `+4` bits 0 to 3 | Search type |
| `+4` bit 5 | Take the GUID from temps 1 and 2 (high, low) |
| `+4` bit 6 | Skip non-leads: only stop on objects where `IsInteractionTile(true)` is true. Ignored for search type 12 |
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
| Others | | Error 0x1E. With skipping on, only 0, 2 and 6 are valid, because 12 turns skipping off |

Search type 9's object has to be in the same room (object variable 0x1D) as the centre object, and off-grid tiles are skipped. A missing centre object is error 0xA, and a local past the tree's count is error 8.

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
3. Preload only: load it and return.
4. The Sim has a queued operation: return 2.
5. An animation of 0 turns the current one off.
6. If this animation hasn't started, resolve and start it. A sync object that isn't ready gives its own return value.
7. If an animation event is waiting, store its number in the event target and **return 0**, the false branch.
8. Otherwise, if the animation has finished, deliver any end event the same way, or stop and **return 1**, the true branch.
9. Otherwise **return 2** and wait.

So a tree animates in a loop: the false branch handles an event, then comes back to the same node, and the true branch means it finished. With no events to deliver, one play is many ticks of 2 and then a 1. A port without real animation can return 1 at once, or after the animation's length, and trees that wait for events will just see none. The event target is `+2`'s local or parameter 0, and a tree with neither is error 8. End events are negative in the animator (`+0x194`), and with `+5` bit 1 set they're stored as `-1 - value`.

On an object that isn't a Sim, `cXObject::TryAnimate` (`0x234558`) is all there is: behaviour 7 uses me, 8 the stack object, and anything else is error 5. It resolves a skill for passive influence and plays nothing. Its C++ is under `NON_MATCHING`, 69 of 70 words.

## Idle (opcode 0)

**Proven:** `cXObject::TryIdle` byte-matches. 3 uses on the disc.

`+0` s16 is a parameter number, and that parameter is the counter. Past the tree's parameters is error 8.

1. If the object's idle status is 0, the counter is set to 0 and the status to -1.
2. A negative counter becomes 0.
3. A counter of 0 returns 1, the true branch.
4. Otherwise it returns 2, so the node runs again next tick. Normally the counter goes down by 1. If the object can sleep, the counter stays and the status becomes counter - 1 instead.

An object can sleep if it isn't a Sim, its variable 0x19 is 0, and no sprite slot is active. Idle status is a per-object word in `ObjectModule`, and it's what reading variable 0x19 clears, which ends the sleep at step 1. A port can ignore sleeping and always count down: the tick count comes out the same unless something wakes the object early.

## Random (opcode 8)

**Likely**, from both builds; the C++ is under `NON_MATCHING`, 108 of 118 words.

| Operand | Meaning |
|---|---|
| `+0` s16, `+2` s16 | Target data and scope |
| `+4` s16, `+6` s16 | Limit data and scope |

A limit of 0 is error 4. The result is a random number modulo the limit, taken as unsigned, so a negative limit is a very large one. It's written through `InterpWriteValue`, or to the float for a motive, and with neither address it's error 0x13. It returns 1.

The generator is at `0x262AF0`: `state = state * 0x41C64E6D + 0x3039`, returning bits 16 to 47 of the 64-bit result of that multiply and add. State is 32 bits, at `0x5112E8`.

## CheckObject (opcode 0x35)

**Likely**, from both builds; the C++ is under `NON_MATCHING`, 131 of 189 words.

`+0` data and `+2` scope give an object ID, and `+4` s16 is the test. Reading the value can fail with the usual errors. Any other test is false.

| Test | True when |
|---|---|
| 0 | The object is the player's Sim and it has a player interactor that isn't autonomous |
| 1 | The object exists |
| 2 | The object exists and is a Sim |
| 3 | A Neighbor has that ID |
| 4 | Always true. Temp 0 is a relationship type, 1 to 9, or error 0x14. It's replaced by how many relationships of that type the Sim has, or 0 |
| 5 | The object is the player's Sim and it has no player interactor |

## What a placed object starts with

`ObjM` is the saved state of a running lot, so a placed object arrives with almost everything a tree reads. `docs/formats/world.md` has the layout, and `tools/placed.py` reads it. **Proven** on all 2,611 objects: each ends exactly on its stream mark, and `placed.py check` has the controls. An earlier reader was misaligned from the attribute lists on, which is why the variables and attributes seemed to be missing.

- **Variable 0xB is the object's own ID.** `Reset` and `Initialize` both copy `mID` (`+0xC0`) into it, and it equals the ID on all 2,611 placed objects. So "my variable 0xB" is "me", and a tree that gives it to `TestObjectType` is testing itself.
- **Attributes are in `ObjM`.** They aren't only set by trees. Make the arrays from the counts below, then copy the saved lists over the start of them. The rest stay 0. Non-zero attributes are on 168 objects, semi-global on 1,488, static on 407.
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

**Likely, from Wii.** Loading a lot:

1. `ObjectModule::DoStream` makes each object, runs `Initialize`, and reads its saved state over the top.
2. `cXObject::PostLoad` sets its location.
3. If its selector needs a reset, it runs `Cleanup(0)` and then the init tree, entry point 0.
4. Unless the game is in author mode, it runs the load tree, entry point 2, with parameter 0 set to the transition type.
5. Its main tree carries on from the saved frame.

A selector needs a reset when `objt`'s saved init or main tree version isn't the current one. The version is `GetCumulativeTreeVersion` over the entry point's tree. A newly created object has no saved frame: `Reset` runs init and puts main on the stack.

`Reset` also zeroes the attributes, semi-global attributes and temps, but not static attributes.

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

**Semi-global attributes.** The count is the number of strings in `STR#` set 0x100 in the object's semi-global row, and the array starts at 0. **Proven:** all 420 uses of scope 2 fit, and sets 0x101 and 0x102 overrun 202 and 332 times. The strings are the attributes' names, like `Bed Type`.

**The `ResFile` row's vectors**, as far as they're known:

| Offset | Holds |
|---|---|
| `+0` | Pointers to `ObjDefinition` rows |
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

## What's left

**Open:**
- The bodies of the primitives, except what's above. Each is a function at the address in the table.
- What scope 0x18's virtual call returns, and what reads the flag `0x49DDA4`.
- What `+4` of an expression is for. No code reads it.
- Header bytes 7 and 8, and type 0x16.
