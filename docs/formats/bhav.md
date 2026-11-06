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

A target is a node index, or one of three special values. All 123,186 targets on the disc are in range.

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

**Calls.** The 8 operand bytes are four s16 parameters. All FF means "pass nothing": the call uses the object's default parameters (`TreeSim::mDefaultParams`). 6,887 of the 10,744 calls do this.

## Running a tree

**Proven.** `TreeSim` holds a stack of frames. Each frame has the tree, the current node, the stack object, and that tree's parameters and locals as s16 values. Locals sit straight after parameters, and each frame is rounded up to a multiple of four values.

`TreeSim::Simulate` is the tick. It clears the iteration count and calls `DoNodeAction` on the top frame until something stops it:

| `DoNodeAction` returns | Meaning | `Simulate` |
|---|---|---|
| 0 | Keep going in this tick | loops |
| 1 | Stop until the next tick | returns 1 |
| 2 | Failed | returns 0 |

For each node, `DoNodeAction` adds one to the iteration count. At 25,000 in one tick (`sMaxIterations`, `0x492418`) the tree stops with error 0x44D, which catches infinite loops. Then:

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

A frame also has a word at `+8`. When a callee returns and it's set, the caller stays on the same node instead of taking a target. **Likely** that's how a primitive that starts a subtree gets run again afterwards; `mWaiting` is our name. Moving to a new node clears it.

`RunCheckTree` runs a whole tree inside one call, with its own iteration count, and returns the tree's true or false. Check trees use it to answer a question without waiting. It pushes a marker frame with no behaviour, so it knows when the tree has returned.

`Behavior::GetNodeRef` keeps recently used trees in `s_cachedTrees`, most recent first; a miss reuses the last slot. A port with everything in memory doesn't need this.

Error codes seen so far:

| Code | Meaning |
|---|---|
| 0x005 | Primitive not in the `TryElement` switch |
| 0x00D | Bit number above 16 in an expression |
| 0x013 | An expression tried to write to something it can't |
| 0x026 | Division by zero |
| 0x3E8 | A call failed, though its tree loads |
| 0x3E9 | Stack underflow |
| 0x44C | Target 0xFD reached, or a bad transition |
| 0x44D | Over 25,000 iterations in one tick |
| 0x44E | Node or tree missing |
| 0x44F | A check tree didn't finish |

## Primitives

**Proven** for the numbers and addresses, and for the switch as a whole; the name is the Wii function at the same place.

The switch is `cXObject::TryElement` (`0x234678`), 73 cases, and `cXPerson::TryElement` (`0x1AD720`), which handles the Sim-only opcodes and passes everything else to the first. The names are from pairing the two platforms' jump tables, and they hold up: of the 56 cases that call a function, 53 keep exactly the same order in the Wii and PS2 binaries. The other three are explained: `TreeBreak`'s case is inlined, and `KillSounds` and `HUDData` are tiny functions placed elsewhere. Nine names also agree with the name list, which was made from different evidence.

On a non-Sim object, an opcode with no `cXObject` handler is error 5. Opcode 0x26 on a Sim returns -1 at once. Five primitives are inlined in the switch rather than being functions; `WantsAndFearsNotify` and `ChangeWantFearTree` do nothing on Wii.

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

**Proven** for the layout: all 36,093 are in range, and the PS2 code reads these offsets directly.

| Operand offset | Type | Meaning |
|---|---|---|
| 0 | s16 | Left value |
| 2 | s16 | Right value |
| 4 | u8 | Not read by `TryExpression` on Wii. 0, 0xFE or 0xFF on disc |
| 5 | s8 | Operator, 0 to 0x12 |
| 6 | s8 | Left scope |
| 7 | s8 | Right scope |

**Likely** for what each operator does. The Wii code is read case by case, and the order of the cases in the jump table is the same on both platforms for all 19.

