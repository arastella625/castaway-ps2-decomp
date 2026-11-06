#ifndef GAME_CAS_SIMBODYPARTSHOES_H
#define GAME_CAS_SIMBODYPARTSHOES_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartShoes : public SimBodyPart {
public:
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
};
STATIC_ASSERT(SimBodyPartShoes_size, sizeof(SimBodyPartShoes) == 0x24);

#endif
