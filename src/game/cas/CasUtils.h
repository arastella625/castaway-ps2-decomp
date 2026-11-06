#ifndef GAME_CAS_CASUTILS_H
#define GAME_CAS_CASUTILS_H

#include "common/types.h"
#include "game/cas/CasSimDescription.h"

// The function at 0x3D9930. Ghidra's name list calls it EChecksum::ComputeSymbol; the
// parameter types are read from its callers.
class EChecksum {
public:
    static u32 ComputeSymbol(const char *suffix, u32 id);
};

// The word at 0x515178. The name is ours.
extern int gUseGameStateTextures;

// Static helpers. Only the functions the decompiled code uses are declared so far.
class CasUtils {
public:
    static Character::eBodyPart GetS2CEquivalentBodyPart(Character::eBodyPart part);

    // Also inlined on PS2. Turns a stored texture id into the one to use now: when the
    // game-state switch is set, or the caller asks, a non-zero id is rehashed with "_GAME".
    static u32 GetGameStateTextureID(u32 id, bool a)
    {
        if (gUseGameStateTextures) {
            if (id != 0)
                return EChecksum::ComputeSymbol("_GAME", id);
        } else if (id != 0 && a) {
            return EChecksum::ComputeSymbol("_GAME", id);
        }
        return id;
    }

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

    // Also inlined on PS2. 0x27 is the sleeves, which share the torso's database index;
    // 0x7f means the part has none.
    static u8 GetBodyPartDBIndex(Character::eBodyPart part)
    {
        int index = part - 0x1a;
        if (index >= 0 && index < 13)
            return index;
        if (part >= 0 && part <= 0x19) {
            Character::eBodyPart converted = GetS2CEquivalentBodyPart(part);
            index = converted - 0x1a;
            if (index >= 0 && index < 13)
                return index;
            part = converted;
        }
        if (part == 0x27)
            return 4;
        return 0x7f;
    }
};

#endif
