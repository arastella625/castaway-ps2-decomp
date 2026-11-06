#ifndef GAME_CAS_SIMBODYPARTHAIR_H
#define GAME_CAS_SIMBODYPARTHAIR_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartHair : public SimBodyPart {
public:
    s32 currentHitPoints; // 0x24
    s32 isDamaged;        // 0x28

    virtual void DoStream(ReconBuffer *buffer, int version);
    virtual void CopyFrom(const SimBodyPart *other);
    virtual void Validate();
    virtual int GetType() const;
    virtual int GetBodyPartIndex() const;
    virtual int GetBodyPartDBIndex() const;
    virtual int GetTattooLocation() const;
    virtual bool IsRequiredBodyPart() const;
    virtual int GetLayoutMapNumber() const;
    virtual TRect<u16> GetLayoutMapRect() const;
    virtual int GetImageModelSetType() const;
    virtual const char *GetModelCode() const;
    virtual EString GetTextureName(bool a) const;
    virtual EString GetReflectionMaskName(bool a) const;
    virtual EString GetIconTextureName() const;
    virtual u32 GetModelID() const;
    virtual u32 GetReflectionMaskID(bool a) const;
    virtual u8 CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const;
    virtual u8 CalculateModelCodeIndexFromModelIndex(u8 modelIndex) const;
    virtual u8 CalculateStyleIndexFromModelIndex(u8 modelIndex) const;
    virtual void SwitchToFirstAvailableDegradationStage();
    virtual void UpdateRelatedData();
    virtual u8 GetArmorValue(Weather::eDamageType type);
    virtual s32 GetIsDamaged();
    virtual void SetIsDamaged(bool damaged);
    virtual s32 GetCurrentHitPoints();
    virtual void SetCurrentHitPoints(int hitPoints);
    virtual void GetValidClothingSetIndexes(
         eastl::vector<Sim::ClothingSetIndexInfo, eastl::allocator> &indexes);
    virtual u8 GetClothingSetUIIndex() const;
    virtual u8 GetDegradationModelCodeIndex() const;
};
STATIC_ASSERT(SimBodyPartHair_size, sizeof(SimBodyPartHair) == 0x2c);

#endif
