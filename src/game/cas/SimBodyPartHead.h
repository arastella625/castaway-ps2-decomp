#ifndef GAME_CAS_SIMBODYPARTHEAD_H
#define GAME_CAS_SIMBODYPARTHEAD_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartHead : public SimBodyPart {
public:
    virtual void Validate();
    virtual int GetType() const;
    virtual int GetBodyPartIndex() const;
    virtual int GetBodyPartDBIndex() const;
    virtual int GetTattooLocation() const;
    virtual bool IsLayered() const;
    virtual bool IsRequiredBodyPart() const;
    virtual int GetLayoutMapNumber() const;
    virtual TRect<u16> GetLayoutMapRect() const;
    virtual int GetImageModelSetType() const;
    virtual u8 CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const;
    virtual u8 CalculateModelCodeIndexFromModelIndex(u8 modelIndex) const;
    virtual u8 CalculateStyleIndexFromModelIndex(u8 modelIndex) const;
    virtual s32 GetIsDamaged();
    virtual s32 GetCurrentHitPoints();
    virtual BitFlags<u32> ApplyDamage(const Weather::DamageEvent &event, s16 a);
};
STATIC_ASSERT(SimBodyPartHead_size, sizeof(SimBodyPartHead) == 0x24);

#endif
