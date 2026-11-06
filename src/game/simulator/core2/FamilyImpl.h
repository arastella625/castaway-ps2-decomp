#ifndef GAME_SIMULATOR_CORE2_FAMILYIMPL_H
#define GAME_SIMULATOR_CORE2_FAMILYIMPL_H

#include "common/types.h"

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class FamilyImpl {
public:
    u8 pad_0x0[0x8];
    s32 field_0x8; // 0x08
    s32 field_0xc; // 0x0c
    s32 field_0x10; // 0x10
    u32 field_0x14; // 0x14
    u32 field_0x18; // 0x18
    s32 field_0x1c; // 0x1c

    s32 GetNumber();
    void SetNumber(int value);
    s32 GetHouseNumber();
    void SetHouseNumber(int value);
    s32 GetCreationOrder();
    void SetCreationOrder(int value);
    u32 GetFunds();
    u32 GetPetPoints();
    s32 GetHouseValue();
    void SetHouseValue(int value);
    int GetStatValue(int a) const;
    int GetStatRef(int a, const s16 * *b);
    bool ShouldSerializeMotives();
    int MyDoCommand(s16 a, int b);
};

#endif
