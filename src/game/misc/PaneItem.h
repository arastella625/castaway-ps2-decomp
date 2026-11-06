#ifndef GAME_MISC_PANEITEM_H
#define GAME_MISC_PANEITEM_H

#include "common/types.h"

class ERC;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class PaneItem {
public:
    u8 pad_0x0[0xc];
    f32 field_0xc; // 0x0c
    f32 field_0x10; // 0x10

    void Shutdown();
    void CalculateSize();
    void SetPosX(f32 value);
    f32 GetPosX() const;
    void SetPosY(f32 value);
    f32 GetPosY() const;
    void Startup();
    void Draw(ERC *a, f32 b);
};

#endif
