#ifndef GAME_CAS_SIMBODYPARTHAT_H
#define GAME_CAS_SIMBODYPARTHAT_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartHat : public SimBodyPart {
public:
    virtual ~SimBodyPartHat();
    virtual void Validate();
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
    virtual void UpdateRelatedData();
    virtual s32 GetIsDamaged();
    virtual BitFlags<u32> ApplyDamage(const Weather::DamageEvents &events);
};
STATIC_ASSERT(SimBodyPartHat_size, sizeof(SimBodyPartHat) == 0x24);

#endif
