/* Places the split code at its original address. Symbol values come from splat's
   undefined_*_auto.txt files, passed as extra -T scripts. */
SECTIONS
{
    .text 0x00100000 : { *(.text) }
    /DISCARD/ : { *(.MIPS.abiflags) *(.reginfo) *(.pdr) *(.gnu.attributes) *(.mdebug.*) *(.gcc_compiled_long*) }
}
