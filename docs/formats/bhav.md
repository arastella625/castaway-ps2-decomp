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
| Dialog | `+1` icon index; `+7` bits 1-3 icon type, bits 4-6 result temp, bit 7 semi-global strings |
| DirectionTo, DistanceTo | A flag byte (`+4`, `+2`), bit 0 set: the "from" value is the scope in the next byte and the data after it. Clear: scope 3, data 0xB. DistanceTo bit 1 returns sixteenths of a tile |
| FindGoodLocation | `+2` flags: bit 0 start at relative object, 1 not prefer empty tiles, 2 editable tiles only, 3 orientation in temp 0, 4 prefer under roof |
| Grab | `+0` bit 0 parenting prop bone |
| IdleAnimate | `+2` low byte decrement parameter, high byte sync object; `+5` flags; `+6` behaviour; `+7` literal loop count |
| KillObject | `+2` bit 0 return immediately, bit 1 clear means clean up all |
| PlaySound | `+4` bit 0 looped, bit 1 stack object is source, bit 7 sound in temp 0 |
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

So a tree animates in a loop: the false branch handles an event, then comes back to the same node, and the true branch means it finished. With no events to deliver, one play is many ticks of 2 and then a 1. A port without real animation can return 1 at once, or after the animation's length, and trees that wait for events will just see none. The event target is `+2`'s local or parameter 0, and a tree with neither is error 8. End events are negative in the animator (`+0x194`), and with `+5` bit 1 set they're stored as `-1 - value`.

On an object that isn't a Sim, `cXObject::TryAnimate` (`0x234558`) is all there is: behaviour 7 uses me, 8 the stack object, and anything else is error 5. It resolves a skill for passive influence and plays nothing. It matches the game.

## Idle (opcode 0)

**Proven:** `cXObject::TryIdle` byte-matches. 3 uses on the disc.

`+0` s16 is a parameter number, and that parameter is the counter. Past the tree's parameters is error 8.

1. If the object's idle status is 0, the counter is set to 0 and the status to -1.
2. A negative counter becomes 0.
3. A counter of 0 returns 1, the true branch.
4. Otherwise it returns 2, so the node runs again next tick. Normally the counter goes down by 1. If the object can sleep, the counter stays and the status becomes counter - 1 instead.

An object can sleep if it isn't a Sim, its variable 0x19 is 0, and no sprite slot is active. Idle status is a per-object word in `ObjectModule`, and it's what reading variable 0x19 clears, which ends the sleep at step 1. A port can ignore sleeping and always count down: the tick count comes out the same unless something wakes the object early.

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
| `+0x16` s16 | -1 on the master. On a part, its place: y in the high byte, x in the low byte |
| `+0x1C` u32 | GUID. The master's GUID is just the master definition's GUID |

1. A definition with `+0x14` of 0 is its own master.
2. Otherwise it walks every selector for one with the same file (selector `+4`, the namespace ID), the same `+0x14`, and `+0x16` of -1.
3. If none is found, it's its own master.

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

The change is the value times 1/1800, so it's per tick.

**On a non-Sim**, neither influence bit is -1 at once. With one set, it goes to `PassiveInfluenceSystem`: clear all calls `ClearInfluenceMotiveDeltas`, and otherwise `SetInfluenceMotiveDelta` records motive, change, limit and type for that object.

**On a Sim**, an influence bit means the non-Sim path. Without one, it's the Sim's own list at `+0x200`, 12 bytes an entry: motive, change, limit. Clear all empties it. It holds 16, and a full list quietly drops the new one. All paths without an error are **true**.

## Routing

What's here comes from the PS2 code where it gives an address, and the names come from the Wii build. **Proven** means read on PS2 or tested on the disc. **Likely** means read on Wii only, with PS2's function found by its calls but not compared line by line. Below `TryGotoRoutingSlot` and `TryGotoRelative`, only `RoutingSlot`'s small methods are matched: `WriteSlot`, `AllowDirection`, `SetFacingDirection`, `SetDistances`, `SetTileDistances`, `GetXOffset` and `GetYOffset`. Its constructor isn't: it writes a vtable at `0x4B7FC0`, and the name in `symbols.txt` is on `0x4B8020`, which needs sorting out with the `Slot` base class first.

