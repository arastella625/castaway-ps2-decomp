#ifndef GAME_CAS_SIMBODYPARTUNUSED_H
#define GAME_CAS_SIMBODYPARTUNUSED_H

#include "game/cas/SimBodyPart.h"

// Declares every virtual function this class overrides, in vtable order.
// The ones not defined in the .cpp are not decompiled yet.
class SimBodyPartUnused : public SimBodyPart {
public:
    virtual ~SimBodyPartUnused();
    virtual bool IsValid(bool a, bool b);
    virtual bool IsValidClothingSetIndex(bool a, bool b);
    virtual bool IsValidModelIndex(bool a, bool b);
    virtual bool IsValidModelCodeIndex(bool a, bool b);
    virtual bool IsValidStyleIndex(bool a, bool b);
    virtual bool IsValidTextureIndex(bool a, bool b);
    virtual bool IsValidDegradationState(bool a, bool b);
    virtual void Validate();
    virtual void SetClothingSetIndex(u8 index);
    virtual int GetType() const;
    virtual int GetBodyPartIndex() const;
    virtual int GetBodyPartDBIndex() const;
    virtual int GetTattooLocation() const;
    virtual int GetLayoutMapNumber() const;
    virtual TRect<u16> GetLayoutMapRect() const;
    virtual int GetImageModelSetType() const;
    virtual u8 GetNumPossibleClothingSets() const;
    virtual u8 GetNumPossibleModelCodes() const;
    virtual u8 GetNumPossibleStyles(u8 modelCodeIndex) const;
    virtual u8 GetNumPossibleModels() const;
    virtual u8 GetNumPossibleTextureVariations() const;
    virtual u8 GetNumPossibleDegradationStates() const;
    virtual bool IsGeneric() const;
    virtual const char *GetModelCode() const;
    virtual const char *GetStyleCode() const;
    virtual const char *GetClothingSetName() const;
    virtual const char *GetTextureVariationName() const;
    virtual EString GetModelName() const;
    virtual EString GetTextureName(bool a) const;
    virtual EString GetReflectionMaskName(bool a) const;
    virtual EString GetIconTextureName() const;
    virtual u32 GetModelID() const;
    virtual u32 GetTextureID(bool a) const;
    virtual u32 GetReflectionMaskID(bool a) const;
    virtual u32 GetIconTextureID() const;
    virtual u8 GetDefaultClothingSetIndex(bool a) const;
    virtual void SetToDefault(bool a);
    virtual u8 GetNudeClothingSetIndex(bool a) const;
    virtual void SetToNude(bool a);
    virtual void SetToRandom(int seed);
    virtual u8 CalculateModelCodeIndexFromStyleIndex(u8 styleIndex) const;
    virtual u8 CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const;
    virtual u8 CalculateModelCodeIndexFromModelIndex(u8 modelIndex) const;
    virtual u8 CalculateStyleIndexFromModelIndex(u8 modelIndex) const;
    virtual void SwitchToFirstAvailableDegradationStage();
    virtual void SwitchToFirstAvailableStyle();
    virtual void AdjustToLegalStyle();
    virtual void UpdateRelatedData();
    virtual u8 GetArmorValue(Weather::eDamageType type);
    virtual s32 GetHitPointsPerDegradationState();
    virtual s32 GetIsDamaged();
    virtual s32 GetCurrentHitPoints();
    virtual BitFlags<u32> ApplyDamage(const Weather::DamageEvents &events);
    virtual BitFlags<u32> Degrade();
    virtual BitFlags<u32> Repair();
    virtual u8 GetClothingSetUIIndex() const;
};
STATIC_ASSERT(SimBodyPartUnused_size, sizeof(SimBodyPartUnused) == 0x24);

#endif
