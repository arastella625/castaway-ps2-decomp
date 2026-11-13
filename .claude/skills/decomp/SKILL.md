---
name: decomp
description: Workflow for the Sims 2 Castaway PS2 decompilation in this repo. Use it for any work on the matched C++ in src/, config/symbols.txt, the vtable lists, the BHAV or format docs, or the matching tools. That includes "match this function", "why does this function differ", "park it", "write up what X does", "add this class", "run verify", "check the rebuild", reading PS2 or Wii assembly, and the pre-commit checks. Load it even when the request only names a function, an address or a class, because the match rules, the safety rules for the compiler, and the places to look are all here.
---

# Decompiling The Sims 2: Castaway (PS2)

The game's machine code is the specification. A function is done when `src/` compiles to
the same bytes the game has at its address, and the name is believed only when a vtable
slot or a caller proves it. Everything in this skill follows from that.

Read these before writing code, in this order. They are short and the rules in them are
enforced by scripts, so guessing costs more than reading:

1. `docs/STANDARDS.md`: layout, names, what never goes in the repo, the four checks.
2. `HANDOFF.md`, the two lists headed "What the compiler does with source shape". They are
   the hard-won rules for turning a diff into a source change. Grep for that heading.
3. `CLAUDE.local.md` in the repo root, if present: this machine's paths and wrapper
   scripts. It is untracked on purpose. Without it, ask for `CASTAWAY_COMPILERS` and
   `CASTAWAY_SLUS` before trying to build.

## Where things are

| Need | Look in |
|---|---|
| PS2 address of a function | `config/symbols.txt`, the only place addresses live |
| Wii code of the same function | the local ts2c checkout, `asm/code/<source path>.s`; the path is in the name list's `wii_file` column |
| Name, tier and evidence for an address | `pkg/castaway_ps2_names.csv` |
| Field names and offsets | `pkg/cas_layouts.csv`, kept in step with the headers |
| Which classes have checked vtables | `config/vtables.txt` |
| What's parked and why | `grep -rn NON_MATCHING src` and the comment above each block |
| How the BHAV interpreter works | `docs/formats/bhav.md`, which says what is proven, likely and open |
| Asset and data formats | `docs/ASSET_FORMATS.md`, `docs/formats/` |
| Project history, counts and open questions | `HANDOFF.md`, the dated status entries |

## The matching loop

**Pick a target that teaches something.** `tools/verify_vtables.py -v` lists every
unchecked slot with the game's address, which is the cheapest source of both names and
addresses. Prefer a function whose Wii body is visible in ts2c, whose class already has a
header, and whose size is under a few hundred bytes. The large ones go faster after the
small ones in the same class have fixed the types.

**Read both sides first.** Put the PS2 disassembly and the Wii assembly side by side before
writing anything. The Wii build keeps small inline functions as real functions, so it shows
bodies that PS2 inlined, and its branch shape is the one to copy: one `return 0` at the
end, `if (x) return 1;` above it. Writing from the Wii's shape and then adjusting is what
matched most of the interpreter.

**Write it, then verify one file.** Add the function to the class's `.cpp`, declare what
it proves in the header with offsets as trailing comments, add the mangled name and
address to `config/symbols.txt` (the verifier prints the mangled name when it is missing),
and run the verifier on just that source file. A full run takes minutes; one file takes
seconds.

**Read the diff as the compiler would.** The verifier says how many words differ; the
scratch diff tools show which. Before changing anything, decide what kind of difference it
is, because each kind has its own fix:

- Different instructions in the same place: wrong operation, type or signedness. Check
  `int` against `s16`, `long` against `int`, `bool` returns, and inline accessors against
  raw field reads.
- Same instructions, different registers: the compiler ranked variables differently.
  References inside loops and `do { } while (0)` blocks count extra, so an error check
  written as `TREE_CHECK` instead of `OBJECT_ERROR_CHECK(false, code)`, or the other way,
  rotates every register after it.
- Blocks in a different order: the order of returns and early exits in the source sets
  the order of the blocks. Swap the two sides of an `if`, or move a `return`.
- Only `nop`s moved: alignment padding before labels. Leave these until everything
  before them is right, because they follow from earlier sizes.
- A short loop one word long: `tools/asm_filter.py` handles loop padding; if it stopped
  the build, read its message rather than working around it.

