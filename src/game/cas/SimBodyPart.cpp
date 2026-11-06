#include "game/cas/SimBodyPart.h"

#include "common/EResourceManager.h"
#include "common/ReconBuffer.h"
#include "game/cas/CasSimDescription.h"

void SimBodyPart::DoStream(ReconBuffer *buffer, int version)
{
    if (version >= 3) {
        buffer->Recon32((int *)&flags, 1);
        buffer->Recon8((s8 *)&clothingSetIndex, 1);
        buffer->Recon8((s8 *)&modelCodeIndex, 1);
        buffer->Recon8((s8 *)&modelIndex, 1);
        buffer->Recon8((s8 *)&styleIndex, 1);
        buffer->Recon8((s8 *)&textureIndex, 1);
        buffer->Recon8((s8 *)&degradationState, 1);
        if (buffer->IsReading() && geometryVector != 0)
            SetClothingSetIndex(clothingSetIndex);
    }
}

void SimBodyPart::CopyFrom(const SimBodyPart *other)
{
    flags = other->flags;
    SetClothingSetIndex(other->clothingSetIndex);
    modelCodeIndex = other->modelCodeIndex;
    modelIndex = other->modelIndex;
    styleIndex = other->styleIndex;
    textureIndex = other->textureIndex;
    degradationState = other->degradationState;
    damageState = other->damageState;
    UpdateRelatedData();
}

bool SimBodyPart::IsValid(bool a, bool b)
{
    bool valid = false;
    if (geometryVector != 0 && bodyPartTypeInfo != 0 && IsValidClothingSetIndex(a, b)
        && IsValidModelIndex(a, b) && IsValidModelCodeIndex(a, b) && IsValidStyleIndex(a, b)
        && IsValidTextureIndex(a, b) && IsValidDegradationState(a, b))
        valid = true;
    return valid;
}

void SimBodyPart::Validate()
{
    IsValid(true, false);
}

void SimBodyPart::SetClothingSetIndex(u8 index)
{
    if (clothingSetIndex >= geometryVector->size())
        index = GetNudeClothingSetIndex(flags[0]);
    clothingSetIndex = index;
    clothingSetData = &(*geometryVector)[clothingSetIndex];
    flags.bits = clothingSetData->flags;
}

bool SimBodyPart::IsLayered() const
{
    return false;
}

bool SimBodyPart::UseStyleCodeInTextureName() const
{
    return true;
}

bool SimBodyPart::IsRequiredBodyPart() const
{
    return false;
}

EVec3 SimBodyPart::GetDefaultCameraPos() const
{
    return EVec3(0.3f, -3.05f, 1.52f);
}

EVec3 SimBodyPart::GetDefaultCameraTarget() const
{
    return EVec3(-1.52f, 2.74f, 0.61f);
}

int SimBodyPart::GetIconPoseAnimID() const
{
    return 0;
}

u8 SimBodyPart::GetNumPossibleClothingSets() const
{
    if (GetBodyPartDBIndex() == 0x7f)
        return 0;
    return geometryVector->size();
}

u8 SimBodyPart::GetNumPossibleModelCodes() const
{
    return bodyPartTypeInfo->modelCodes.size();
}

u8 SimBodyPart::GetNumPossibleStyles(u8 modelCodeIndex) const
{
    return bodyPartTypeInfo->modelCodes[modelCodeIndex].styles.size();
}

u8 SimBodyPart::GetNumValidStyles(u8 modelCodeIndex) const
{
    u8 count = GetNumPossibleStyles(modelCodeIndex);
    int total = count;
    for (int i = 0; i < total; i++) {
        u8 model = CalculateModelIndexFromModelCodeIndexAndStyleIndex(modelCodeIndex, i);
        if (clothingSetData == 0)
            count--;
        else if (model >= clothingSetData->modelIDs.size() || clothingSetData->modelIDs[model] == 0)
            count--;
    }
    return count;
}

u8 SimBodyPart::GetNumPossibleModels() const
{
    return GetNumPossibleModelCodes() * GetNumPossibleStyles(0);
}

