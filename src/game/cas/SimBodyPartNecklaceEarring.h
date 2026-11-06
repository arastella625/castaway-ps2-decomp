#ifndef GAME_CAS_SIMBODYPARTNECKLACEEARRING_H
#define GAME_CAS_SIMBODYPARTNECKLACEEARRING_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartNecklaceEarring : public SimBodyPart {
public:
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
STATIC_ASSERT(SimBodyPartNecklaceEarring_size, sizeof(SimBodyPartNecklaceEarring) == 0x24);

#endif
