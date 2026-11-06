#ifndef GAME_CAS_CASUTILS_H
#define GAME_CAS_CASUTILS_H

#include "common/types.h"
#include "game/cas/CasSimDescription.h"

// Static helpers. Only the functions the decompiled code uses are declared so far.
class CasUtils {
public:
    static Character::eBodyPart GetS2CEquivalentBodyPart(Character::eBodyPart part);

    // The PS2 build inlines this into its callers; the Wii build keeps it as a function.
    static u8 GetBodyPartIndex(Character::eBodyPart part)
    {
        int index = part - 0x1a;
        if (index >= 0 && index < 15)
            return index;
        if (part >= 0 && part <= 0x19)
            return GetS2CEquivalentBodyPart(part) - 0x1a;
        return 14;
    }
};

#endif
