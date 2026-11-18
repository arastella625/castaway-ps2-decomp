#ifndef GAME_SIMULATOR_CORE_PASSIVEINFLUENCESYSTEM_H
#define GAME_SIMULATOR_CORE_PASSIVEINFLUENCESYSTEM_H

#include "common/types.h"

class cXObject;
class AnimRef;

// Wii: PassiveInfluenceSystem, the simulator's map of what objects advertise to Sims nearby.
class PassiveInfluenceSystem {
public:
    void SetSkillNameID(cXObject *object, const AnimRef *anim);
    void ClearInfluenceMotiveDeltas(cXObject *object);
    void SetInfluenceMotiveDelta(cXObject *object, u8 motive, f32 delta, s16 limit, s8 type);
    void RemoveObjectFromMap(cXObject *object); // 0x1A0F08
};

#endif
