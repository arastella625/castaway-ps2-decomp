#!/bin/bash
# Run inside a network namespace: COMPILERS=<folder> unshare -rn bash run.sh <source basename>
# COMPILERS holds the unpacked compilers, flags.txt and t/<source>.ii.
export WINEPREFIX=$HOME/.wine-ps2 WINEDEBUG=-all WINEARCH=win32
cd "${COMPILERS:?Set COMPILERS to the folder holding the compilers}" || exit 1
src=$1
mkdir -p out
rm -f out/${src}_*
i=0
while IFS= read -r flags; do
  [ -z "$flags" ] && continue
  for b in ee-gcc2.95.2-273a ee-gcc2.95.3-114 ee-gcc2.95.3-136 ee-gcc2.96; do
    o=out/${src}_${b}_$i.s
    if [ $b = ee-gcc2.96 ]; then
      # 32-bit stat fails on the Windows drive's large inodes, so build on the Linux side
      cp t/$src.ii /tmp/$src.ii
      mkdir -p /tmp/c296 && cp ee-gcc2.96/lib/gcc-lib/ee/2.96-ee-001003-1/cc1plus /tmp/c296/cc1plus && chmod +x /tmp/c296/cc1plus
      (cd /tmp && timeout 60 c296/cc1plus -lang-c++ -quiet $flags $src.ii -o $src.s) 2> $o.err
      rc=$?; cp /tmp/$src.s $o 2>/dev/null; (exit $rc)
    else
      timeout 60 wine "$(find $b -name cc1plus.exe)" -quiet $flags t/$src.ii -o $o 2> $o.err
    fi
    echo "$b|$i|$flags|$?|$o"
  done
  i=$((i+1))
done < flags.txt