| Function | PS2 | Wii |
|---|---|---|
| `cXPerson::GotoRoutingSlot` | `0x1AA008` | `800C57xx` |
| `Routing_Initial` | `0x1AA6D0` | `800C5D08` |
| `Routing_WaitingForPortal` | `0x1AA980` | `800C5F78` |
| `Routing_Walking` | `0x1AAAC8` | `800C60DC` |
| `Routing_Trapped` | `0x1AAC70` | `800C6320` |
| `Routing_Failed` | `0x1DC708` | `800C64F4` |
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
| `+2` bit 2 | Only tiles that may be edited (tile flag 0x20) |
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
| 6 | **Random:** 10 tries at random tiles, the middle of the tile. With bit 2 it needs flag 0x20, and when preferring empty the tile must have no object | False |
| 7 | **Search along temp 1 as the direction**, modulo 8 | 0x3D |
| 8 | **Multi-tile, every turn:** it tries directions 0, 2, 4 and 6 and keeps the nearest good tile. It tries with bit 4 if set, then without, then with stay-in-room off | False |

Directions must be even or -1, or it's error 0x5F. Modes 3, 4 and 7 use `CanPlace` again after the search, then `Place`. After a good placement the function writes back an action (temps aren't touched): 1 for mode 2, 2 for mode 1, 3 for mode 0, 4 for modes 3, 4, 6, 7 and 8, 5 for mode 5.

**The search** (`FindGoodLocation`). It starts at the given tile, or the cursor without one, kept to 1 and size - 2, on level 1 if none is given. Stay in room is on, and it's the start tile's room. From offset (0, 0) it tests each tile:

1. Out of bounds: not good, and it counts. It stops once (width + height) x 4 tiles in a row are out of bounds. Going past -0x40 on either axis also stops it.
2. Editable only: tile flag 0x20 is needed.
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

`GotoRoutingSlot` then runs. State 0 or 0x15 starts a route:

- A multi-tile target with a TS2 routing slot is checked first: a part whose definition has `+0x3A` above 0 means the route can go ahead.
- No object, or one that isn't in the world, returns 0.
- The route (`XRoute`, 0xA4 bytes) goes on the Sim's list at Wii `+0x5C4`, unless the Sim is already facing the target and is there, which returns 1 at once.

Each tick, a loop calls the handler for the state until one says it's done. Handlers return the next state, and set the tree's result. It starts as 2, wait.

| State | Handler | What it does |
|---|---|---|
| 1 | Initial | Interrupted is route result 4, then state 7. A Sim sitting on the wrong chair runs the stand tree, state 5. Otherwise it builds the goals, then state 0x12 |
| 2 | WaitingForPortal | Waits to pass through a portal |
| 3 | Walking | Interrupted or nothing left to walk is state 0xB. Otherwise it asks the animator: 1 is state 9, 0 state 0xA, 2 is wait this tick |
| 4, 5 | WaitingForSit, WaitingForStand | Wait on the animation |
| 6 | Trapped | Asks others to move, up to a trap count, then state 7 or 0x10 |
| 7 | Failed | Result 1 if none was set, cleans up, **returns 0**, the false branch |
| 8 | Succeeded | Result 0, turns to face if the slot says so, cleans up, **returns 1**, the true branch |
| 9 | WalkingSucceeded | State 8 once the animator agrees |
| 0xA | WalkingFailed | State 6 once the animator agrees |
| 0xB | WalkTerminated | State 7 once the animator agrees |
| 0xC | FoundPath | Starts the walk, then state 3 |
| 0xD | Waiting | Waits for a timer, or fails if interrupted |
| 0xE | WaitingForPersonToMove | Waits on the Sim asked to move |
| 0xF | RouteInvalidatedWhileWalking | Replans |
| 0x10, 0x11 | NeedFailureFeedback, WaitingForFailureFeedback | Runs the object's failure tree, if `+4` allowed it, then state 7 |
| 0x12 | FoundGoals | Waits for a routing turn: a queue allows so many Sims a frame. Then a path: 0 is state 6, 1 state 8, 2 state 0xC, 3 waits for a portal, others state 7 |
| 0x13, 0x14 | ObjectDeleted, ObjectDeletedWhileWalking | The target went away: state 7 |

