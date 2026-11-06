#ifndef APTSCRIPTFUNCTION2_H
#define APTSCRIPTFUNCTION2_H

#include "common/types.h"

// Whatever AptScriptFunction2::field_0x34 points to. The name is ours.
struct AptScriptFunction2_0x34 {
    u32 field_0x0; // 0x00
    u32 field_0x4; // 0x04
    u8 pad_0x8[0x8];
    u32 field_0x10; // 0x10
};

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class AptScriptFunction2 {
public:
    u8 pad_0x0[0x34];
    AptScriptFunction2_0x34 *field_0x34; // 0x34

    u32 GetName() const;
    u32 GetNumArguments();
    u32 GetByteCodeSize();
};

#endif
