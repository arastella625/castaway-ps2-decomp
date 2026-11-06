#include "game/cas/SimBodyPartUnused.h"

bool SimBodyPartUnused::IsValid(bool a, bool b)
{
    return true;
}

bool SimBodyPartUnused::IsValidClothingSetIndex(bool a, bool b)
{
    return true;
}

bool SimBodyPartUnused::IsValidModelIndex(bool a, bool b)
{
    return true;
}

bool SimBodyPartUnused::IsValidModelCodeIndex(bool a, bool b)
{
    return true;
}

bool SimBodyPartUnused::IsValidStyleIndex(bool a, bool b)
{
    return true;
}

bool SimBodyPartUnused::IsValidTextureIndex(bool a, bool b)
{
    return true;
}

bool SimBodyPartUnused::IsValidDegradationState(bool a, bool b)
{
    return true;
}

void SimBodyPartUnused::Validate()
{
}

void SimBodyPartUnused::SetClothingSetIndex(u8 index)
{
}

int SimBodyPartUnused::GetType() const
{
    return 0x28;
}

int SimBodyPartUnused::GetBodyPartIndex() const
{
    return 0xe;
}

int SimBodyPartUnused::GetBodyPartDBIndex() const
{
    return 0x7f;
}

int SimBodyPartUnused::GetTattooLocation() const
{
    return 4;
}

int SimBodyPartUnused::GetLayoutMapNumber() const
{
    return 0;
}

TRect<u16> SimBodyPartUnused::GetLayoutMapRect() const
{
    return TRect<u16>(0x40, 0, 0x80, 0x40);
}

int SimBodyPartUnused::GetImageModelSetType() const
{
    return 8;
}

u8 SimBodyPartUnused::GetNumPossibleClothingSets() const
{
    return 0;
}

u8 SimBodyPartUnused::GetNumPossibleModelCodes() const
{
    return 0;
}

u8 SimBodyPartUnused::GetNumPossibleStyles(u8 modelCodeIndex) const
{
    return 0;
}

u8 SimBodyPartUnused::GetNumPossibleModels() const
{
    return 0;
}

u8 SimBodyPartUnused::GetNumPossibleTextureVariations() const
{
    return 0;
}

u8 SimBodyPartUnused::GetNumPossibleDegradationStates() const
{
    return 0;
}

bool SimBodyPartUnused::IsGeneric() const
{
    return false;
}

const char *SimBodyPartUnused::GetModelCode() const
{
    return "";
}

const char *SimBodyPartUnused::GetStyleCode() const
{
    return "";
}

const char *SimBodyPartUnused::GetClothingSetName() const
{
    return "";
}

const char *SimBodyPartUnused::GetTextureVariationName() const
{
    return "";
}

EString SimBodyPartUnused::GetModelName() const
{
    return EString("");
}

EString SimBodyPartUnused::GetTextureName(bool a) const
{
    return EString("");
}

EString SimBodyPartUnused::GetReflectionMaskName(bool a) const
{
    return EString("");
}

EString SimBodyPartUnused::GetIconTextureName() const
{
    return EString("");
}

u32 SimBodyPartUnused::GetModelID() const
{
    return 0;
}

u32 SimBodyPartUnused::GetTextureID(bool a) const
{
    return 0;
}

u32 SimBodyPartUnused::GetReflectionMaskID(bool a) const
{
    return 0;
}

u32 SimBodyPartUnused::GetIconTextureID() const
{
    return 0;
}

u8 SimBodyPartUnused::GetDefaultClothingSetIndex(bool a) const
{
    return 0;
}

void SimBodyPartUnused::SetToDefault(bool a)
{
}

u8 SimBodyPartUnused::GetNudeClothingSetIndex(bool a) const
{
    return 0;
}

void SimBodyPartUnused::SetToNude(bool a)
{
}

void SimBodyPartUnused::SetToRandom(int seed)
{
}

u8 SimBodyPartUnused::CalculateModelCodeIndexFromStyleIndex(u8 styleIndex) const
{
    return 0;
}

u8 SimBodyPartUnused::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return 0;
}

u8 SimBodyPartUnused::CalculateModelCodeIndexFromModelIndex(u8 modelIndex) const
{
    return 0;
}

u8 SimBodyPartUnused::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return 0;
}

void SimBodyPartUnused::SwitchToFirstAvailableDegradationStage()
{
}

void SimBodyPartUnused::SwitchToFirstAvailableStyle()
{
}

void SimBodyPartUnused::AdjustToLegalStyle()
{
}

void SimBodyPartUnused::UpdateRelatedData()
{
}

u8 SimBodyPartUnused::GetArmorValue(Weather::eDamageType type)
{
    return 0xff;
}

s32 SimBodyPartUnused::GetHitPointsPerDegradationState()
{
    return 0x3e8;
}

s32 SimBodyPartUnused::GetIsDamaged()
{
    return 0;
}

s32 SimBodyPartUnused::GetCurrentHitPoints()
{
    return 0;
}

BitFlags<u32> SimBodyPartUnused::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags<u32>(0);
}

BitFlags<u32> SimBodyPartUnused::Degrade()
{
    return BitFlags<u32>(0);
}

BitFlags<u32> SimBodyPartUnused::Repair()
{
    return BitFlags<u32>(0);
}

u8 SimBodyPartUnused::GetClothingSetUIIndex() const
{
    return 0;
}
