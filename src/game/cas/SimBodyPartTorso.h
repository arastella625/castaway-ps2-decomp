#ifndef GAME_CAS_SIMBODYPARTTORSO_H
#define GAME_CAS_SIMBODYPARTTORSO_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartTorso : public SimBodyPart {
public:
    virtual ~SimBodyPartTorso();
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
    virtual void UpdateRelatedData();
    virtual BitFlags<u32> Degrade();
    virtual BitFlags<u32> Repair();
};
STATIC_ASSERT(SimBodyPartTorso_size, sizeof(SimBodyPartTorso) == 0x24);

#endif
