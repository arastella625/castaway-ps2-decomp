# The Sims 2: Castaway (PS2) decompilation

A hobby reverse-engineering project for the US PS2 build, `SLUS_216.64`. The goal is C++ source that rebuilds the original binary, and later a native PC version.

**This repo contains no game data.** You need your own copy of the game. `pkg/target.sha1` holds the checksum of the expected executable.

## Status

- 2,205 of 13,231 functions are named, each with a confidence tier.
- Structs are recovered for `CasSimDescription`, `SimBodyPart` and its subclasses.
- 896 functions are matched byte for byte (57,372 bytes, 1.54% of the code). 366 are in Create-a-Sim (`CasSimDescription`, `SimBodyPart` and its 15 subclasses). 46 are the behaviour tree interpreter (`TreeSim`, `TreeStack`, `Behavior`). The other 461 are functions of one to four instructions in 96 classes across the game, generated from patterns; for those a match confirms the code but not the name. Nearly everything else is still undecompiled.
- BHAV instructions are decoded: [docs/formats/bhav.md](docs/formats/bhav.md) has the format, the primitive table, the expression operators, all 69 operand scopes, tree constants and the semi-global link, and `tools/bhav.py` tests the format on every tree on the disc.
- The compiler for the Create-a-Sim code is SN Systems ee-gcc 2.95.3 build 1.36 at `-O2 -G0 -fno-rtti -fno-exceptions`, with SN's own assembler. The exact assembler build isn't available, so one of its behaviours, the padding of very short loops, is reproduced by `tools/asm_filter.py` (see HANDOFF.md).
- A full rebuild links all of those functions in place of their assembly and reproduces the game's checksum. String literals and jump tables are checked against the game's copies. The data sections are still raw binary.

## Where to start

- [HANDOFF.md](HANDOFF.md): project goals, rules, findings and next steps.
- [pkg/README.md](pkg/README.md): setting up Ghidra and applying the names.
- [docs/STANDARDS.md](docs/STANDARDS.md): how code is written, checked and kept safe to publish.

## Checking the decompiled code

`tools/verify.py` builds `src/` with the game's compiler and compares each function with your copy of the game. It needs WSL with Wine, the compiler from decompme/compilers, and the game executable; [docs/STANDARDS.md](docs/STANDARDS.md) has the command.

`tools/verify_vtables.py` checks names: each class's declared virtual functions must sit in the same vtable slots as in the game. It confirms 1,085 slots across 17 classes.

`rebuild/rebuild.py` goes further: it links the C++ into a complete executable and checks its SHA-1. [rebuild/README.md](rebuild/README.md) has the setup.

## Reading the game's assets

`tools/arc.py` lists and extracts the disc's `.ARC` archives, `tools/texture.py` turns all 13,249 textures into PNGs, `tools/model.py` turns all 2,404 models into OBJ files with their materials and textures, `tools/skeleton.py` reads the 338 character skeletons, `tools/anim.py` decodes all 3,104 animations, `tools/gltf.py` exports models as binary glTF with their rigs and animations, ready for Godot or Blender, and `tools/sim.py` assembles a whole Sim with its skin, clothing, face, hair and body shape. `tools/audio.py`, `movie.py`, `font.py` and `gamedata.py` cover sound, video, fonts and the game's data tables, and `tools/livecheck.py` compares all this with the game running in PCSX2. They read your own disc image in place and use only the standard library. [docs/ASSET_FORMATS.md](docs/ASSET_FORMATS.md) describes the formats and what's still unknown. What they extract is game data, so keep it out of the repo.

## Credits

Function names come from the symbol table in [simdecomp/ts2c](https://github.com/simdecomp/ts2c), a decompilation of the Wii build. This project wouldn't have got far without that work.

PS2 analysis uses [Ghidra](https://github.com/NationalSecurityAgency/ghidra) and [ghidra-emotionengine-reloaded](https://github.com/chaoticgd/ghidra-emotionengine-reloaded). Compiler builds for testing come from [decompme/compilers](https://github.com/decompme/compilers).

## License

The original tools and docs are under the [MIT License](LICENSE). That doesn't cover the game or anything derived from it, or third-party material such as the ts2c symbol names.

The Sims is a trademark of Electronic Arts. This project isn't affiliated with or endorsed by EA.
