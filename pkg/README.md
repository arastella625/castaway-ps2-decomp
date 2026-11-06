# Sims 2: Castaway (PS2) — reverse-engineering starter kit

Target: `SLUS_216.64` (US PS2, build string "PS2 The Sims 2 Console Build B.08.09.0").
The Wii build in ts2c is the **same build number** (B.08.09.0), which is why its symbols carry over so well.

## What's in here

| File | What it is |
|---|---|
| `castaway_ps2_names_ghidra.txt` | 2,173 PS2 function names to import into Ghidra (218 high, 76 medium, 494 vtable, 940 vtorder, 260 propagated, 185 order — see below) |
| `castaway_ps2_names.csv` | Every candidate match with its evidence, including the `review` and `low` tiers that are **not** auto-applied |
| `wii_function_reference.csv` | All 27,483 named Wii functions: readable name, mangled name, source file, size, strings used. Your main lookup table — open in a spreadsheet and filter. Local only: `.gitignore` excludes it because it's built from unlicensed ts2c data and contains game strings. |
| `ApplyCastawayNames.java` | Ghidra script that applies the names |
| `cas_layouts.csv` | Recovered struct layouts for `CasSimDescription` and `SimBodyPart`, with the evidence for each field. Apply with `tools/ApplyClassLayouts.java` |
| `tools/` | The scripts that produced all of this (re-run them as matching improves) |

## Confidence tiers

- **high** — shares a string that appears in exactly one function on each platform, wins by a wide margin, similar size. Trust these.
- **medium** — mutual best match on shared strings, but the evidence is weaker (common strings like `loaded`, `%d,%d`). Usually right.
- **vtable** — named by slot position after aligning a PS2 vtable with the Wii one. Hold-out test: 38/38 correct, on a small sample.
- **vtorder** — named by slot position in a table paired by table order and slot counts, with no named slot needed. Hold-out test: 45/45; sizes and call counts match the vtable tier and clearly beat a shuffled baseline. Same-length sibling classes could still be swapped, and those checks can't see that.
- **vtctor** — named by slot position in a vtable that a named constructor or destructor points to, so the table's class is known exactly. Used as a cross-check on the other vtable tiers: of 1,500 slots both name, 1,477 agree. Of the 23 that differ, 4 were corrected here (the function is in exactly one vtable); in the rest the function is shared by several tables, which supports the base-class name already in the list.
- **matched** — set by hand after the function was decompiled and its callers read.
- **propagated** — no strings; inferred because a matched function calls it in the same position on both platforms. Hold-out test: 19/19 correct, on a small sample of functions that also have strings. Bookmarked so you can verify.
- **order** — inferred from position between two named neighbours in the same source file. Hold-out test: 210/211; misses were close neighbours in the same class, so check these first if something looks off.
- **review** (CSV only) — good string match but very different size, nearly always a much larger PS2 function. Ghidra's boundaries didn't resolve these, so the cause is still open (inlining or merged functions).
- **low** (CSV only) — ambiguous. Leads, not answers.

## Setup (about 20 minutes)

1. Install **Ghidra** (NationalSecurityAgency GitHub releases) and the 64-bit JDK it requires: `application.java.min` in `Ghidra/application.properties` says which (**JDK 25** for Ghidra 12.2).
2. Download **ghidra-emotionengine-reloaded** (github.com/chaoticgd/ghidra-emotionengine-reloaded/releases). Pick the zip built for *your exact Ghidra version* — the filename says which.
3. In Ghidra: **File → Install Extensions → +**, choose that zip, restart Ghidra.
4. New project → **File → Import File** → `SLUS_216.64`. It should be detected as MIPS-R5900 / Emotion Engine. Accept the defaults.
5. Open it, say **Yes** to auto-analysis (defaults are fine). Takes a few minutes.
6. **Window → Script Manager → Manage Script Directories** (bundle icon) → add this folder. Find `ApplyCastawayNames` under category *Castaway*, run it, pick `castaway_ps2_names_ghidra.txt`.
7. **Window → Bookmarks**, filter by category "Castaway names" to walk through what was applied.

## Suggested first area: Create-a-Sim

CAS has the most string evidence, so the most anchors: `SimBodyPart::*`, `CasSimDescription::*`, `CasUtils::*`, `CharacterImageMaker::*` all matched. Open one of the `SimBodyPart::IsValid*` functions next to the same function in ts2c's `asm/code/game/cas/unity.cpp.s` and compare. Name the unnamed functions they call, then re-export and iterate.

The core simulator (`game/simulator/*`) has almost no strings, so it'll be named mostly by working outward from matched callers — expect that to come later.