The lists in `HANDOFF.md` have thirty more of these with the function that proved each.
Check them before inventing a theory, and add a line there when a diff teaches a new one,
with the function's name as the evidence.

**Try shapes in a scratch file, not in `src/`.** Compile `src/zz_t.cpp` through the
verifier to see what a construct turns into, and delete that file before committing. Eight
spellings of one expression can give the same code; what moves registers is usually an
assignment to the same variable, a reference parameter, or the type of a local.

**Park honestly.** After a reasonable number of attempts with complete logic, put the
function under `#ifdef NON_MATCHING` with a comment that says how many words differ, where,
and which shapes were tried. Measure the count at that moment with the diff tool, not from
memory, because on a long function the verifier counts every word after the first size
change. A parked function with correct logic is still evidence for the docs; say so there.

**Before saying it matches, run the four checks.** `tools/verify.py` for the bytes,
`tools/verify_vtables.py` for the names, `rebuild/rebuild.py` for the checksum, and
`tools/check_repo.py` for what must not be committed. CI runs only the last one, because
it has no game file, so the first three are the local author's job. A commit that adds
matched functions says how many and in which class, and the count in `README.md` moves
with it.

## Rules that are easy to break

- **A byte match proves the code, not the name.** Any function that returns 0 matches any
  other. Before trusting a propagated-tier name, look at how callers use the function.
  `0x14A050` matched as `GetCharacterFlag` and was `BitFlags::operator[]`.
- **The original's bugs stay.** Mark them `// BUG:` and fix them in the port, never here.
- **Don't optimise matched code or restate it in comments.** Comments hold evidence: why a
  field has its name, what a flag bit means, which Wii function a body was copied from.
- **The language is GCC 2.95 C++.** No `nullptr`, `auto`, `static_assert`, `<cstdint>` or
  range-for. Fixed-width types come from `common/types.h`.
- **Generated files are hand-maintained now.** The fifteen `SimBodyPart<Subclass>` pairs
  and `ClothingItem.h` came from generators that would overwrite hand edits. Never rerun
  the generators on them.
- **`asm_filter.py` stays narrow.** It imitates one behaviour of an assembler nobody has.
  A new difference, such as the `nop`s before `div.s`, gets evidence in `HANDOFF.md` first
  and its own discussion with the owner before any code.
- **Addresses live only in `config/symbols.txt`.** Some blocks of that file are rewritten
  from lists in the scratch folder; `CLAUDE.local.md` says which, so edit the list.

## Safety

The compilers are old proprietary Windows binaries. They run only under Wine inside WSL
in a network namespace with no interfaces; `verify.py` enters that namespace by itself
and refuses to run if it still sees a network. Never run the `.exe` files on Windows
directly or through WSL interop, never commit them, and keep third-party Python away from
them (the verifier uses the standard library only for that reason).

The game executable, disc images, Ghidra projects, extracted assets, RAM snapshots and
anything with the `example_strings` column are game data and never enter the repo. The
`check_repo.py` hook catches the mechanical cases; the judgement cases, like a save file's
Sim name or a machine path in a doc, are yours. The repo is public.

Run Python that reads the game, downloads or extracted archives with `-I`, from a folder
that holds no scripts of its own.

## Documenting what the code does

Most of the value of a read function reaches the port through the docs, with or without a
byte match, so write up behaviour as it is learned. In `docs/formats/`, each claim is
proven (tested on the disc or matched), likely (read from the Wii code) or open; keep the
three apart, and name the function or test that proves it. When a format doc gets a
section from PS2 that corrects an earlier Wii reading, list the corrections at the top of
that section.

Every round of work ends with a dated status entry in `HANDOFF.md`: the branch, the
function count before and after, what matched, what's parked and why, and what wasn't
started. Those entries are how the next session finds its footing, so write them for
someone who has only the repo.

## Working with this owner

A software engineering student doing this as a hobby, comfortable with code and newer to
console reverse engineering. Finish the work rather than handing back pieces, and explain
the reverse-engineering ideas as they come up: what a vtable slot proves, why a delay slot
moves a `nop`, what branch-likely means. Practical progress over theory, but the why of
each rule matters more than the rule.

Before quoting a count or a percentage, measure it on the current branch. Numbers in old
status entries go stale as branches merge.

Branches are `decomp/<topic>`, `assets/<topic>`, `docs/<topic>` or `tools/<topic>`, merged
into `main` by pull request. Commit when the work is checked, and push or open a pull
request when the owner says to.
