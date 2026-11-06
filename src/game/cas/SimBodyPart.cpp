#include "game/cas/SimBodyPart.h"

#include "common/EResourceManager.h"
#include "common/ReconBuffer.h"
#include "game/cas/CasSimDescription.h"
#include "game/cas/CasUtils.h"
#include "game/simulator/core/ClothingItem.h"

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
    clothingItem = other->clothingItem;
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

// The six checks below share a shape: report through AssertWithInfo when `b` is set, repair
// the index when `a` is set, then check again without either.
bool SimBodyPart::IsValidClothingSetIndex(bool a, bool b)
{
    if (clothingSetIndex < geometryVector->size())
        return true;
    if (b) {
        char buffer[512];
        Sprintf(buffer, "Invalid index %d into %s model array, which only has %d models", clothingSetIndex,
                bodyPartTypeInfo->name, geometryVector->size());
        AssertWithInfo(buffer);
    }
    if (a) {
        SetClothingSetIndex(GetDefaultClothingSetIndex(GetCharacterFlag(0)));
        return IsValidClothingSetIndex(false, false);
    }
    return false;
}

bool SimBodyPart::IsValidModelIndex(bool a, bool b)
{
    if (!IsValidClothingSetIndex(a, b))
        return false;
    if (modelIndex >= clothingSetData->modelIDs.size()
        || (IsRequiredBodyPart() && clothingSetData->modelIDs[modelIndex] == 0)) {
        if (b) {
            char buffer[512];
            Sprintf(buffer, "Invalid model index %d in clothing set %s of %s model array, which only has %d models",
                    modelIndex, clothingSetData->name, bodyPartTypeInfo->name, geometryVector->size());
            AssertWithInfo(buffer);
        }
        if (a) {
            SwitchToFirstAvailableStyle();
            return IsValidModelIndex(false, false);
        }
        return false;
    }
    return true;
}

bool SimBodyPart::IsValidModelCodeIndex(bool a, bool b)
{
    if (modelCodeIndex >= bodyPartTypeInfo->modelCodes.size()) {
        if (b) {
            char buffer[512];
            Sprintf(buffer, "Invalid model code index %d for body part %s", modelCodeIndex,
                    bodyPartTypeInfo->name);
            AssertWithInfo(buffer);
        }
        if (a) {
            SwitchToFirstAvailableStyle();
            return IsValidModelCodeIndex(false, false);
        }
        return false;
    }
    return true;
}

bool SimBodyPart::IsValidStyleIndex(bool a, bool b)
{
    if (modelCodeIndex >= bodyPartTypeInfo->modelCodes.size()
        || styleIndex >= bodyPartTypeInfo->modelCodes[modelCodeIndex].styles.size()) {
        if (b) {
            char buffer[512];
            Sprintf(buffer, "Invalid style code index %d for body part %s", styleIndex, bodyPartTypeInfo->name);
            AssertWithInfo(buffer);
        }
        if (a) {
            SwitchToFirstAvailableStyle();
            return IsValidStyleIndex(false, false);
        }
        return false;
    }
    return true;
}

bool SimBodyPart::IsValidTextureIndex(bool a, bool b)
{
    if (!IsValidClothingSetIndex(a, b))
        return false;
    if (textureIndex >= clothingSetData->textureVariations.size()) {
        if (b) {
            char buffer[512];
            Sprintf(buffer,
                    "Invalid texture index %d into clothing-set %s of the %s array, which only has %d textures",
                    textureIndex, clothingSetData->name, bodyPartTypeInfo->name,
                    clothingSetData->textureVariations.size());
            AssertWithInfo(buffer);
        }
        if (a) {
            textureIndex = 0;
            return IsValidTextureIndex(false, false);
        }
        return false;
    }
    return true;
}

