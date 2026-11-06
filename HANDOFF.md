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

**Compiler (step 5, done for `game/cas`):** SN Systems ee-gcc 2.95.3 build 1.36 at `-O2` (`ee-gcc2.95.3-136` in decompme/compilers). Three `SimBodyPart` methods compiled and assembled with its own `as.exe` are byte-identical to the game: 115 of 115 comparable words, with 3 relocated words excluded. SN 2.95.3 v1.14 and 2.95.2 v2.73a do not match. Sony 2.96 was not fairly tested, because my flags didn't target the R5900.

Flags, from 315 matched functions: `-O2 -G0 -fno-rtti -fno-exceptions`. `-G0` is proven: float constants assemble to `lui`/`ori`/`mtc1` only when `as` gets `-G0`, and string addresses are `lui`/`addiu`, not gp-relative. `-fno-rtti` is likely, because slot 0 of every vtable is null. `-fno-exceptions` is proven: with exceptions on, the compiler adds cleanup code around a call made while a temporary is alive, and the game has none (`SimBodyPart::GetIconTextureID`). Still open: `-O2` and `-O3` gave the same code. Only one source module has been tested; other modules may use other flags, and EA and Sony libraries may have been built separately.

**Matching workflow:** decompiled code is in `src/`, addresses in `config/symbols.txt`, and `tools/verify.py` builds and byte-compares every function. `docs/STANDARDS.md` has the rules and the command. `pkg/tools/matching` is the older harness that compared four compilers and twelve flag sets; keep it for testing other modules.

To run the older harness: it is in `pkg/tools/matching`. Set `COMPILERS` to the folder holding the compilers (here `/mnt/d/tools/castaway_work/compilers`). Run the SN `.exe` files under Wine in WSL with `unshare -rn` so they have no network. `cc1plus` has no preprocessor, so feed it preprocessed `.ii` input. `Ps2EeAs.exe` crashes under Wine, and `as.exe` works. Never commit the compilers.

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

**Status, 2026-11-05, matching:** 315 functions match: 44 in `SimBodyPart`, 45 in `CasSimDescription`, 224 across the 15 `SimBodyPart` subclasses, and both `BitFlags::operator[]` instantiations. Two near misses sit under `NON_MATCHING`, each with a note on what differs: `SimBodyPart::GetModelID` (one word) and `SimBodyPartHead::CalculateModelIndexFromModelCodeIndexAndStyleIndex` (two words). Both base classes have their full virtual tables declared in order: the Wii vtables in ts2c `asm/data.s` list every slot by name, and the PS2 tables have the same slots in the same order (72 for `SimBodyPart`, 67 for `CasSimDescription`), so any class's virtual layout can be read off the Wii data.

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

A byte match proves the code, not the name. `0x14A050` matched as `SimBodyPart::GetCharacterFlag`, which is what the name list calls it, but its callers pass `this + 0x1E`: it is `BitFlags<unsigned short>::operator[]`, and `GetCharacterFlag` is inlined. The source is corrected; the name list and Ghidra still have the wrong name. Check how a function is called before trusting a propagated-tier name.

How the subclasses were done: the `CasSimDescription` destructor writes each member's vtable pointer, which gives the PS2 vtable address of every `SimBodyPart` subclass, and the Wii vtable of the same class names every slot. That gave a name and address for 249 overrides, most of them one instruction long. The same method should work for any class whose vtable address is known.

Not matched yet in `game/cas`, with the reason:
- `SimBodyPart::IsValidClothingSetIndex` and its five siblings: each inlines `AssertWithInfo`, which formats a message into 0x600 bytes of stack buffers and copies the sim's name.
- `CasSimDescription::UseDyesForTextureVariation`: reads `PlanManager::cheatIgnoreRequirements` at `0x4923B0` and inlines `CasUtils::UseArchetype`.
- `CasUtils::GetNumTattooTextures`: inlines `CasUtils::GetBodyData`, a resource lookup through several unmapped globals.
- Subclass destructors: the compiler doesn't emit its implicit destructors as standalone functions in this build.
- The name functions (`GetModelName`, `GetTextureName` and so on): they build `EString` values, so the string class needs more of its members first.

Follow-ups: `Sim::BodyPartModelInfoTSC6` and `Sim::BodyPartStyleInfoTSC6` are the Wii names of the structs called `BodyPartModelCodeInfo` and `BodyPartStyleInfo` in `SimBodyPart.h`. `GetType` returns `Character::eBodyPart` values but is declared `int`.

Best next seam: the name list has about 1,400 vtable-tier names, and many of those functions are one or two instructions. Their Wii symbols encode the class, method and parameter types, so trivial bodies could be generated for many classes at once, the way the subclasses were. That needs a decision on how to lay out hundreds of small classes in `src/`.

**Status, 2026-11-05, rebuild (step 6):** `rebuild/rebuild.py` reproduces the `pkg/target.sha1` checksum exactly from a splat split plus the C++ in `src/`. 303 of the 315 matched functions are linked in place of their assembly; see `rebuild/README.md` for how. Breaking one C++ function on purpose changes one word and the report names the function, so the check is real. Twelve functions stay as assembly because they use string literals or a jump table; `rebuild.py` lists them on every run. Linking those needs the game's `.rodata` split, which is the next rebuild milestone, followed by `.data` (vtables) and a decomp.me workflow. Still raw binary: the ELF header, `.vutext`, `.data`, `.rodata` and the rest (12.7% of the file). The original linker is still unidentified, and the rebuild doesn't need it.

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
