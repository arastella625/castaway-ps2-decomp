#include "game/cas/CasSimDescription.h"

#include "common/EResourceManager.h"
#include "game/cas/CasUtils.h"

Character::eSpecies CasSimDescription::GetSpecies() const
{
    return (Character::eSpecies)(flags[0] == false);
}

int CasSimDescription::GetSimType() const
{
    return 0;
}

void CasSimDescription::CopyBodyPart(Character::eBodyPart part, const CasSimDescription &other)
{
    u8 index = GetBodyPartIndex(part);
    SimBodyPart *source = other.GetBodyPartByIndex(index);
    if (source != 0) {
        bodyParts[index]->CopyFrom(source);
    } else {
        bodyParts[index]->SetClothingSetIndex(other.GetClothingSetIndex(part));
        bodyParts[index]->SwitchToFirstAvailableDegradationStage();
    }
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

const char *CasSimDescription::GetTypeCode() const
{
    if (flags[0])
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

u32 CasSimDescription::GetSkinTextureID(Sim::eSimPartsMapLocation location, bool a) const
{
    if (skinColor >= textureInfo->skins.size())
        return 0;
    u8 index = skinTextureOffset + typeInfo->skinTexturesPerLocation * location;
    Sim::BodyPartSkinTSC6 *skin = &textureInfo->skins[skinColor];
    if (skin == 0 || index >= skin->textureIDs.size())
        return 0;
    return CasUtils::GetGameStateTextureID(skin->textureIDs[index], a);
}

EString CasSimDescription::GetSkinTextureName(Sim::eSimPartsMapLocation location, bool a) const
{
    EString name;
    if (skinColor >= textureInfo->skins.size())
        return 0;
    u8 index = skinTextureOffset + typeInfo->skinTexturesPerLocation * location;
    Sim::BodyPartSkinTSC6 *skin = &textureInfo->skins[skinColor];
    if (skin == 0 || index >= skin->textureIDs.size())
        return 0;
    name.Format("%s_%u%s", skin->name, skinTextureOffset, a ? "_GAME" : "");
    return name;
}

u32 CasSimDescription::GetTattooTextureID(Sim::eTattooLocation location, bool a) const
{
    if (tattooIndices[location] >= (u8)textureInfo->tattooTextures[(u8)location].size())
        return 0;
    return CasUtils::GetGameStateTextureID(
        textureInfo->tattooTextures[(u8)location][tattooIndices[location]].textureID, a);
}

EString CasSimDescription::GetTattooClothingSetName(Sim::eTattooLocation location) const
{
    EString name;
    if (tattooIndices[location] < (u8)textureInfo->tattooTextures[(u8)location].size())
        name.Format("%s", textureInfo->tattooTextures[(u8)location][tattooIndices[location]].name);
    return name;
}

EString CasSimDescription::GetCompositeTextureBaseName() const
{
    EString name;
    name.Format("%S_%S", firstName, lastName);
    name.MakeLegalSymbolName();
    return name;
}

EString CasSimDescription::GetCompositeTextureName(const char *base, Sim::eSimPartsMapLocation location) const
{
    EString name;
    switch (location) {
    case 1:
        name.Format("%s_cur_%s", GetTypeCode(), base);
        break;
    case 2:
        name.Format("%s_cll_%s", GetTypeCode(), base);
        break;
    case 3:
        name.Format("%s_clr_%s", GetTypeCode(), base);
        break;
    case 0:
    default:
        name.Format("%s_cul_%s", GetTypeCode(), base);
        break;
    }
    return name;
}

u32 CasSimDescription::GetCompositeTextureID(const char *base, Sim::eSimPartsMapLocation location) const
{
    return EResourceManager::CalcId(GetCompositeTextureName(base, location).c_str(), true);
}

void CasSimDescription::SwitchToFirstAvailableStyle(Character::eBodyPart part)
{
    bodyParts[GetBodyPartIndex(part)]->SwitchToFirstAvailableStyle();
}

void CasSimDescription::AdjustToLegalStyle(Character::eBodyPart part)
{
    bodyParts[GetBodyPartIndex(part)]->AdjustToLegalStyle();
}

void CasSimDescription::InitializeToDefaultCostume(bool a)
{
    ClearSim(a, false, true);
    for (u8 part = 0x1a; part <= 0x28; part++)
        bodyParts[(u8)(part - 0x1a)]->SetToDefault(a);
}

void CasSimDescription::RemoveClothesForTattoo()
{
    for (u8 part = 0x1d; part < 0x28; part++)
        bodyParts[(u8)(part - 0x1a)]->SetToNude(GetFlag(0));
}

// A clothing set with flag bit 7 takes its texture variations from dyes, except when the
// requirements cheat is on or the sim is a ready-made archetype.
bool CasSimDescription::UseDyesForTextureVariation(Character::eBodyPart part)
{
    if (PlanManager::cheatIgnoreRequirements)
        return false;
    if (!(bodyParts[GetBodyPartIndex(part)]->flags.bits & 0x80))
        return false;
    return !CasUtils::UseArchetype();
}

const char *CasSimDescription::GetBodyPartName(Character::eBodyPart part) const
{
    return typeInfo->bodyPartTypes[GetBodyPartIndex(part)].name;
}

u8 CasSimDescription::GetNumBodyParts() const
{
    return Character::BODY_PART_COUNT;
}

u8 CasSimDescription::GetBodyPartIndex(Character::eBodyPart part) const
{
    return CasUtils::GetBodyPartIndex(part);
}

u8 CasSimDescription::GetClothingSetUIIndex(Character::eBodyPart part) const
{
    u8 index = CasUtils::GetBodyPartDBIndex(part);
    if (index == 0x7f)
        return 0;
    return bodyParts[index]->GetClothingSetUIIndex();
}