bool SimBodyPart::IsValidDegradationState(bool a, bool b)
{
    if (modelCodeIndex >= bodyPartTypeInfo->modelCodes.size()
        || styleIndex >= bodyPartTypeInfo->modelCodes[modelCodeIndex].styles.size()
        || degradationState
               >= bodyPartTypeInfo->modelCodes[modelCodeIndex].styles[styleIndex].numDegradationStates) {
        if (b) {
            char buffer[512];
            Sprintf(buffer, "Invalid degradation state %d for body part %s", degradationState,
                    bodyPartTypeInfo->name);
            AssertWithInfo(buffer);
        }
        if (a) {
            degradationState = 0;
            return IsValidDegradationState(false, false);
        }
        return false;
    }
    return true;
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

// The "default" costume names a clothing set for most parts. A part it leaves out falls
// back to the index stored with the part's type.
u8 SimBodyPart::GetDefaultClothingSetIndex(bool a) const
{
    const Sim::CostumesTSC6 *costume = CasUtils::GetCostumeData("default");
    if (costume != 0) {
        const VECTOR<char *> *names = a ? &costume->flagSetNames : &costume->flagClearNames;
        if (strlen((*names)[GetBodyPartDBIndex()]) != 0)
            return CasUtils::GetClothingSetIndexByName(a, (Character::eBodyPart)GetType(),
                                                       (*names)[GetBodyPartDBIndex()]);
    }
    return bodyPartTypeInfo->defaultClothingSetIndexes[a == false];
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
    if (clothingItem != 0)
        return clothingItem->isDamaged;
    return 0;
}

void SimBodyPart::SetIsDamaged(bool damaged)
{
    if (clothingItem != 0)
        clothingItem->isDamaged = damaged;
}

s32 SimBodyPart::GetCurrentHitPoints()
{
    if (clothingItem != 0)
        return clothingItem->currentHitPoints;
    return 0;
}

void SimBodyPart::SetCurrentHitPoints(int hitPoints)
{
    if (clothingItem != 0)
        clothingItem->currentHitPoints = hitPoints;
}

// Takes what the armor doesn't stop off the hit points. Each time they run out the part
// degrades one state and starts again with a full set, until the damage is used up.
// The result has the bits of every Degrade call.
BitFlags<u32> SimBodyPart::ApplyDamage(const Weather::DamageEvent &event, s16 scale)
{
    BitFlags<u32> result(0);
    if (!(bodyPartTypeInfo->field_0xc & 2))
        return result;
    int max = scale * 255;
    s16 damage = 0;
    for (int type = 0; type < Weather::DAMAGE_TYPE_COUNT; type++) {
        s16 armor = GetArmorValue((Weather::eDamageType)type) * scale;
        if (armor < max) {
            s16 excess = event.amounts[type] - armor;
            if (excess > 0)
                damage += excess;
        }
    }
    while (damage > 0) {
        SetIsDamaged(true);
        if (GetCurrentHitPoints() - damage <= 0) {
            damage -= GetCurrentHitPoints();
            s16 perState = GetHitPointsPerDegradationState();
            BitFlags<u32> degraded = Degrade();
            if (degraded != 0) {
                result.setMask(degraded);
                if (clothingItem != 0)
                    clothingItem->CopyFromSimBodyPart(this);
                if (perState - damage > 0) {
                    SetCurrentHitPoints(perState - damage);
                    damage = 0;
                } else {
                    SetCurrentHitPoints(perState);
                }
            } else {
                damage = 0;
            }
        } else {
            SetCurrentHitPoints(GetCurrentHitPoints() - damage);
            damage = 0;
        }
    }
    return result;
}

// A texture variation's name is a string of dye letters. Sets dyes[n] for each letter it
// has: b, g, l, o, p, r, w, y are 1 to 8. The nude set needs none, except on the two types
// (0x1b and 0x1c) that are never nude.
void SimBodyPart::GetRequiredDyesForTextureVariation(u8 textureIndex, s16 *dyes)
{
    for (int i = 8; i >= 0; i--)
        dyes[i] = 0;
    if (GetType() == 0x1c || GetType() == 0x1b
        || GetClothingSetIndex() != GetNudeClothingSetIndex(GetCharacterFlag(0))) {
        if (clothingSetData != 0 && textureIndex < clothingSetData->textureVariations.size()) {
            for (const char *letter = clothingSetData->textureVariations[textureIndex];
                 letter != 0 && *letter != 0; letter++) {
                switch (*letter) {
                case 'b':
                    dyes[1] = 1;
                    break;
                case 'g':
                    dyes[2] = 1;
                    break;
                case 'l':
                    dyes[3] = 1;
                    break;
                case 'o':
                    dyes[4] = 1;
                    break;
                case 'p':
                    dyes[5] = 1;
                    break;
                case 'r':
                    dyes[6] = 1;
                    break;
                case 'w':
                    dyes[7] = 1;
                    break;
                case 'y':
                    dyes[8] = 1;
                    break;
                }
            }
        }
    }
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Picks the first model the clothing set has, then pulls the texture and the degradation
// state back into range.
void SimBodyPart::SwitchToFirstAvailableStyle()
{
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
        if (textureIndex >= clothingSetData->textureVariations.size())
            textureIndex = 0;
        if (degradationState >= NumDegradationStates())
            degradationState = MAX(0, NumDegradationStates() - 1);
        UpdateRelatedData();
    }
}

// Like SwitchToFirstAvailableStyle, but keeps the current model when it exists, and
// otherwise takes the next one after it.
void SimBodyPart::AdjustToLegalStyle()
{
    if (IsValidClothingSetIndex(false, true)) {
        flags.bits = clothingSetData->flags;
        bool found = false;
        for (u8 i = 0; i < (u8)clothingSetData->modelIDs.size(); i++) {
            if (clothingSetData->modelIDs[i] != 0 && i >= modelIndex) {
                modelIndex = i;
                modelCodeIndex = CalculateModelCodeIndexFromModelIndex(i);
                styleIndex = CalculateStyleIndexFromModelIndex(i);
                found = true;
                break;
            }
        }
        if (!found) {
            SwitchToFirstAvailableStyle();
            return;
        }
        if (textureIndex >= clothingSetData->textureVariations.size())
            textureIndex = 0;
        if (degradationState >= NumDegradationStates())
            degradationState = MAX(0, NumDegradationStates() - 1);
        UpdateRelatedData();
    }
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

#ifdef NON_MATCHING
// Six of 53 words differ, all in the inlined CasUtils::GetBodyPartTypeInfoTable. The game
// tests the table pointer and reads its count through one register, and reads the rows
// through a copy; this uses one register for all three and comes out a word shorter.
// Tried: early return, nested ifs, ?:, an inline wrapper, and a Wii-style out-parameter.
// A part that belongs to a sim asks the sim. One on its own looks its type up in the
// quickdata: the table's first row holds one code per value of character flag 0.
const char *SimBodyPart::GetCharacterType() const
{
    if (description != 0)
        return description->GetTypeCode();
    const CasSimTypeInfo *info = CasUtils::GetBodyPartTypeInfoTable();
    if (info != 0) {
        if (characterFlags[0])
            return info->typeCodes[0];
        return info->typeCodes[1];
    }
    return "";
}
#endif
