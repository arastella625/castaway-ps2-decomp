# Project standards

These rules keep the decompilation trustworthy and the repo safe to publish. Four scripts
enforce the parts a machine can check:

- `tools/verify.py` builds `src/` with the game's compiler and fails unless every function
  is byte-identical to the game.
- `rebuild/rebuild.py` links `src/` into a complete executable and fails unless its SHA-1
  is the game's. It catches what `verify.py` can't: a wrong address in `config/symbols.txt`,
  a function whose size or neighbours are wrong, or a string literal that differs from the
  game's.
- `tools/verify_vtables.py` checks the virtual functions each class declares against the
  game's vtable, slot by slot. A byte match proves a function's code, not its name; a
  function sitting in the right slot of the right class's vtable is the evidence for the
  name. Add a class to `config/vtables.txt` once its vtable address is known.
- `tools/check_repo.py` fails when a commit holds game data, binaries or secrets. It runs
  as a pre-commit hook and in CI.

`tools/asm_filter.py` is part of the build, not a check. The game's assembler isn't
available, and the one we have pads very short loops differently, so the filter pads them
the game's way before the assembler runs. It is the only place where the build imitates a
tool instead of running it. Keep it that narrow: a new difference gets its own evidence in
`HANDOFF.md` before any code.

## Repo layout

| Path | Holds |
|---|---|
| `src/` | Decompiled C++. Everything here compiles and matches the game. |
| `src/game/...` | Mirrors the original source paths known from the Wii symbols (`game/cas/unity.cpp` becomes `src/game/cas/`). |
| `src/common/` | Shared types whose original location isn't known yet. |
| `config/symbols.txt` | The PS2 address of each function, by mangled name. The only place addresses live. |
| `config/splat/` | The splat config that splits the executable for the rebuild. |
| `rebuild/` | The full rebuild: `rebuild.py` and its notes. |
| `tools/` | Scripts for the decompilation: build, compare, repo checks. |
| `pkg/` | The naming kit: name lists, struct layouts and the scripts that produce them. |
| `docs/` | Standards and notes. |
| `build/` | Build output. Ignored by git. |

## What never goes in the repo

- The game executable, disc images, or any extracted model, texture, audio or text.
- The compilers. They are proprietary.
- Ghidra projects. They embed the executable.
- `wii_function_reference.csv`, and any CSV that still has the `example_strings` column.
  Run `pkg/tools/publish_names.py` before copying a names CSV into the kit.
- Object files, assembly listings and other build output.
- Keys, tokens and passwords.

Enable the hook once per clone:

```bash
git config core.hooksPath .githooks
```

## Decompiled code (`src/`)

The game's machine code is the specification. Source is correct when it compiles to the
same bytes, so write the clearest code that still matches, and never trade a match for style.

**Always matching.** Every function compiled from `src/` is listed in `config/symbols.txt`
and passes `tools/verify.py`. Work that doesn't match yet stays on a branch, or sits inside
`#ifdef NON_MATCHING` with a comment that says what differs. Check it with
`verify.py -D NON_MATCHING`.

**Names.**
- Classes and functions use the Wii symbol names exactly.
- Fields use the same names as `pkg/cas_layouts.csv`, so Ghidra and the source agree.
  Update both together.
- An unknown field is `field_0x<offset>`.
- A type with no Wii name gets a descriptive name and a comment saying the name is ours.
- A return type or parameter that no code has proven yet is a guess. Say so in a comment
  on the class.

**Layout.**
- One class per `.h` and `.cpp` pair, named after the class.
- Each field has its offset in a trailing comment.
- Each struct of known size has a `STATIC_ASSERT` on `sizeof`.
- Virtual functions are declared in vtable order. The order sets the slot each call uses.
- Fixed-width types come from `common/types.h` (`u8`, `s32`, `f32`).
- Include paths are relative to `src/`.

