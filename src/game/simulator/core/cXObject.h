#ifndef GAME_SIMULATOR_CORE_CXOBJECT_H
#define GAME_SIMULATOR_CORE_CXOBJECT_H

#include "common/types.h"

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class cXObject {
public:
    void HandleError();
    u8 pad_0x0[0x110];
    u32 field_0x110; // 0x110
    u8 pad_0x114[0x20];
    s32 field_0x134; // 0x134

    void PreSave();
    void TagGraphicsField();
    u32 GetSpriteSlot();
    void ForceLocation();
    u32 ReconType();
};

#endif
