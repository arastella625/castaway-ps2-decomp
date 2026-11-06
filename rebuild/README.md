# Full rebuild

`rebuild.py` builds the whole executable from a splat split plus the decompiled C++ in `src/`, and checks the result against `pkg/target.sha1`. A passing run means the C++ is linked into a file that is byte-identical to the game.

**Status:** all 776 matched C++ functions are linked in place of their assembly, and the rebuilt file matches. The ELF header, `.vutext`, `.data`, `.rodata` and everything else after `.text` (12.7% of the file) are still raw binary.

## Setup

- `pip install "splat64[mips]"`
- In WSL: `sudo apt install binutils-mips-linux-gnu`, plus Wine and the compiler, as for `tools/verify.py`.

## Run

Use a work folder outside the repo, because splat's output is the game's code in text form and must never be committed.

1. Copy `SLUS_216.64` and `config/splat/castaway.yaml` into the work folder.
2. In that folder: `python -m splat split castaway.yaml`. This only needs redoing if the config changes.
3. In WSL, from the repo:

```bash
CASTAWAY_SPLIT=<work folder> CASTAWAY_COMPILERS=<compilers folder> CASTAWAY_SLUS=<SLUS_216.64> python3 rebuild/rebuild.py
```

It takes about a minute. `--asm-only` links no C++, which tells a problem in the split apart from a problem in `src/`.

## How it works

1. Each source file is compiled with `-ffunction-sections`, so every function sits in its own section. The code is the same as `verify.py` builds.
2. Each function is checked for what it refers to. Calls to addresses in `config/symbols.txt` can be linked. A string literal or jump table has no address of its own, because the game's `.rodata` is still raw binary. The game's instruction says where its copy is; if the bytes there equal ours, the reference is pointed at it. The address is taken on trust and the content is checked. A function that fails this, or refers to a symbol with no address, is renamed out of the way, the game's assembly is kept, and the run fails unless `--allow-kept` is given.
3. The address range of every linked function is cut out of the split assembly, along with the padding after it.
4. A generated linker script places each assembly piece and each C++ function at its original address. A piece that is too big makes the linker stop at that address.
5. The file is put back together and its SHA-1 compared.

## Reading a failure

- **`NO MATCH`** lists the differing words and the C++ function each one is in. Run `tools/verify.py` for a per-function report.
- **`cannot move location counter backwards`** from `ld` means the piece before that address is bigger than the game's. Check the address in `config/symbols.txt` and the function before it.
- **`kept: <function> refers to X, which has no address`** means `X` needs a line in `config/symbols.txt`.
- **`kept: <function> uses data ... the game's copy doesn't confirm`** means a string literal or jump table in that function differs from the game's.

## Notes

- Assemble with `-mabi=eabi`. `-mabi=32` expands every `ld` and `sd` into two instructions.
- The game's assembler writes symbol tables that `ld` rejects, so each object goes through `objcopy` first.
- The assembler pads the standard `.text` section to 16 bytes, so every assembly piece gets its own section name.
- This uses GNU binutils, not the original linker, which is still unidentified.
