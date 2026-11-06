#ifndef GAME_CAS_SIMBODYPARTFACIALFEATURE_H
#define GAME_CAS_SIMBODYPARTFACIALFEATURE_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartFacialFeature : public SimBodyPart {
public:
    s32 currentHitPoints; // 0x24
    s32 isDamaged;        // 0x28

    virtual ~SimBodyPartFacialFeature();
    virtual void DoStream(ReconBuffer *buffer, int version);
    virtual void CopyFrom(const SimBodyPart *other);
    virtual int GetType() const;
    virtual int GetBodyPartIndex() const;
    virtual int GetBodyPartDBIndex() const;
    virtual int GetTattooLocation() const;
    virtual int GetLayoutMapNumber() const;
    virtual TRect<u16> GetLayoutMapRect() const;
    virtual int GetImageModelSetType() const;
    virtual void GetTextureName(bool a) const;
    virtual u32 GetReflectionMaskID(bool a) const;
    virtual u8 CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const;
    virtual u8 CalculateStyleIndexFromModelIndex(u8 modelIndex) const;
    virtual void SwitchToFirstAvailableDegradationStage();
    virtual u8 GetArmorValue(Weather::eDamageType type);
    virtual s32 GetIsDamaged();
    virtual void SetIsDamaged(bool damaged);
    virtual s32 GetCurrentHitPoints();
    virtual void SetCurrentHitPoints(int hitPoints);
    virtual void GetValidClothingSetIndexes(
         eastl::vector<Sim::ClothingSetIndexInfo, eastl::allocator> &indexes);
    virtual u8 GetClothingSetUIIndex() const;
};
STATIC_ASSERT(SimBodyPartFacialFeature_size, sizeof(SimBodyPartFacialFeature) == 0x2c);

#endif
