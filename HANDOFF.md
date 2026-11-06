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

**Status, 2026-11-06, matching:** 827 functions match. 461 of them are generated (see "Generated classes" below). The other 366 were written for Create-a-Sim: 59 in `SimBodyPart`, 60 in `CasSimDescription`, 245 across the 15 `SimBodyPart` subclasses, and both `BitFlags::operator[]` instantiations. Four near misses sit under `NON_MATCHING`, each with a note on what differs: `SimBodyPart::GetModelID` (one word), `SimBodyPart::GetCharacterType` (six words), `SimBodyPartHead::CalculateModelIndexFromModelCodeIndexAndStyleIndex` (two words) and `CasSimDescription::GetValidClothingSetIndexes` (its loop is padded correctly now; what's left is a branch-likely and one register, so the source shape of `eastl::vector::clear` is still wrong). Both base classes have their full virtual tables declared in order: the Wii vtables in ts2c `asm/data.s` list every slot by name, and the PS2 tables have the same slots in the same order (72 for `SimBodyPart`, 67 for `CasSimDescription`), so any class's virtual layout can be read off the Wii data.

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

Best next seam: names. 2,955 of 4,538 vtable slot targets are still unnamed, and every newly named one-instruction function can be generated immediately. Tried and rejected: pairing sibling vtables (same base class, same length) in address order. Against tables whose class is already known it was right 12 times in 16, which is too low to name anything from.

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
