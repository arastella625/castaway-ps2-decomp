#!/bin/bash
# Run inside a network namespace: COMPILERS=<folder> unshare -rn bash asm.sh
export WINEPREFIX=$HOME/.wine-ps2 WINEDEBUG=-all WINEARCH=win32
cd "${COMPILERS:?Set COMPILERS to the folder holding the compilers}" || exit 1
d=ee-gcc2.95.3-136/lib/gcc-lib/ee/2.95.3
for a in as.exe Ps2EeAs.exe; do
  timeout 60 wine "$d/$a" -EL -mips3 -mcpu=r5900 -o out/body_136_$a.o out/body_ee-gcc2.95.3-136_1.s 2>&1 | head -n 4
  echo "$a exit=${PIPESTATUS[0]} size=$(stat -c %s out/body_136_$a.o 2>/dev/null)"
done
file out/body_136_*.o | cut -c1-110
