#ifndef GAME_CAS_SIMBODYPARTARMACCESSORY_H
#define GAME_CAS_SIMBODYPARTARMACCESSORY_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartArmAccessory : public SimBodyPart {
public:
    virtual int GetType() const;
    virtual int GetBodyPartIndex() const;
    virtual int GetBodyPartDBIndex() const;
    virtual int GetTattooLocation() const;
    virtual bool UseStyleCodeInTextureName() const;
    virtual int GetLayoutMapNumber() const;
    virtual TRect<u16> GetLayoutMapRect() const;
    virtual int GetImageModelSetType() const;
    virtual u8 CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const;
    virtual u8 CalculateStyleIndexFromModelIndex(u8 modelIndex) const;
    virtual s32 GetIsDamaged();
    virtual BitFlags<u32> ApplyDamage(const Weather::DamageEvents &events);
};
STATIC_ASSERT(SimBodyPartArmAccessory_size, sizeof(SimBodyPartArmAccessory) == 0x24);

#endif
