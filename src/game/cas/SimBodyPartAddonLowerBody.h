#ifndef GAME_CAS_SIMBODYPARTADDONLOWERBODY_H
#define GAME_CAS_SIMBODYPARTADDONLOWERBODY_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartAddonLowerBody : public SimBodyPart {
public:
    virtual ~SimBodyPartAddonLowerBody();
    virtual int GetType() const;
    virtual int GetBodyPartIndex() const;
    virtual int GetBodyPartDBIndex() const;
    virtual int GetTattooLocation() const;
    virtual int GetLayoutMapNumber() const;
    virtual TRect<u16> GetLayoutMapRect() const;
    virtual int GetImageModelSetType() const;
    virtual s32 GetIsDamaged();
    virtual BitFlags<u32> ApplyDamage(const Weather::DamageEvents &events);
};
STATIC_ASSERT(SimBodyPartAddonLowerBody_size, sizeof(SimBodyPartAddonLowerBody) == 0x24);

#endif