**Language.** The compiler is GCC 2.95.3, so the code is C++ as it stood before C++98 was
fully implemented: no `nullptr`, `auto`, range-for, `static_assert` or `<cstdint>`.

**Original bugs stay.** If the game reads past an array or skips a null check, the matching
source does too. Mark it with a `// BUG:` comment that explains the fault. Fixes belong in
the port, not here.

**Comments.** Record evidence and intent: why a field has its name, what a flag bit means.
Don't restate the code.

**Format.** Four spaces, no tabs, lines up to 110 columns, braces on their own line for
functions, LF line endings.

## Adding a function

1. Find the function in Ghidra and in the ts2c assembly for the same symbol.
2. Write it in the class's `.cpp`. Add types to the header as the code proves them.
3. Run `tools/verify.py`. It reports the mangled name of anything not yet in
   `config/symbols.txt`; add that name with the function's PS2 address.
4. Read the differing words, adjust the source, and repeat until it matches.
5. Run `rebuild/rebuild.py` before committing. It must still report `MATCH`.
6. Update `pkg/cas_layouts.csv` if you learned a field, and the count in `README.md`.

Run it from WSL:

```bash
CASTAWAY_COMPILERS=<path to the compilers> CASTAWAY_SLUS=<path to SLUS_216.64> python3 tools/verify.py -v
```

## Tools (`tools/`, `pkg/tools/`)

- Python 3.10 or later. Ghidra scripts are Java.
- Each script starts with a docstring that says what it does and shows its usage line.
- Paths come from arguments or environment variables. No machine-specific paths in code.
- A script exits non-zero when it fails, and prints what to do about it.
- Keep functions short and named for what they return. Name constants; don't repeat literals.
- Add a dependency only when the standard library can't do the job. Pin its version.
- Generated files are reproducible: the same inputs give the same output, with sorted order
  and LF line endings.

## Security

**Proprietary and untrusted binaries.**
- The compilers run only under Wine in WSL, inside a network namespace with no interfaces
  (`unshare -rn`). `verify.py` enters that namespace by itself and stops if it still sees a
  network. Never run the `.exe` files on Windows directly or through WSL interop.
- Scripts that run next to the compiler use the standard library only.
- Run Python that reads the game, downloads or extracted archives with `-I`, and keep those
  files in a folder separate from the scripts.

**Code that reads binary data.**
- Treat every offset, size and count read from a file as hostile: check it against the
  buffer before using it.
- Verify the game's SHA-1 against `pkg/target.sha1` before trusting addresses in it.
- No `eval`, `exec`, `pickle` or `shell=True`. Pass command arguments as a list.
- Give every subprocess a timeout.

**The repo.**
- CI jobs get read-only permissions and don't keep credentials.
- Nothing is published without the checks in "What never goes in the repo".

**The port, when it starts.**
- It ships no game data and loads assets from the user's own disc at run time.
- Save files and assets are untrusted input: validate lengths, indices and offsets at the
  loader, and fail with an error instead of reading out of bounds.
- Bugs kept in `src/` for matching are fixed in the port build.
- Build with warnings as errors, and run tests under AddressSanitizer and
  UndefinedBehaviorSanitizer in CI.

## Performance

- **Decompiled code:** don't optimise it. The original compiler's output is the target, and
  any hand-tuning breaks the match.
- **Tools:** correctness first. Measure before optimising, and say in the commit what got
  faster. `verify.py` starts Wine three times per source file; build incrementally once a
  full run passes about 30 seconds.
- **The port:** keep the original data layouts until a profile shows a reason to change
  them, and keep the simulation step separate from rendering so each can be measured.

## Git

- `main` always passes `tools/verify.py`, `tools/verify_vtables.py`, `rebuild/rebuild.py` and
  `tools/check_repo.py`. CI can only run the last one, because it has no game file, so run
  the others locally.
- One topic per commit. The subject line says what changed, in the imperative.
- A commit that adds matched functions says how many and in which class.
- Commit or push only with the owner's go-ahead while the repo's publication status is open.
