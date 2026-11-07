# Handoff: The Sims 2: Castaway (PS2) decompilation → PC port

You're picking up a hobby reverse-engineering project. Read this first, then `castaway_ps2_re_kit/README.md`.

## Goal

1. **Short term:** Reverse-engineer the US PS2 build of *The Sims 2: Castaway* in Ghidra: name the functions, recover the types, understand the systems.
2. **Medium term:** Decompile it into C++ source that rebuilds, ideally byte-matching.
3. **Long term:** A native, enhanced PC version. The decompiled simulation logic would be a library, with **Godot** as the likely front end for rendering, input and audio, via GDExtension. A thin SDL + OpenGL/Vulkan renderer is the fallback.

The owner chose PS2 over Wii on purpose. As far as we know nobody has decompiled the PS2 version, and they own the PS2 disc. Don't steer back to the Wii build. Use it as a **reference**.

## Hard rules

- **Never commit or distribute game data.** That covers the executable (`SLUS_216.64`), disc images, models, textures and audio. Repos hold code, scripts, symbol/name lists and docs only. Any future port must extract assets from the user's own disc at runtime.
- Keep `.gitignore` covering `*.iso`, `SLUS_*`, `*.elf`, `*.bin` and any extracted-asset folders.
- Treat every name as a hypothesis until it's been checked against the code.

## Key facts established so far

**Target binary:** `SLUS_216.64`, the US PS2 build. The owner has it locally at `D:\tools\castaway_work\game`, outside this kit; `pkg/target.sha1` holds its checksum.
- ELF32 MIPS (Emotion Engine / R5900), entry point `0x100008`, **stripped**: no symbol table or debug sections.
- `.text` runs from `0x00100000`, size `0x38C604` (about 930k instructions).
- It also has `.vutext` (vector unit microcode) and many `.DVP.overlay.*` sections. That's VU1 rendering microcode, which doesn't port directly and will need a rewrite.
- Built with SN Systems ProDG, which is GCC-based. There's a `.gcc_except_table` section and the strings mention the ProDG Tuner. The exact compiler version **isn't identified yet**, and a byte-matching decomp needs it.
- Build string: `PS2 The Sims 2 Console Build B.08.09.0`. Leftover file paths point to `D:/Sandboxes/cronus/Urbz2/...`, so it's built on the same codebase as The Urbz. EA libraries present: Apt (Flash-like UI), EASTL 1.03.00, the EA `common/engine` code.

**Reference: the Wii decomp `simdecomp/ts2c`** (github.com/simdecomp/ts2c)
- Dormant since May 2022, 9 commits. It holds split PowerPC assembly only (472 files), with no matched C yet.
- It's valuable because it has the **full symbol table**: `symbols.txt`, about 49.8k entries, including 27,483 named functions and the original source file names (`game/simulator/core/unity.cpp`, `game/cas/unity.cpp`, and so on). The game was compiled as "unity" builds, a few huge translation units.
- The Wii build string is `Wii The Sims 2 Console Build B.08.09.0`. **It's the same build as the PS2 one**, so the logic matches closely.
- Names are CodeWarrior/cfront-mangled, e.g. `GetSkinTextureName__17CasSimDescriptionCFQ23Sim20eSimPartsMapLocationb`. `c++filt` doesn't handle them; `tools/match.py` has a small demangler, `cw_demangle()`.

## What has been produced (`castaway_ps2_re_kit.zip`)

| File | Purpose |
|---|---|
| `castaway_ps2_names_ghidra.txt` | 396 names to apply: 218 high, 69 medium, 109 propagated. Format: `<ps2_hex_addr> <confidence> <Namespace::Name> <wii_mangled>` |
| `castaway_ps2_names.csv` | Every candidate with its evidence, including the `review` (about 290) and `low` tiers that aren't auto-applied |
| `wii_function_reference.csv` | All 27,483 Wii functions: address, readable name, mangled name, source file, size and the strings each one references |
| `ApplyCastawayNames.java` | Ghidra script: creates the function if missing, sets the namespace and name (SourceType.IMPORTED), adds a plate comment with the Wii symbol, and bookmarks under "Castaway names". It skips functions the user has renamed. **Not yet compiled or run in Ghidra**, so fix any API errors when it's first run. |
| `tools/wii_refs.py` | Parses the ts2c asm into a map of function → referenced strings and called functions |
| `tools/ps2_refs.py` | Capstone pass over the SLUS: finds function starts (`jal` targets and the code after `jr ra`), resolves `lui`+`addiu`/`ori` string refs, records `jal` calls |
| `tools/match.py` | Matching: weights shared strings (rarer and longer count more), requires a mutual best match, sorts into tiers, sends size-ratio outliers to `review`, then propagates names through the call graph (position-aligned callees when call counts are equal) |

**Compiler (step 5, done for `game/cas`):** SN Systems ee-gcc 2.95.3 build 1.36 at `-O2` (`ee-gcc2.95.3-136` in decompme/compilers). Three `SimBodyPart` methods compiled with it were byte-identical to the game at first test (115 of 115 comparable words, with 3 relocated words excluded), and 825 functions match now. SN 2.95.3 v1.14 and 2.95.2 v2.73a do not match. Sony 2.96 was not fairly tested, because my flags didn't target the R5900.

