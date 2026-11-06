#ifndef APTSCRIPTFUNCTIONBYTECODEBLOCK_H
#define APTSCRIPTFUNCTIONBYTECODEBLOCK_H

#include "common/types.h"

class AptCIH;
class AptValue;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class AptScriptFunctionByteCodeBlock {
public:
    u8 pad_0x0[0x34];
    u32 field_0x34; // 0x34
    u32 field_0x38; // 0x38
    u32 field_0x3c; // 0x3c

    u32 GetName() const;
    int GetNumArguments();
    u32 GetByteCodeBase();
    u32 GetByteCodeSize();
    void SetArgument(AptValue *a, int b);
    int Duplicate(AptCIH *a);
};

#endif
