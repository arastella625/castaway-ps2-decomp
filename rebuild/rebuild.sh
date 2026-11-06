#!/bin/bash
# Rebuilds SLUS_216.64 from a splat split and compares it with the game.
# Usage: bash rebuild.sh <split folder>
# The split folder holds splat's output, which is the game's code, so keep it outside the repo.
# Needs binutils-mips-linux-gnu. Run in WSL.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
split=$(cd "${1:?Usage: rebuild.sh <split folder>}" && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
cd "$split"

# -mabi=eabi matches the game's e_flags; -mabi=32 expands every ld and sd into two instructions
mips-linux-gnu-as -EL -march=r5900 -mabi=eabi -G0 -I include -o "$out/text.o" asm/text.s
mips-linux-gnu-ld -EL --no-check-sections -T "$here/text.ld" -T undefined_syms_auto.txt \
    -T undefined_funcs_auto.txt -o "$out/text.elf" "$out/text.o"
mips-linux-gnu-objcopy -O binary -j .text "$out/text.elf" "$out/text.bin"

# .text is 0x38C604 bytes; the assembler pads it, and the padding is in after_text.bin
head -c $((0x38C604)) "$out/text.bin" > "$out/text.trim"
cat assets/header.bin "$out/text.trim" assets/after_text.bin > "$out/SLUS_216.64.rebuilt"
python3 "$here/compare.py" "$out/SLUS_216.64.rebuilt" SLUS_216.64 "$here/../pkg/target.sha1"