u8 SimBodyPart::GetNumPossibleTextureVariations() const
{
    return clothingSetData->textureVariations.size();
}

u8 SimBodyPart::GetNumPossibleDegradationStates() const
{
    return bodyPartTypeInfo->modelCodes[GetDegradationModelCodeIndex()].styles[styleIndex].numDegradationStates;
}

bool SimBodyPart::IsGeneric() const
{
    if (clothingSetIndex >= geometryVector->size())
        return false;
    switch (styleIndex) {
    case 1: if (clothingSetData->flags & 0x02) return true; break;
    case 2: if (clothingSetData->flags & 0x04) return true; break;
    case 3: if (clothingSetData->flags & 0x08) return true; break;
    case 4: if (clothingSetData->flags & 0x10) return true; break;
    case 5: if (clothingSetData->flags & 0x20) return true; break;
    }
    return false;
}

const char *SimBodyPart::GetModelCode() const
{
    if (modelCodeIndex < bodyPartTypeInfo->modelCodes.size())
        return bodyPartTypeInfo->modelCodes[modelCodeIndex].code;
    return "";
}

const char *SimBodyPart::GetStyleCode() const
{
    if (modelCodeIndex < bodyPartTypeInfo->modelCodes.size()) {
        if (styleIndex < bodyPartTypeInfo->modelCodes[modelCodeIndex].styles.size())
            return bodyPartTypeInfo->modelCodes[modelCodeIndex].styles[styleIndex].code;
    }
    return "";
}

const char *SimBodyPart::GetClothingSetName() const
{
    if (IsGeneric())
        return "generic";
    if (clothingSetIndex < (u8)geometryVector->size())
        return clothingSetData->name;
    return "";
}

const char *SimBodyPart::GetTextureVariationName() const
{
    if (clothingSetIndex < geometryVector->size()) {
        if (textureIndex < clothingSetData->textureVariations.size())
            return clothingSetData->textureVariations[textureIndex];
    }
    return "";
}

#ifdef NON_MATCHING
// One of 29 words differs. The game gives each `return 0` its own copy of the return; here
// the second one branches back to the first. Four phrasings tried; this is the closest.
u32 SimBodyPart::GetModelID() const
{
    if (clothingSetIndex >= geometryVector->size())
        return 0;
    if (modelIndex < clothingSetData->modelIDs.size())
        return clothingSetData->modelIDs[modelIndex];
    return 0;
}
#endif

u32 SimBodyPart::GetTextureID(bool a) const
{
    return GetModelID() != 0 ? EResourceManager::CalcId(GetTextureName(a).c_str(), true) : 0;
}

u32 SimBodyPart::GetReflectionMaskID(bool a) const
{
    return GetModelID() != 0 ? EResourceManager::CalcId(GetReflectionMaskName(a).c_str(), true) : 0;
}

u32 SimBodyPart::GetIconTextureID() const
{
    return EResourceManager::CalcId(GetIconTextureName().c_str(), true);
}

void SimBodyPart::SetToDefault(bool a)
{
    SetClothingSetIndex(GetDefaultClothingSetIndex(a));
    SwitchToFirstAvailableDegradationStage();
}

u8 SimBodyPart::GetNudeClothingSetIndex(bool a) const
{
    return bodyPartTypeInfo->nudeClothingSetIndexes[a == false];
}

void SimBodyPart::SetToNude(bool a)
{
    SetClothingSetIndex(GetNudeClothingSetIndex(a));
    SwitchToFirstAvailableDegradationStage();
}

u8 SimBodyPart::CalculateModelCodeIndexFromStyleIndex(u8 styleIndex) const
{
    return 0;
}

u8 SimBodyPart::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return 0;
}

u8 SimBodyPart::CalculateModelCodeIndexFromModelIndex(u8 modelIndex) const
{
    return 0;
}

u8 SimBodyPart::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return 0;
}