Flags, from 315 matched functions: `-O2 -G0 -fno-rtti -fno-exceptions`. `-G0` is proven: float constants assemble to `lui`/`ori`/`mtc1` only when `as` gets `-G0`, and string addresses are `lui`/`addiu`, not gp-relative. `-fno-rtti` is likely, because slot 0 of every vtable is null. `-fno-exceptions` is proven: with exceptions on, the compiler adds cleanup code around a call made while a temporary is alive, and the game has none (`SimBodyPart::GetIconTextureID`). Still open: `-O2` and `-O3` gave the same code. Only one source module has been tested; other modules may use other flags, and EA and Sony libraries may have been built separately.

**Assembler: SN's `Ps2EeAs.exe`, not GNU `as`.** The compiler package ships both. The game has a `nop` after every `mtc1` (moving an integer into a float register); SN's assembler inserts that and GNU `as` doesn't under any flags (`CasSimDescription::ApplyDamage` is the test case). `tools/verify.py` and `rebuild/rebuild.py` use `Ps2EeAs.exe` since 2026-11-06, and every function still matches. It runs under Wine when given only `-G0`: `-EL`, `-mips3` and `-mcpu=r5900` make it crash with a stack overflow while it reports them, which is why it looked broken before.

**Short loops: the game's assembler is missing, and a filter stands in for it.** The PS2's processor has a fault with loops of a few instructions, and assemblers work around it by padding such loops with `nop`s. The game's loops are padded until the closing branch is 4 instructions after the top of the loop (about 1,500 loops, plain and branch-likely alike). The build 1.36 and 1.14 assemblers pad to 5, and the 2.73a `ps2eeas` doesn't pad. So the game's assembler is a different `ps2eeas` build from the three we have (1.36's reports version 1.9.25.758), and without help any function with a short loop comes out one `nop` too long. `tools/asm_filter.py` supplies that help; see below. The parked near misses are worth rechecking if a later toolchain ever turns up.

**What the research found (2026-11-06, web search only, nothing downloaded).** The assembler we need is not publicly available, and other projects have stopped looking for it:
- Our three packages are the two archived ProDG releases: ProDG 2.0 (`ps2eeas` 1.8.19.316, March 2001, no padding) and ProDG 3.01 (`Ps2EeAs` 1.9.25.758, November 2002, pads to 7), plus build 1.14 (`ps2eeas` 1.9.6.516, February 2002, pads to 7). The game was built on 20 August 2007 (`EOR/Maxis Engine v3.0 built ... Aug 20 2007` in `.rodata`), nearly five years after the newest of them.
- The Ratchet & Clank 3 decomp (`OpenRAC/rac3-uya-decomp`), which uses the same compiler build 1.36, describes the identical rule: the retail assembler "pads every loop to at least 6 instructions (counting the branch and its delay slot) with nops right before the backward branch", GNU `ee-as` never pads, and `Ps2EeAs` pads to 7. It does not name the retail assembler.
- A catalogue of SN ee-gcc behaviour from another decomp (`Ketbome/decomp-skills`, `toolchain-forensics.md`) calls it "SN's later assembler, not available".
- splat issue 525 records the same padding mismatch; the maintainer's answer is that it needs the right assembler or a patching tool.
- What those projects do instead: a filter over the compiler's assembly output that pads each short loop itself and writes the closing branch as a raw `.word`, so the assembler sees data and doesn't pad again (`tools/asm_filter.py` in the Ratchet & Clank 3 repo).

Checked here: that workaround works with our `Ps2EeAs`. With three `nop`s and a `.word` branch written by hand, the loop in `CasSimDescription::GetValidClothingSetIndexes` assembles to the game's layout. The rule also holds across this game: about 1,500 loops are exactly 6 instructions with padding before the branch, and only 24 are shorter, all between `0x350000` and `0x3A0000` or above `0x470000`, which is library code built with other tools. **The filter (`tools/asm_filter.py`, built 2026-11-06).** `verify.py` runs it between `cc1plus` and the assembler, so `rebuild.py` and `verify_vtables.py` get it too. It is our own code and it stands in for one behaviour of a tool we don't have; nothing else about the build is emulated. For each backward branch within four lines of its label, it assembles a probe copy of the file to measure the loop's real length (one line of assembly can become several instructions, so counting lines isn't enough). A loop of six instructions or fewer gets its `nop`s and has its branch written as a `.word`; longer loops are left as they were. A short loop it can't handle (a branch outside `.set noreorder`, or one it can't encode) stops the build instead of passing through. Evidence that it is right: all 825 earlier functions still match with it on, `CasSimDescription::CopyFrom` (a loop of exactly six, which the assembler alone pads to seven) and `SimBodyPart::GetRequiredDyesForTextureVariation` (two `nop`s added) match only with it, and assembling `CopyFrom` without it gives 680 bytes where the game has 672. `python3 tools/asm_filter.py --self-test` checks the rules without a compiler, and CI runs it.

**A second difference is waiting behind it: float division.** The game has 0, 1, 2 or 3 `nop`s directly before `div.s` (130, 131, 349 and 10 times) and almost always 2 before `sqrt.s`. The other project found the same and could not derive a rule; it copies the count for each `div.s` from the original. No function matched so far contains a float division, so this hasn't bitten yet.

**Matching workflow:** decompiled code is in `src/`, addresses in `config/symbols.txt`, and `tools/verify.py` builds and byte-compares every function. `docs/STANDARDS.md` has the rules and the command. `pkg/tools/matching` is the older harness that compared four compilers and twelve flag sets; keep it for testing other modules.

