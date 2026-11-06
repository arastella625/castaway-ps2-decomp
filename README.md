# The Sims 2: Castaway (PS2) decompilation

A hobby reverse-engineering project for the US PS2 build, `SLUS_216.64`. The goal is C++ source that rebuilds the original binary, and later a native PC version.

**This repo contains no game data.** You need your own copy of the game. `pkg/target.sha1` holds the checksum of the expected executable.

## Status

- 2,205 of 13,231 functions are named, each with a confidence tier.
- Structs are recovered for `CasSimDescription`, `SimBodyPart` and its subclasses.
- 827 functions are matched byte for byte (34,240 bytes, 0.92% of the code). 366 are in Create-a-Sim (`CasSimDescription`, `SimBodyPart` and its 15 subclasses). The other 461 are functions of one to four instructions in 96 classes across the game, generated from patterns; for those a match confirms the code but not the name. Nearly everything else is still undecompiled.
- The compiler for the Create-a-Sim code is SN Systems ee-gcc 2.95.3 build 1.36 at `-O2 -G0 -fno-rtti -fno-exceptions`, with SN's own assembler. The exact assembler build isn't available, so one of its behaviours, the padding of very short loops, is reproduced by `tools/asm_filter.py` (see HANDOFF.md).
- A full rebuild links all of those functions in place of their assembly and reproduces the game's checksum. String literals and jump tables are checked against the game's copies. The data sections are still raw binary.

## Where to start

- [HANDOFF.md](HANDOFF.md): project goals, rules, findings and next steps.
- [pkg/README.md](pkg/README.md): setting up Ghidra and applying the names.
- [docs/STANDARDS.md](docs/STANDARDS.md): how code is written, checked and kept safe to publish.

## Checking the decompiled code

`tools/verify.py` builds `src/` with the game's compiler and compares each function with your copy of the game. It needs WSL with Wine, the compiler from decompme/compilers, and the game executable; [docs/STANDARDS.md](docs/STANDARDS.md) has the command.

`tools/verify_vtables.py` checks names: each class's declared virtual functions must sit in the same vtable slots as in the game. It confirms 1,066 slots across the 16 Create-a-Sim classes.

`rebuild/rebuild.py` goes further: it links the C++ into a complete executable and checks its SHA-1. [rebuild/README.md](rebuild/README.md) has the setup.

## Credits

Function names come from the symbol table in [simdecomp/ts2c](https://github.com/simdecomp/ts2c), a decompilation of the Wii build. This project wouldn't have got far without that work.

PS2 analysis uses [Ghidra](https://github.com/NationalSecurityAgency/ghidra) and [ghidra-emotionengine-reloaded](https://github.com/chaoticgd/ghidra-emotionengine-reloaded). Compiler builds for testing come from [decompme/compilers](https://github.com/decompme/compilers).

## License

The original tools and docs are under the [MIT License](LICENSE). That doesn't cover the game or anything derived from it, or third-party material such as the ts2c symbol names.

The Sims is a trademark of Electronic Arts. This project isn't affiliated with or endorsed by EA.