Person data 0x1C ends up holding the route's result: 0 worked, 1 failed, 4 interrupted. If the target leaves the world part way, the state changes to 0x13 or, while walking, 0x14.

**For a port:** no pathfinding is needed for trees to run. Wait a tick or more, put the Sim on the slot, and return 1. Return 0 for a slot that can't be reached.

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
| 2 | 2 | Stack object is the icon of my current action. Missing is error 0xA, not a Sim error 0x1C |
| 3 | 1 | If the stack object is the selected Sim, asks the game flow for 0x20, otherwise error 0x14. Not a Sim is error 0x1C |
| 4 | 8 | Moves the neighbour in the stack object into family t1, or the selected family if t1 is 0 or less. Makes the family if needed. Copies the last name |
| 5 | 1 | Merges family t0 into the selected family: funds and inventories |
| 6 | 6 | Removes the neighbour in the stack object from its family. False if that fails |
| 7 | 1 | Goal and unlock data call |
| 8 | 11 | Loading screen. t0 non-zero starts one and returns 1. Otherwise it returns 4 until the iris transition is in, then 1 |
| 9 | 24 | Finishes loading: clears `_gd +0x9C`'s word `+0x10`, then `LoadFinishing` |
| 0xA, 0xB | 4, 5 | Adds me to, or removes me from, ObjectModule's list at `+0x2030`. Buy and build are off while anything is on it |
| 0xC | 1 | Tribe gets all tools |
| 0xD | 2 | `CleanupPeople` on the stack object, inlined: every person but the stack object gets `Cleanup(stack object)`, vtable `+0xE0`. Missing is error 0xA |
| 0xE | 3 | Global 0x1F = t0. Globals are s16s from simulator `+0x38` |
| 0xF | 52 | Needs me to be a Sim, or error 0x1C. Person data 0x20 = t0, 0x21 = 0. With t0 of 0, also `ComputeRect` from my location |
| 0x10 | 6 | Game sequences, by t0: 0 starts sequence 2, 1 sequence 3, -1 and -2 wait for them, 2 is game over. Waiting returns 4 |
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
| 0x22 | 5 | Starts a lot change to lot t2 |
| 0x24 | 1 | Replenishes the tribe, t0 |
| 0x25 | 5 | A flag = t0 != 0: `_globals +0x4C4` on PS2, a byte at `+0x3FB` on Wii |
| 0x26 | 2 | t1 = 1 if object t0 is in the world, else 0. False if there's no object |
| 0x27 | 1 | `AutonomousFoodSelection` on t1, t2, t3. Its result |
| 0x29 | 4 | Only checks: t0 above 1 is error 0x14, and not being a Sim is error 0x4C |
| 0x2C | 17 | Closes modeless dialogs |
| 0x2D | 30 | Controller rumble, for the player's Sim only. `+4` u16 is the effect, 0 is none, and t0 is an object, or -1 for none |
| 0x34 | 1 | t2 = 0 |
| 0x35 | 0 | t0 = the stack object's GUID modulo 0xF08A. Missing is error 0x33. **Proven, PS2.** No tree uses it |
| 0x36 | 4 | Returns 0 whatever t7 holds |
| 0x37 | 1 | Moves neighbour t0 into the first free family, and t1 = its number. Missing is -1 |
| 0x38 | 1 | Returns 0 |
| 0x39 | 9 | Clears cached catalog resources of the stack object |
| 0x3A | 4 | Adds or removes the wall between objects t0 and t1. t2 of 1 adds |
| 0x3B, 0x3E, 0x3F | 1 each | Plans: toggles all in the UI, ignores requirements, forces unlock |
| 0x3C | 12 | Is the stack object's plan unlocked? Missing is error 0x17 |
| 0x40 | 2 | Neighbour t0's lot, t1 = 0 if it's here |
| 0x41 | 3 | True if the tribe has token t0 |
| 0x42 | 7 | t1 = a goal value for t0. False if there's none |

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
