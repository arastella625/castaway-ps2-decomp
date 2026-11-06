#ifndef GAME_SIMULATOR_CORE2_TREESIM_H
#define GAME_SIMULATOR_CORE2_TREESIM_H

#include "common/types.h"

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class TreeSim {
public:
    u32 field_0x0; // 0x00
    u32 field_0x4; // 0x04
    u32 field_0x8; // 0x08
    u32 field_0xc; // 0x0c
    u8 pad_0x10[0x8];
    u32 field_0x18; // 0x18
    u8 pad_0x1c[0x24];
    u32 field_0x40; // 0x40

    u32 GetPortal() const;
    u32 GetPerson() const;
    u32 GetMTObject() const;
    u32 GetObject() const;
    void StackJustPopped();
    u32 GetIterations();
    u32 GetLastResult();
};

#endif
