#ifndef GAME_MISC_TEXTBASEITEM_H
#define GAME_MISC_TEXTBASEITEM_H

#include "common/types.h"

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class TextBaseItem {
public:
    u8 pad_0x0[0x4];
    s32 field_0x4; // 0x04
    u8 pad_0x8[0x1c];
    f32 field_0x24; // 0x24
    f32 field_0x28; // 0x28
    u8 pad_0x2c[0x18];
    u32 field_0x44; // 0x44
    f32 field_0x48; // 0x48
    f32 field_0x4c; // 0x4c

    void Reset();
    u32 GetColor();
    void SetShadowAlpha(f32 value);
    f32 GetShadowAlpha();
    void SetShadowSize(f32 value);
    f32 GetShadowSize();
    void SetPosX(f32 value);
    f32 GetPosX();
    void SetPosY(f32 value);
    f32 GetPosY();
};

#endif