void SimBodyPart::SwitchToFirstAvailableDegradationStage()
{
    modelCodeIndex = 0;
    modelIndex = 0;
    styleIndex = 0;
    textureIndex = 0;
    degradationState = 0;
    flags.bits = 0;
    if (IsValidClothingSetIndex(false, true)) {
        flags.bits = clothingSetData->flags;
        for (u8 i = 0; i < (u8)clothingSetData->modelIDs.size(); i++) {
            if (clothingSetData->modelIDs[i] != 0) {
                modelIndex = i;
                modelCodeIndex = CalculateModelCodeIndexFromModelIndex(i);
                styleIndex = CalculateStyleIndexFromModelIndex(i);
                break;
            }
        }
        UpdateRelatedData();
    }
}

void SimBodyPart::UpdateRelatedData()
{
}

u8 SimBodyPart::GetArmorValue(Weather::eDamageType type)
{
    if (!IsValidClothingSetIndex(false, true))
        return 0xff;
    if (type >= Weather::DAMAGE_TYPE_COUNT)
        return 0;
    return clothingSetData->armorValues[type];
}

s32 SimBodyPart::GetHitPointsPerDegradationState()
{
    if (!IsValidClothingSetIndex(false, true))
        return 0;
    return clothingSetData->hitPointsPerDegradationState;
}

s32 SimBodyPart::GetIsDamaged()
{
    if (damageState != 0)
        return damageState->isDamaged;
    return 0;
}

void SimBodyPart::SetIsDamaged(bool damaged)
{
    if (damageState != 0)
        damageState->isDamaged = damaged;
}

s32 SimBodyPart::GetCurrentHitPoints()
{
    if (damageState != 0)
        return damageState->currentHitPoints;
    return 0;
}

void SimBodyPart::SetCurrentHitPoints(int hitPoints)
{
    if (damageState != 0)
        damageState->currentHitPoints = hitPoints;
}

u8 SimBodyPart::GetDegradationModelCodeIndex() const
{
    return modelCodeIndex;
}

Sim::BodyPartGeometryTSC6 *SimBodyPart::GetClothingSetData() const
{
    return clothingSetData;
}

BodyPartTypeInfo *SimBodyPart::GetBodyPartTypeInfo() const
{
    return bodyPartTypeInfo;
}

EString SimBodyPart::GetModelName() const
{
    char buffer[256];
    CalculateNameBase(buffer);
    CalculateNameStyle(buffer + strlen(buffer));
    EString name;
    name.Format("%s", buffer);
    return name;
}

// An empty name while the part is the nude set. The texture's name is the model's, plus
// the texture variation and the degradation state; `a` asks for the in-game copy.
EString SimBodyPart::GetTextureName(bool a) const
{
    if (clothingSetIndex == GetNudeClothingSetIndex(GetCharacterFlag(0)))
        return "";
    char buffer[256];
    CalculateNameBase(buffer);
    if (UseStyleCodeInTextureName())
        CalculateNameStyle(buffer + strlen(buffer));
    CalculateNameTextureVariation(buffer + strlen(buffer));
    CalculateNameDegradationState(buffer + strlen(buffer));
    if (a)
        CalculateNameLowRes(buffer + strlen(buffer));
    EString name;
    name.Format("%s", buffer);
    return name;
}

EString SimBodyPart::GetReflectionMaskName(bool a) const
{
    if (clothingSetIndex == GetNudeClothingSetIndex(GetCharacterFlag(0)))
        return "";
    char buffer[256];
    CalculateNameBase(buffer);
    if (UseStyleCodeInTextureName())
        CalculateNameStyle(buffer + strlen(buffer));
    CalculateNameDegradationState(buffer + strlen(buffer));
    Sprintf(buffer + strlen(buffer), "_RFL");
    if (a)
        CalculateNameLowRes(buffer + strlen(buffer));
    EString name;
    name.Format("%s", buffer);
    return name;
}

EString SimBodyPart::GetIconTextureName() const
{
    char buffer[256];
    CalculateNameBase(buffer);
    CalculateNameStyle(buffer + strlen(buffer));
    CalculateNameTextureVariation(buffer + strlen(buffer));
    CalculateNameDegradationState(buffer + strlen(buffer));
    Sprintf(buffer + strlen(buffer), "_ICON");
    EString name;
    name.Format("%s", buffer);
    return name;
}
