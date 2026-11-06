#include "game/cas/CasSimDescription.h"

Character::eSpecies CasSimDescription::GetSpecies() const
{
    return (Character::eSpecies)(flags.Get(0) == false);
}

int CasSimDescription::GetSimType() const
{
    return 0;
}

u8 CasSimDescription::GetClothingSetIndex(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)]->clothingSetIndex;
}

void CasSimDescription::SetClothingSetIndex(Character::eBodyPart part, u8 index)
{
    bodyParts[GetBodyPartIndex(part)]->SetClothingSetIndex(index);
}

u8 CasSimDescription::GetTextureIndex(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)]->textureIndex;
}

void CasSimDescription::SetTextureIndex(Character::eBodyPart part, u8 index)
{
    bodyParts[GetBodyPartIndex(part)]->textureIndex = index;
}

u8 &CasSimDescription::TextureIndex(Character::eBodyPart part)
{
    return bodyParts[GetBodyPartIndex(part)]->textureIndex;
}

u8 CasSimDescription::GetStyleIndex(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)]->styleIndex;
}

void CasSimDescription::SetStyleIndex(Character::eBodyPart part, u8 index)
{
    bodyParts[GetBodyPartIndex(part)]->styleIndex = index;
}

u8 &CasSimDescription::StyleIndex(Character::eBodyPart part)
{
    return bodyParts[GetBodyPartIndex(part)]->styleIndex;
}

u8 CasSimDescription::GetModelCodeIndex(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)]->modelCodeIndex;
}

void CasSimDescription::SetModelCodeIndex(Character::eBodyPart part, u8 index)
{
    bodyParts[GetBodyPartIndex(part)]->modelCodeIndex = index;
}

u8 &CasSimDescription::ModelCodeIndex(Character::eBodyPart part)
{
    return bodyParts[GetBodyPartIndex(part)]->modelCodeIndex;
}

u8 CasSimDescription::GetTattooIndex(Sim::eTattooLocation location) const
{
    return tattooIndices[location];
}

u8 &CasSimDescription::TattooIndex(Sim::eTattooLocation location)
{
    return tattooIndices[location];
}

f32 CasSimDescription::GetLatticeMorphValue(Sim::eLatticeMorphRegion region) const
{
    return latticeMorphValues[region];
}

f32 &CasSimDescription::LatticeMorphValue(Sim::eLatticeMorphRegion region)
{
    return latticeMorphValues[region];
}

f32 CasSimDescription::GetMorphTargetValue(Sim::eMorphTargetRegion region) const
{
    return morphTargetValues[region];
}

void CasSimDescription::SetMorphTargetValue(Sim::eMorphTargetRegion region, f32 value)
{
    morphTargetValues[region] = value;
}

f32 &CasSimDescription::MorphTargetValue(Sim::eMorphTargetRegion region)
{
    return morphTargetValues[region];
}

f32 CasSimDescription::GetMorphTargetWeightScaleFactor()
{
    return 1.0f;
}

u8 CasSimDescription::GetPersonalityValue(PersonalityEnum personality) const
{
    return personalityValues[personality];
}

u8 &CasSimDescription::PersonalityValue(PersonalityEnum personality)
{
    return personalityValues[personality];
}

u8 *CasSimDescription::GetPersonalityArray()
{
    return personalityValues;
}

u8 CasSimDescription::GetArchetype() const
{
    return archetype;
}

u8 &CasSimDescription::Archetype()
{
    return archetype;
}

u8 CasSimDescription::GetBodyType() const
{
    return bodyType;
}

u8 &CasSimDescription::BodyType()
{
    return bodyType;
}

u8 CasSimDescription::GetSkinColor() const
{
    return skinColor;
}

u8 &CasSimDescription::SkinColor()
{
    return skinColor;
}

u32 CasSimDescription::GetTypeCode() const
{
    if (flags.Get(0))
        return typeInfo->typeCodes[0];
    return typeInfo->typeCodes[1];
}

SimBodyPart *CasSimDescription::GetBodyPart(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)];
}

SimBodyPart *CasSimDescription::GetBodyPartByIndex(u8 index) const
{
    if (index < Character::BODY_PART_COUNT)
        return bodyParts[index];
    return 0;
}

void CasSimDescription::SwitchToFirstAvailableStyle(Character::eBodyPart part)
{
    bodyParts[GetBodyPartIndex(part)]->SwitchToFirstAvailableStyle();
}

void CasSimDescription::AdjustToLegalStyle(Character::eBodyPart part)
{
    bodyParts[GetBodyPartIndex(part)]->AdjustToLegalStyle();
}

const char *CasSimDescription::GetBodyPartName(Character::eBodyPart part) const
{
    return typeInfo->bodyPartTypes[GetBodyPartIndex(part)].name;
}

u8 CasSimDescription::GetNumBodyParts() const
{
    return Character::BODY_PART_COUNT;
}
