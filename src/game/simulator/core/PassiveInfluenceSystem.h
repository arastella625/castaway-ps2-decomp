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
    // Wii: AddObjectToMap(cXObject *, sc, sc, sc, sc, s, s).
    void AddObjectToMap(cXObject *object, s8 a, s8 b, s8 count, s8 flags, s16 trigger,
                        s16 lockout); // 0x1A09E0
};

#endif
