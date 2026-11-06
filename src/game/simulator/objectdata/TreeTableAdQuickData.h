#ifndef GAME_SIMULATOR_OBJECTDATA_TREETABLEADQUICKDATA_H
#define GAME_SIMULATOR_OBJECTDATA_TREETABLEADQUICKDATA_H

#include "common/types.h"

// Whatever TreeTableAdQuickData::field_0x4 points to. The name is ours.
struct TreeTableAdQuickData_0x4 {
    s16 field_0x0; // 0x00
    s16 field_0x2; // 0x02
    s16 field_0x4; // 0x04
};

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class TreeTableAdQuickData {
public:
    u8 pad_0x0[0x4];
    TreeTableAdQuickData_0x4 *field_0x4; // 0x04
    s32 field_0x8; // 0x08

    s16 GetPersonalityAd() const;
    s16 GetMin() const;
    s16 GetRange() const;
    void SetPersonalityAd(int a);
    void SetMin(int a);
    void SetMax(int a);
    void SetRange(int a);
    void AddRef();
};

#endif
