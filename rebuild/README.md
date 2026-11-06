# Full rebuild

Splits `SLUS_216.64` with splat, reassembles its code, and checks the result against `pkg/target.sha1`. This proves the assembly round-trips exactly, so functions can be replaced with compiled C++ one at a time while the checksum guards the build.

**Status:** the rebuilt file is byte-identical to the game. `.text` is reassembled from source (87.3% of the file). The ELF header, `.vutext`, `.data`, `.rodata` and everything else are still raw binary, and no compiled C++ is linked in yet.

## Setup

- `pip install "splat64[mips]"`
- In WSL: `sudo apt install binutils-mips-linux-gnu`

## Run

Use a work folder outside the repo, because splat's output is the game's code in text form and must never be committed.

1. Copy `SLUS_216.64` and `config/splat/castaway.yaml` into the work folder.
2. In that folder: `python -m splat split castaway.yaml`
3. In WSL: `bash rebuild/rebuild.sh <work folder>`

A mismatch prints the first differing addresses. Changing one instruction reports exactly that word.

## Notes

- Assemble with `-mabi=eabi`. `-mabi=32` expands every `ld` and `sd` into two instructions, which shifts everything after them.
- This uses GNU binutils, not the original linker, which is still unidentified. `rebuild.sh` puts the file together itself, so it doesn't need one.
