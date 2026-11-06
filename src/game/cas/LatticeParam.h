#ifndef GAME_CAS_LATTICEPARAM_H
#define GAME_CAS_LATTICEPARAM_H

#include "common/types.h"
#include "game/cas/CasSimDescription.h"

namespace Sim {
enum eBodyType { BODY_TYPE_COUNT = 3 };
}

// Wii: LatticeParam, a singleton that holds the body-shape values for each species.
// Only what the decompiled code calls is declared so far.
class LatticeParam {
public:
    static LatticeParam *Instance();
    f32 GetMorphValNormal(Character::eSpecies species, Sim::eLatticeMorphRegion region) const;
    f32 GetMorphValForBodyType(Character::eSpecies species, Sim::eLatticeMorphRegion region,
                               Sim::eBodyType bodyType) const;
};

// Wii: MorphTargetParam, the same for the face-shape values.
class MorphTargetParam {
public:
    static MorphTargetParam *Instance();
    f32 GetMorphValNormal(Character::eSpecies species, Sim::eMorphTargetRegion region) const;
};

#endif