To run the older harness: it is in `pkg/tools/matching`. Set `COMPILERS` to the folder holding the compilers (here `/mnt/d/tools/castaway_work/compilers`). Run the SN `.exe` files under Wine in WSL with `unshare -rn` so they have no network. `cc1plus` has no preprocessor, so feed it preprocessed `.ii` input. `Ps2EeAs.exe` works when given only `-G0` (see "Assembler" above). Never commit the compilers.

Rebuild from scratch (export `ghidra_functions.csv` first with `tools/ExportGhidraFunctions.java`):
```
python3 tools/wii_refs.py <ts2c>/asm wii_funcs.json
python3 -I tools/ps2_refs.py SLUS_216.64 ps2_funcs.json ghidra_functions.csv
python3 tools/match.py wii_funcs.json ps2_funcs.json strings
python3 -I tools/vtables.py SLUS_216.64 <ts2c>/asm strings.csv vtable_names.csv --holdout
python3 tools/match.py wii_funcs.json ps2_funcs.json seeded vtable_names.csv
python3 tools/order_fill.py wii_funcs.json ghidra_functions.csv seeded.csv order_names.csv
```
Then merge `vtable_names.csv` and `order_names.csv` into one CSV (columns `ps2_addr,wii_symbol,evidence,tier`) and run `match.py` once more with it as the extra file, using `castaway_ps2_names` as the prefix.

`match.py` output keeps an `example_strings` column of game text for local review. Run `tools/publish_names.py` before copying a names CSV into the kit, because that column shouldn't be published. The full copy is in `D:\tools\castaway_work\v4`.

Dependencies: `pip install capstone pyelftools`. Wii and PS2 strings are normalized by swapping `Wii`/`PS2` for `@PLAT@`.

**Status, 2026-11-05:** steps 1–3 are done, with names applied in Ghidra (`D:\tools\ghidra\projects`). PS2 vtable slots are 8 bytes (`{delta, index}`, pointer), which points to an older GCC ABI and is a lead for step 5. Ghidra's boundaries did not clear the `review` tier as predicted. Constants, switch-table sizes and call counts were not tried.

Step 4 is partly done. `CasSimDescription` (0x334 bytes), `SimBodyPart` (0x24) and its 15 subclasses have structs, defined in `cas_layouts.csv` and applied with `tools/ApplyClassLayouts.java`; 96 methods have `this` typed. `CasUtils` and `CharacterImageMaker` have no structs yet. Field rule for these classes: PS2 offset = Wii offset − 4, because the vptr is the last member on PS2 and the first on Wii. Subclass extra fields and the `field_0x*` entries are still unknown.

Creating functions at all vtable slot targets (`tools/CreateFunctionsAt.java`) added 2,105 functions, bringing the total to 13,231. `tools/FixSwitchTables.java` recovered one switch; 44 functions with computed jumps stay unresolved.

Names now total 2,173 of 13,231 functions (16.4%), including 650 in `game/simulator/core` and `core2`. The jump came from `tools/vtable_order.py`, which pairs tables by order and slot counts (940 names). To rebuild, loop `vtable_order.py`, `vtables.py --all-tiers`, `order_fill.py` and `match.py`, feeding each `castaway_ps2_names.csv` back in, until nothing new appears (5 rounds here). Still open: 2,955 of 4,538 vtable slot targets are unnamed, and Wii seems to combine multiple-inheritance tables that PS2 keeps separate, so those don't match on length.

**Status, 2026-11-07, fifth primitives round (branch `decomp/primitives`):** 904 functions match, no new matches. `bhav.md` gained FindGoodLocation, where a Sim starts on a lot, LotTransition, the transition flags and passive influence, and `TestPlace` and the path search are compared with PS2. `TryLotTransition` is parked 2 words short. Not started: Snap's helper, `TryFamilyInventory`, matching `TryFindGoodLocation`.

**Status, 2026-11-07, fourth primitives round (branch `decomp/primitives`):** 904 functions match, 58,016 bytes, 1.56%. New: the `RoutingSlot` constructor. `FTilePt`, `Slot`, `EVec3` and `EMat4` now have their real shape, and `_vt$11RoutingSlot` is at `0x4B7FC0` with `_vt$4Slot` at `0x4B8020`. `bhav.md`'s Routing section is now from PS2 and lists its own corrections at the top, and it has Rooms. Parked: `TrySnap` at 610 of 726 words. Not started: Snap's helper and `TryFamilyInventory`.

**Status, 2026-11-07, third primitives round (branch `decomp/primitives`):** 903 functions match, 57,920 bytes, 1.56%. New: `AddAction`, `TryUpdate` and seven `RoutingSlot` methods. `bhav.md` now has Routing, Lot transitions, Neighbourhood (`NGBH`, `NBRS`, `FAMI`, checked by `tools/bhav.py`), Update, CompleteGoal, AddAction and Snap's comparison with PS2. Parked: `TryCompleteGoal`, 2 words of 216. Not started: matching Snap, which needs real `FTilePt`, `EVec3` and `EMat4` classes, and `TryFamilyInventory`.

**Status, 2026-11-07, second primitives round (branch `decomp/primitives`):** 894 functions match. New: `TryKillObject`, `TryNotifyStackObject`, both `TrySetMotiveDelta`s, `cXPerson::TryGotoRelative` and `TryIdleForInput`. `TryMakeActionString` is parked at 162 of 164 words. `bhav.md` gained master selectors, load details, and sections for these primitives and Snap, and GenericSimCall is compared with PS2.