| Op | Meaning | Returns | Uses |
|---|---|---|---|
| 0x00 | left > right | the comparison | 1,441 |
| 0x01 | left < right | the comparison | 901 |
| 0x02 | left == right | the comparison | 7,905 |
| 0x03 | left += right | true | 1,125 |
| 0x04 | left -= right | true | 403 |
| 0x05 | left = right | true | 19,584 |
| 0x06 | left *= right | true | 209 |
| 0x07 | left /= right; right 0 is error 0x26 | true | 192 |
| 0x08 | Test bit `right` of left; bits count from 1 | the bit | 545 |
| 0x09 | Set bit `right` of left | true | 880 |
| 0x0A | Clear bit `right` of left | true | 435 |
| 0x0B | ++left, then left < right | the comparison | 194 |
| 0x0C | left %= right; both made positive first | true | 104 |
| 0x0D | left &= right | true | 11 |
| 0x0E | left >= right | the comparison | 806 |
| 0x0F | left <= right | the comparison | 568 |
| 0x10 | left != right | the comparison | 549 |
| 0x11 | --left, then left > right | the comparison | 239 |
| 0x12 | left \|= right | true | 2 |

Comparisons don't write. Everything else writes through `InterpWriteValue`, and a scope that can't be written is error 0x13. Scope 0xE, 0xF and 0x11 (motives) are floats underneath: arithmetic goes through the float, and the result is clamped to a limit of 100.0 at the top; the bottom limit is likely -100.0.

## Scopes

A value is a scope and a data word, read by `cXObject::InterpValue` (`0x235968`) and written by `InterpWriteValue` (`0x238860`). There are 69 scopes. Scope 7 is the commonest, at 17,203 of 72,186 operands.

**Likely** for these, read from the Wii code. The PS2 jump table has 69 entries in the same order, except that 0x27, 0x2A, 0x42 and 0x43 sit elsewhere, so don't trust those four from Wii alone.

| Scope | Value | Limit on data |
|---|---|---|
| 0x00 | My attribute | object's attribute count |
| 0x01 | Stack object's attribute | its count |
| 0x02 | My semi-global attribute | its count |
| 0x03 | My object variable (`EdithVariableSet`) | known variable |
| 0x04 | Stack object's variable | known variable |
| 0x05 | Stack object's semi-global attribute | its count |
| 0x06 | Global (`cSimulator::GetGlobalRef`) | below 0x30 |
| 0x07 | **Literal**: the data word itself | none |
| 0x08 | My temp | below 8 |
| 0x09 | Parameter of this tree | parameter count |
| 0x0A | Stack object's ID. Data must be 0 | 0 |
| 0x0B | My temp, indexed by temp `data` | below 8, both times |
| 0x0E | My motive. Sims only (object type 2) | below 16 |
| 0x0F | Stack object's motive | below 16 |
| 0x10 | Stack object's slot contents, as an object ID | |
| 0x11 | Motive indexed by temp `data` | temp below 8, motive below 16 |
| 0x14 | My slot contents, as an object ID | |
| 0x19 | **Local** of this tree | local count |
| 0x1A | Tree constant (`XObjLang::GetConstantsID`); data is the constant's ID | |
| 0x1B | Attribute of the stack object, indexed by temp `data` | |
| 0x12, 0x13 | Sim data fields, mine and the stack object's | |
| 0x20, 0x26, 0x2E | Neighbour data, through `Neighborhood::FindNeighborByID` | |
| 0x24, 0x25 | Type attributes, mine and the stack object's | |
| 0x28 | Whether a lot has been visited (`LotManager`) | |
| 0x29 | Goal and unlock data | |
| 0x40, 0x41 | Static attributes, mine and the stack object's | |

**Open:** the other scopes. They are all in the switch at `0x4BACB0`, and 0x2F to 0x3F look like tile and room data.

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
| TreeBreak | `+4` bit 0 log only, bit 1 break all |

The full list is `*Param` in the Wii symbols. `tools/bhav.py` doesn't test any of this table.

## What's left

**Open:**
- The bodies of the primitives themselves, except what's above. Each is a function at the address in the table.
- `InterpValue` scopes not listed, and what `+4` of an expression is for.
- Header bytes 7 and 8, and type 0x16.
- Tree constants: how `GetConstantsID` finds them, and whether a set holds any on PS2.
