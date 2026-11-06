#ifndef APTSCRIPTFUNCTION1_H
#define APTSCRIPTFUNCTION1_H

#include "common/types.h"

// Whatever AptScriptFunction1::field_0x34 points to. The name is ours.
struct AptScriptFunction1_0x34 {
    u32 field_0x0; // 0x00
    u32 field_0x4; // 0x04
    u8 pad_0x8[0x4];
    u32 field_0xc; // 0x0c
};

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class AptScriptFunction1 {
public:
    u8 pad_0x0[0x34];
    AptScriptFunction1_0x34 *field_0x34; // 0x34

    u32 GetName() const;
    u32 GetNumArguments();
    u32 GetByteCodeSize();
};

#endif