**Status, 2026-11-06, primitives (branch `decomp/primitives`):** 888 functions match. New: `cXPerson::TryGotoRoutingSlot`, `TryPushAction`, the helpers `cXPerson::IsIdle`, `IsModelLoaded`, `cXObject::GetInteractionLeader` and `SAnimator2::PreloadOnly`, `TrySetToNext`, which at 4,284 bytes is the largest function matched so far, then the parked `TryTestObjectType`, `TryCheckObject` and `cXObject::TryAnimate`. All three fell to the same fixes: the Wii's branch shape (one `return 0` at the end, `if (x) return 1;` above it), and `findItem` written as the Wii has it. `bhav.md` now covers GotoRoutingSlot, PushAction, Relationship2, CallNamedTree, GenericSimCall and what preload only does.

Parked now, each with a note: `TryCallNamedTree` (410 of 411 words: one branch lands on the alignment nop in the game and just past it here), `TryExpression` (618 of 632, the right size: only alignment nops are in different places), `TryRelationship2` (563 of 732, a first pass), `InterpValue` (2,872 of 3,060) and `DoNodeActionNormal` (65 of 66). `GenericSimCall` has no C++ yet: at 12,744 bytes it's documented only.

**Status, 2026-11-06, run-time state (branch `decomp/operands`):** 878 functions match, with `TryRandom` new. This round was mostly behaviour, not matching. The main find: `tools/placed.py` read an extra i32 in each `ObjM` object, which misaligned everything after the position, so attributes and variables looked missing. It now decodes every object exactly to its stream mark, and that decode answered most of the round: variable `0xB` is the object's ID, attributes are the three lists, each object has one frame on its main tree. `bhav.md` also has entry points (`ObjFnData`), list order, the load sequence, `SetToNext` skipping, `CreateObject`, and a short list of what a RAM snapshot would settle. Most placed objects reset on load, because the disc's trees are newer than the saved houses: 2,027 of 2,610. `tools/bhav.py` now reruns the two attribute count proofs.

About byte counts: `verify.py` counts functions' code. `rebuild.py` prints a higher number, because it adds each function's section, and a function with an odd number of words has 4 bytes of padding to reach 8-byte alignment. At 877 functions that was 43,808 against 44,432, from 156 padded functions. The README uses `verify.py`'s number.

**Status, 2026-11-06, Sim animation and object loading (branch `decomp/operands`):** 877 functions match. New: `cXObject::TryIdle` and `SAnimator2::TryAnimate` (`0x104050`, found through the animator pointer at Sim `+0x5F0`, slot 8), which matched on its first compile after a careful read of the Wii code. `TryRandom` (108 of 118 words) and `TryCheckObject` (131 of 189) are parked with complete logic. `bhav.md` now has Animate's operands and return protocol, Idle, Random, CheckObject, the all-FF TreeBreak answer, and a "Loading an object" section with what was tested on the disc. `SetToNext`'s match still isn't started.

**Status, 2026-11-06, constants and SetToNext (branch `decomp/operands`):** still 875 functions; this round was about data, because that's what the port needed. Tree constants and the semi-global link are both in the `ResFile` quickdata table: `+0x20` is the constant tables and `+0x34` points at the semi-global row. `tools/bhav.py` tests both on the disc with controls, and has `read_resfiles` for anything that needs the data. `SetToNext` is read and documented, but not matched: at 1,070 instructions it needs `CTilePt` (s8 coordinates from 28.4 fixed point, with a range check), `ObjectIterator` and several neighbour lookups written first. `cXObject::TryAnimate` is parked one `nop` short (69 of 70 words). It's only the non-Sim wrapper; the real one is `SAnimator2::TryAnimate`. `CheckObject` wasn't started.

**Status, 2026-11-06, operands (branch `decomp/operands`):** 875 functions match. `cXObject` now derives from `TreeSim`, with its virtuals in Wii order (`verify_vtables.py` checks them against `0x4BC630`) and its fields at offsets taken from the Wii constructor and the PS2 destructor: past `TreeSim`, a PS2 offset is the Wii offset plus 4. `cXObject::InterpWriteValue` and `TryElement` match. `TryElement` matching proves all 73 primitive numbers and the inlined cases.

Functions are parked under `NON_MATCHING`, with the differences listed in a comment above each. All three have complete logic, and what's left is compiler detail, so `docs/formats/bhav.md` uses them as strong evidence:
- `cXObject::InterpValue`: 2,872 of 3,060 words line up. Register allocation, stack frame and block order agree. Five things differ (`data == 7` test in scope 0x22, `lw`/`move` in the neighbour lookup, a `v0`/`v1` swap in scopes 0x34 to 0x37, a retest in scope 0x1A, a null test in scope 0x2A).
- `cXObject::TryExpression`: 618 of 632 words line up, and the size is right.

Not started then: `PlaySound`, `Dialog`.

