#ifndef GAME_UI_CORE_CURTARGET_H
#define GAME_UI_CORE_CURTARGET_H

#include "common/types.h"

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class CURTarget {
public:
    u8 pad_0x0[0x8c];
    // The cursor, in the Flash stage's 640 x 448 units: the constructor sets the bounds at
    // +0x9C to +0xA8 to x 0 to 0x280 and y 0 to 0x1C0, and ClampCursor keeps it inside them.
    int mX;         // 0x8c
    int mY;         // 0x90
    u8 pad_0x94[0x1c];
    u32 field_0xb0; // 0xb0

    void GetPosition(int &x, int &y);
    u32 GetCursorHighlight();
};

#endif
