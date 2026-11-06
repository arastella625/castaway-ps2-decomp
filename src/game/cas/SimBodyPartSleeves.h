#ifndef GAME_CAS_SIMBODYPARTSLEEVES_H
#define GAME_CAS_SIMBODYPARTSLEEVES_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartSleeves : public SimBodyPart {
public:
    u8 field_0x24; // 0x24

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
    virtual u8 CalculateStyleIndexFromModelIndex(u8 modelIndex) const;
    virtual void SwitchToFirstAvailableDegradationStage();
    virtual s32 GetIsDamaged();
    virtual BitFlags<u32> ApplyDamage(const Weather::DamageEvents &events);
    virtual BitFlags<u32> Degrade();
    virtual BitFlags<u32> Repair();

    // Not virtual.
    void SwitchToMatchTorsoSettings();
};
STATIC_ASSERT(SimBodyPartSleeves_size, sizeof(SimBodyPartSleeves) == 0x28);

#endif