More of what the compiler does with source shape, from this round:
- Only `BLKmode` objects get 16-byte stack alignment, and a class gets `BLKmode` from a user copy constructor (`ConsoleAutoRefCount`, `FTilePt`). A local that's 16-aligned when it looks like it shouldn't be means this.
- A local that lives in a callee-saved register in a case with no call is shared with another case that has one. `object` in `InterpValue` is one function-level variable for that reason.
- A parameter's register being reused means it's reassigned: `data = mTemps[data]`, not a new local.
- `movz`/`movn` from `if (a == b) result = 0` with the constant 1 needs a 64-bit `result`; in `int` it's always `sltu`. That's how `InterpValue` and every primitive turned out to return `long`.
- GCC reassociates: `x - 1 - data` comes out as `x - (data + 1)`, and the other way round. Write the opposite of what you see.
- `(x & 4) != 0` in a return becomes a shift; through an inline taking the mask it stays `andi`/`sltu`. `(x & 1) != 0` folds to `x & 1`.
- Cross-jumping keeps the last copy, as a jump to a label: the first jump found is the one deleted. Where the game keeps the first copy of a block, written as `bnez x, earlier`, the only way found to get it is a `goto` (`InterpValue`'s person data scopes).
- A jump into a new label made by cross-jumping isn't merged again in the same pass, which is why several `b tail` blocks each keep their own copy of the same few instructions.
- Stack slots of block-scope locals are reused only between objects of the same machine mode and a size that fits. `TryElement`'s `value` shares `tile`'s slot and `effect` doesn't, which is why `FTilePt` is declared as 4 bytes there.
- An inline function's by-value argument is copied before the body runs. That's how to spot one: the copy comes before anything else in the case (`TryParticleEffect`).
- Writing through a reference parameter keeps the caller's register for the result; returning a value from an inline gives it a new one.
- `asm volatile("")` emits nothing and stops two identical blocks merging. It's used twice in `ObjectModule.h`, with a comment, because nothing else kept them apart. If the real reason turns up, take them out.
- A local whose stack slot is shared needs the same mode as the slot's first owner, and a by-value argument to an inline gets its own slot, so a copy at the top of a case points at an inline.
- A word tested with `lh` then reloaded with `lhu` is an `s16` local that one use reads through a `u16` pointer (`TryRandom`). Different extension means GCC won't reuse the first load.
- A fall-through `return 1` at the end, after an `else` that returns early, gives a shared `li v0, 1` block that a `return 1` inside the `if` doesn't.
- If `int` and `s16` accessors give different code, keep the Wii's shape and try the return types. `SetToNextParam` needs `if`/`return` bodies returning `int`, not ternaries returning `s16`.
- A call that returns something the caller ignores still ties up `$v0`. `ObjectIterator::operator++` returns a reference, and declaring it `void` moves the next load from `$v1` to `$v0`.
- A local's stack slot follows the order temporaries are made. `object->GetCTilePt() - centreTile` makes the result first, and a named `tile` local makes it second.
- **The game's real error check looks like `if (!cond) { OBJECT_ERROR_CHECK(false, code); }`.** Written that way, the test is a plain `if`, so a branch-likely reaches the good path, and the code inside the do/while counts extra towards `this` when registers are shared out. It took `TryGotoRoutingSlot` and `TryPushAction` to a full match, and fixed the registers in `TryExpression` and `TryCallNamedTree`. If a function has its registers rotated, check which of its error checks need this. `TREE_CHECK` alone is still right in many places, so try them one at a time.
- References inside a loop count extra when the compiler ranks registers, and a do/while(0) counts as a loop. That's why adding or moving a check changes which saved register a variable gets.
- An error branch that's the last thing before a label wants `OBJECT_ERROR_CHECK(false, code)`, not `TREE_ERROR` and `return`: the do/while gives the aligned label. That was the last word of `cXObject::TryAnimate`.
- A test the compiler drops because it "knows" the answer can come back if the code before an `else` can't run off its end. See the comment in `TrySetToNext`.
- When a stream decodes "well enough", make it end exactly on its marks before trusting any field. A reader that stops early can be wrong from an early field on and still look right.
- Disassembly `nop`s before a label are alignment. They follow from sizes earlier in the function, so leave them until everything before is right.

New scratch tools in `D:\tools\castaway_work\bhav`: `dd.py` aligns our object against the game by sequence and prints only real differences, with `REMAP=s2=s3,...` to look past a register rename; `dr.ps1 "" <limit> <lines> <mangled> <source stem>` builds and runs it; `cases.py` and `te.py` mark jump-table cases in Wii and PS2 listings; `squeeze.py` and `squeeze2.py` fold error and virtual call sequences; `opsym.py` rewrites the operands block of `config/symbols.txt` from `opsyms.txt`, so edit that list, not the file.

**Status, 2026-11-06, the behaviour interpreter:** 873 functions match. The 46 new ones are the code that runs BHAV trees: all of `TreeSim` that isn't in a subclass, `TreeStack`, `StackElem`, and `Behavior`'s lookup of trees and nodes. One function is parked: `TreeSim::DoNodeActionNormal` is one word off (`beql` for `beq`, same delay slot, so behaviour is the same) and sits under `NON_MATCHING` with a note. `docs/formats/bhav.md` holds what this taught about the instruction set, and says what's proven, likely and open.

What is known about BHAV now, in short. Instructions are u16 opcode, u8 true target, u8 false target, 8 operand bytes; 0xFD, 0xFE and 0xFF are error, return true and return false. Below 0x100 is a primitive, and above it the opcode is a tree to call: global up to 0xFFF, private up to 0x1FFF, semi-global up to 0x2328. `tools/bhav.py` tests this on all 5,396 trees with a negative control. All 73 primitive numbers have names and PS2 addresses, from pairing the two jump tables of `TryElement`. Expression (opcode 2, 59% of all instructions) has its 19 operators and all 69 scopes read. Nothing here has been run yet.

**Status, 2026-11-06, matching:** 827 functions matched before the interpreter. 461 of them are generated (see "Generated classes" below). The other 366 were written for Create-a-Sim: 59 in `SimBodyPart`, 60 in `CasSimDescription`, 245 across the 15 `SimBodyPart` subclasses, and both `BitFlags::operator[]` instantiations. Four near misses sit under `NON_MATCHING`, each with a note on what differs: `SimBodyPart::GetModelID` (one word), `SimBodyPart::GetCharacterType` (six words), `SimBodyPartHead::CalculateModelIndexFromModelCodeIndexAndStyleIndex` (two words) and `CasSimDescription::GetValidClothingSetIndexes` (39 of 60 words; its loop is padded correctly now, and what's left starts with a branch-likely and a different register after the loop, so the source shape of `eastl::vector::clear` is still wrong). Both base classes have their full virtual tables declared in order: the Wii vtables in ts2c `asm/data.s` list every slot by name, and the PS2 tables have the same slots in the same order (72 for `SimBodyPart`, 67 for `CasSimDescription`), so any class's virtual layout can be read off the Wii data.

What the compiler does with source shape, learned while matching:
- `if (ok) return a; return b;` puts `a` at the branch target. Swap the two when the branch sense is reversed.
- `x == false` on a `bool` gives `sltiu`; `!x` gives `xori`.
- Indexing through an inline accessor (`VECTOR::operator[]`) picks different registers from indexing a raw pointer.
- A callee declared to return `bool` avoids an extra `sltu` at the call site.
- An early `if (bad) return 0;` puts the return-0 block in the middle of the function. `return ok ? x : 0;` puts it at the end.
- A class is returned through a hidden pointer only if it has its own copy constructor. A destructor alone doesn't do it. `BitFlags` and `EString` both need one declared.
- Small accessors that the Wii build calls are inlined on PS2. Writing the call instead of the field access changes when the value is loaded (`SimBodyPartHair::UpdateRelatedData`).
- Two separate `count--` statements allocate registers differently from one statement behind a combined condition (`SimBodyPart::GetNumValidStyles`).
- Template instantiations go in linkonce sections, which `verify.py` doesn't check. Write them as explicit specialisations (`src/common/BitFlags.cpp`).
- The Wii build keeps small inline functions as real functions, so it shows their bodies. `EString`'s constructors and destructor, `CasSimDescription::AssertWithInfo` and `DegradeSkin`, and the `CalculateName...` helpers were all copied from there, and each unlocked several PS2 functions that inline them.
- An inlined wrapper changes the order arguments are set up in. `return name;` through `EString`'s inline copy constructor matches where `return EString(name.c_str());` is two words off.
- The order of a function's exits follows the source. `if (bad) { ...; return false; } return true;` and `if (ok) return true; ...; return false;` compile differently (the six `IsValid...Index` functions).
- A virtual call whose argument is another call looks its vtable up before making the inner call. Putting the inner result in a local first reverses that (`CasSimDescription::ChangeToCostume`).
- Nested `if`s keep both tests; `a < 7 && a < 4` is folded to one (`CasSimDescription::DoStream`).
- The type of a local decides whether a value is reloaded: a `u8` count against a `u8` field reloads the field for a later use, a `u32` count doesn't (`SimBodyPartTorso::Validate`).
- `switch` on four cases with `case 0:` sharing the default's body tests in a different order from one that only has `default:` (`GetCompositeTextureName`).
- Signedness shows in constants: `addiu -1` for an `int`, `lui`/`ori` for an `unsigned` (`field_0x80`, the stream magic).
- A struct assigned with a user-written `operator=` copies field by field; the compiler's own assignment moves it in one piece (`TRect`).

More of the same, from the interpreter:
- `long` is 64 bits on the EE. A `daddiu` on a value, or `dsll32`/`dsra32` before a jump table, means the source has a `long` (`TreeSim::TryElement` returns one). The game's `size_t` is 32 bits, so anything using `unsigned long` for a size is wrong.
- A `do { } while (0)` around code shows up three ways, and asserts in this codebase are written like that. The label after it can be aligned to eight bytes, with a `nop` before it. A forward branch out of it counts as unlikely, so its delay slot fills from the fall-through instead of giving `bnel`. References inside it count for more in register allocation. `TREE_CHECK` in `TreeSim.h` exists because of these three, and it unlocked `Pop`, `DoNodeAction` and `RunCheckTree`.
- A loop note is a scheduling barrier, so a call inside the `do` schedules differently from one before it: `int found = Get...; TREE_CHECK(found...)` isn't the same as testing the call in the macro.
- Assigning to the same variable forces a new register. `x = a + b; x = (x + 3) & ~3;` gives the game's `a1`/`v1` pattern; one expression gives `v0` all the way (`StackElem::GetSize`). Eight spellings of the expression changed nothing and this did.
- A virtual function's return type shows in the registers around the call: `void` swaps `v0` and `v1` in the vtable load (`Release`, `GetData`).
- Only `inline` functions are inlined at `-O2`, and an inline function's out-of-line copy goes to a linkonce section the tools don't check. Where the game has both, the source has the inline body under a second name (`NextFrameInline`). Order in the file matters too: a body the compiler hasn't seen yet is called, not inlined.
- A class whose only base holds nothing but a vptr keeps the vptr at offset 0; with no base it goes last. `Behavior` and `ObjectDataBehaviorTree` derive from `IConsoleUnknown`.
- `#pragma interface` keeps a vtable an external symbol, so the rebuild links to the game's copy. Needed for any constructor or destructor that stores the vtable.
- `x == c ? 1 : 0` becomes `xori`/`sltu`, however it's written, and a second test stops that: `if (x == 0xfe) return 1; if (x == 0xff) return 0; return 0;` keeps the branch (`GetLastTransition`).
- `a->Count() <= b.Get()` calls first; `b.Get() >= a->Count()` reads first. An inline accessor returning `s16` gives `lhu` then `sll`/`sra`, where a field read gives `lh` (`GetNodeIndex`).
- Two calls merge into one `jalr` only when they're in a nested block ending in returns (`Behavior::GetTree`).
- The return value of a statement can be the real result: `RunCheckTree` looks like it returns 1, but there's no `li` in the game, because it returns what `GetLastTransition` returned. Check for a missing constant before writing one.

A byte match proves the code, not the name. `0x14A050` matched as `SimBodyPart::GetCharacterFlag`, which is what the name list calls it, but its callers pass `this + 0x1E`: it is `BitFlags<unsigned short>::operator[]`, and `GetCharacterFlag` is inlined. The source, the name list and Ghidra are all corrected. Check how a function is called before trusting a propagated-tier name.

How the subclasses were done: the `CasSimDescription` destructor writes each member's vtable pointer, which gives the PS2 vtable address of every `SimBodyPart` subclass, and the Wii vtable of the same class names every slot. That gave a name and address for 249 overrides, most of them one instruction long. The same method should work for any class whose vtable address is known.

Not matched yet in `game/cas`, with the reason (`tools/verify_vtables.py -v` lists every one with its address):
- Have short loops, which no longer block them, and are not tried yet: `CasSimDescription::DoStreamTSC5Pet`; `SimBodyPart::SetToRandom`, `GetClothingSetUIIndex` and `GetValidClothingSetIndexes`.
- `CasSimDescription::GetTattooTextureInfo`: it calls `GetType`, `GetLayoutMapRect` and `GetLayoutMapNumber` on the body parts stored inside the sim, and the PS2 code has the results as constants. So the original defines those small virtual functions inline in the class headers, and the 15 parts are real members. Matching it needs the members declared (with their constructors), the small virtuals moved into the headers, and `verify.py` taught to check the linkonce copies that then replace today's out-of-line ones.
- `CasSimDescription::Randomize`, `Verify`, `GetValidTattooIndexes`, `HandleEventChangeCharacter` and `ChangeEventTypeHasMoreThanOneOption`; `SimBodyPart::CreateIcon`; `GetClothingSetUIIndex` and `GetValidClothingSetIndexes` in `SimBodyPartHair` and `SimBodyPartFacialFeature`: not tried yet. They are the largest ones (576 to 1,692 bytes), and the last four need `eastl::vector::push_back`.
- `CasUtils::GetNumTattooTextures`: inlines `CasUtils::GetBodyData`, a resource lookup through several unmapped globals.
- Subclass destructors: the compiler doesn't emit its implicit destructors as standalone functions in this build.

Names learned from the code this round, worth knowing when reading it: `_globals` (an `EGlobal`) is at `0x514D30`, with the Create-a-Sim quickdata pointer at `+0x1a8` and the game-state texture switch at `+0x448`; `SimBodyPart`'s pointer at `0x14` is the `ClothingItem` being worn; character flag 2 is `kHairBottomDisplayed` (from an assert message); `CasSimDescription` `0x84` and `0x86` are the stream magic and version, and `0xfe` is the skin's hit points; the struct called `CasSimTypeInfo` in the source is `Sim::BodyPartTypeInfoTableTSC6` on Wii.

`config/symbols.txt` is ahead of the name list. About 60 addresses used by the matched code are missing from `pkg/castaway_ps2_names.csv` or sit there at the `low` or `review` tier under a different name (for example `0x138218` is `SimBodyPart::GetModelName`, listed as `CalculateNameBase`). Nothing wrong is applied in Ghidra, because those tiers aren't applied. Syncing them needs a small script that turns the GCC names in `symbols.txt` back into list rows. `src/game/simulator/core/ClothingItem.h` is a generated header that now has hand-added fields; don't regenerate it.

Follow-ups: `Sim::BodyPartModelInfoTSC6` and `Sim::BodyPartStyleInfoTSC6` are the Wii names of the structs called `BodyPartModelCodeInfo` and `BodyPartStyleInfo` in `SimBodyPart.h`. `GetType` returns `Character::eBodyPart` values but is declared `int`.

**Names are checked through vtables.** `tools/verify_vtables.py` compiles a throwaway class derived from each class in `config/vtables.txt` and compares its vtable with the game's. It confirms 1,066 slots in the 16 Create-a-Sim classes, with none wrong: each of those functions' addresses sits in the slot its name claims. Swapping two declarations on purpose is caught, and so is a wrong address for the last slot. 65 slots are unchecked because the function has no address in `config/symbols.txt` yet; `-v` lists each with the game's address, which is the cheapest source of addresses for functions not decompiled yet. Adding a class needs only its vtable address, which its constructor or destructor stores.

**Generated classes.** 461 functions in 96 classes outside Create-a-Sim were generated, not written by hand. Each is one to four instructions: empty, return a constant, read or write one field, increment or clear a field, read through a pointer, or test one bit. The class, method and parameter types come from the Wii symbol, and the body from the PS2 code. Each class header says what that means: only these functions are declared, as plain members; the base class, vtable order and layout are unknown; return types are guesses. Classes sit under the folder matching their Wii source path, and classes from a source file at the root of the Wii tree (`Apt.cpp`) sit directly in `src/`.

For these, a byte match confirms the code and the address, but not the name: any function that returns 0 matches any other. The names carry the confidence of their tier in the name list and no more. That tier was cross-checked this session: a named constructor or destructor stores its class's vtable address, which ties a PS2 vtable to a class exactly, and naming slots that way agrees with the list on 1,477 of 1,500 slots. Of the 23 that differ, 4 were corrected (the function is in exactly one vtable); in the rest the function is shared by several vtables, which supports the list's base-class name. That method also gave 31 new names (tier `vtctor`).

Still unmatched among small named functions: about 50 with other shapes, and about 30 whose Wii symbol the generator doesn't parse (templates, class or enum parameters passed by value, overloads). Not tried: functions of 17 to 32 bytes, which are too varied for cheap patterns.

Next for object behaviour, in order of what the port needs: a RAM snapshot for the three questions at the end of `bhav.md`, a byte match of `Relationship2`, `PlaySound`, `Dialog`, then finishing the parked functions (see "operands" above). `TreeStack::ReconStream` (`0x1C30B0`) and `StackElem::ReconStream` are the save-game side and can wait. Reading these functions is worth doing before matching them: `bhav.md` got most of its content from the Wii code, and the port can use that without any byte match.

Best next seam for count alone: names. 2,955 of 4,538 vtable slot targets are still unnamed, and every newly named one-instruction function can be generated immediately. Tried and rejected: pairing sibling vtables (same base class, same length) in address order. Against tables whose class is already known it was right 12 times in 16, which is too low to name anything from.

**Status, 2026-11-06, rebuild (step 6):** `rebuild/rebuild.py` reproduces the `pkg/target.sha1` checksum exactly from a splat split plus the C++ in `src/`. All 827 matched functions are linked in place of their assembly, including calls between functions in the same file; see `rebuild/README.md` for how. Breaking one C++ function on purpose changes one word and the report names the function, so the check is real. Functions that use a string literal or jump table link too: `rebuild.py` reads where the game's instruction points, checks that the game's bytes there equal ours, and only then fills in the address. Changing a literal on purpose is caught. A function that can't be linked fails the run unless `--allow-kept` is given. Both tools reuse unchanged objects and compile eight files at once, so a full verify takes seconds and a rebuild under a minute. Next for the rebuild: split the game's `.rodata` and `.data` so that data is built from source too, then a decomp.me workflow. Still raw binary: the ELF header, `.vutext`, `.data`, `.rodata` and the rest (12.7% of the file). The original linker is still unidentified, and the rebuild doesn't need it.

## Known weaknesses

- **PS2 function boundaries are rough.** The jal/`jr ra` heuristic finds about 15.7k starts, while the Wii build has about 27.5k functions. It misses some starts, so some "functions" are really two merged together. That's what fills the `review` tier. The fix is to export Ghidra's function list after auto-analysis and rerun the matching against it.
- **Wii string extraction misses some refs.** Only about 2,051 of 27k Wii functions resolve string refs, because the parser only follows `@l` and SDA-relative references to labels with known offsets. Better resolution means more matches.
- **Propagated names are around 70–85% accurate** in spot checks. They're bookmarked so they can be checked.
- **The core simulator has few strings** (needs/motives, routing, `cSimulator`). It'll be named mostly by working outward through the call graph from matched callers.

## Recommended next steps, in order

1. **Get Ghidra running** with `ghidra-emotionengine-reloaded` (chaoticgd), using the build that matches the installed Ghidra version. Import the SLUS, auto-analyze, and run `ApplyCastawayNames.java`. Fix any script compile errors.
2. **Export Ghidra's function list** (start, end, size) with a small script. Change `ps2_refs.py` to read those boundaries instead of its own heuristic, rerun `match.py`, and expect the `review` tier to turn into high/medium.
3. **Add more matching signals:** shared float/int constants, switch-table sizes, call counts, ordering inside a translation unit (functions in a Wii `unity.cpp` tend to sit next to each other in the PS2 build too), and vtable layouts (match class vtables by slot count, then name the slots).
4. **Start in Create-a-Sim**, which has the richest string evidence: `SimBodyPart::*`, `CasSimDescription::*`, `CasUtils::*`, `CharacterImageMaker::*`. Recover their struct layouts in Ghidra.
5. **Identify the compiler** (SN Systems ee-gcc version) before trying any byte-matching decomp. Compare the prologue/epilogue style and library code against known toolchains. decomp.me has PS2 presets. splat is the usual PS2 splitting tool.
6. **Set up the decomp repo** (splat config, build that reproduces the SLUS checksum, decomp.me workflow) only once enough names and types exist to make it worthwhile.
7. **Godot comes later.** It only matters once the simulator code is readable.

## Before making the repo public

- Decide whether to contact the ts2c maintainers; ts2c has no license, and the function names come from it. It's credited in `README.md`.
- Check the history for anything that shouldn't be published.
- The Ghidra project is not in git. A copy is in `G:\My Drive\castaway_ghidra_backup`; refresh it after big changes, and don't share that folder, because it contains the game executable.

## Owner context

- A student (software engineering) doing this as a hobby. Comfortable with code, and new to console reverse engineering, so explain RE-specific steps instead of assuming them.
- Wants to build something that doesn't exist yet. The original-work angle matters to them.
- Prefers practical progress over long theory.
