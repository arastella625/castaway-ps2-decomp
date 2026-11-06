#ifndef GAME_SIMULATOR_CORE2_PROPTABLEIMPL_H
#define GAME_SIMULATOR_CORE2_PROPTABLEIMPL_H

#include "common/types.h"

// Whatever PropTableImpl::field_0x4 points to. The name is ours.
struct PropTableImpl_0x4 {
    u8 pad_0x0[0x4];
    s16 field_0x4; // 0x04
};

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class PropTableImpl {
public:
    u8 pad_0x0[0x4];
    PropTableImpl_0x4 *field_0x4; // 0x04

    s16 GetID();
    int GetEntryName(int a);
};

#endif
