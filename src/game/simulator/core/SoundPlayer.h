#ifndef GAME_SIMULATOR_CORE_SOUNDPLAYER_H
#define GAME_SIMULATOR_CORE_SOUNDPLAYER_H

#include "common/types.h"
#include "game/simulator/core/Neighborhood.h"

class ERSoundEvent;

// Wii: cBoxX, the sound mixer. Only the call Cleanup reaches is here.
class cBoxX {
public:
    void Event(int type, int a, int sourceID, ERSoundEvent *event, f32 b, bool c); // 0x1EA458
};

extern cBoxX *g_pBoxX; // 0x5108F0

// Wii: cSoundPlayer. The two words QuietBySourceID tests are bytes on Wii.
class cSoundPlayer {
public:
    int field_0x0; // 0x00
    int field_0x4; // 0x04
    int field_0x8; // 0x08

    // Wii: QuietBySourceID, inlined on PS2: event 0x17 for the source.
    void QuietBySourceID(int sourceID)
    {
        if (field_0x0 == 0 || field_0x8 == 0)
            return;
        g_pBoxX->Event(0x17, 0, sourceID, 0, 0.0f, false);
    }
};

inline cSoundPlayer *GetSoundPlayer()
{
    return Globs::pSound;
}

#endif
