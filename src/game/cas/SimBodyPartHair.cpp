#include "game/cas/SimBodyPartHair.h"

#include "common/ReconBuffer.h"
#include "game/cas/CasSimDescription.h"

void SimBodyPartHair::DoStream(ReconBuffer *buffer, int version)
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
    // Hit points were added to the save format in version 7.
    if (version >= 7) {
        buffer->Recon32(&currentHitPoints, 1);
        buffer->ReconBool((bool *)&isDamaged);
    }
}

void SimBodyPartHair::CopyFrom(const SimBodyPart *other)
{
    flags = other->flags;
    SetClothingSetIndex(other->clothingSetIndex);
    modelCodeIndex = other->modelCodeIndex;
    modelIndex = other->modelIndex;
    styleIndex = other->styleIndex;
    textureIndex = other->textureIndex;
    degradationState = other->degradationState;
    clothingItem = other->clothingItem;
    // BUG: 0x1b is the facial feature's type; the hair's own is 0x1c. So hit points are
    // copied from a facial feature and never from another hair.
    if (other->GetType() == 0x1b) {
        const SimBodyPartHair *source = (const SimBodyPartHair *)other;
        currentHitPoints = source->currentHitPoints;
        isDamaged = source->isDamaged;
    }
    UpdateRelatedData();
}

int SimBodyPartHair::GetType() const
{
    return 0x1c;
}

int SimBodyPartHair::GetBodyPartIndex() const
{
    return 2;
}

int SimBodyPartHair::GetBodyPartDBIndex() const
{
    return 2;
}

int SimBodyPartHair::GetTattooLocation() const
{
    return 4;
}

bool SimBodyPartHair::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartHair::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartHair::GetLayoutMapRect() const
{
    return TRect<u16>(0x40, 0, 0x80, 0x40);
}

int SimBodyPartHair::GetImageModelSetType() const
{
    return 4;
}

const char *SimBodyPartHair::GetModelCode() const
{
    u8 index = modelCodeIndex;
    if (GetCharacterFlag(2))
        index = 1;
    if (index < bodyPartTypeInfo->modelCodes.size())
        return bodyPartTypeInfo->modelCodes[index].code;
    return "";
}

u32 SimBodyPartHair::GetModelID() const
{
    if (clothingSetIndex >= geometryVector->size())
        return 0;
    if (modelIndex >= clothingSetData->modelIDs.size())
        return 0;
    u8 index = modelIndex;
    if (GetCharacterFlag(2))
        index = CalculateModelIndexFromModelCodeIndexAndStyleIndex(1, styleIndex);
    return clothingSetData->modelIDs[index];
}

u32 SimBodyPartHair::GetReflectionMaskID(bool a) const
{
    return 0;
}

u8 SimBodyPartHair::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return styleIndex + modelCodeIndex * GetNumPossibleStyles(modelCodeIndex);
}

u8 SimBodyPartHair::CalculateModelCodeIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex / GetNumPossibleStyles(0);
}

u8 SimBodyPartHair::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex % GetNumPossibleStyles(0);
}

void SimBodyPartHair::SwitchToFirstAvailableDegradationStage()
{
    SimBodyPart::SwitchToFirstAvailableDegradationStage();
    currentHitPoints = clothingSetData->hitPointsPerDegradationState;
    isDamaged = false;
}

void SimBodyPartHair::UpdateRelatedData()
{
    if (GetCharacterFlag(0) && description != 0) {
        // 0x1b is the facial feature.
        SimBodyPart *part = description->GetBodyPart((Character::eBodyPart)0x1b);
        if (part != 0 && part->GetClothingSetIndex() == part->GetNudeClothingSetIndex(true)
            && part->StyleIndex() == 0)
            part->TextureIndex() = TextureIndex();
    }
}

u8 SimBodyPartHair::GetArmorValue(Weather::eDamageType type)
{
    if (!IsValidClothingSetIndex(false, true))
        return 0xff;
    switch (type) {
    case 0:
    case 1:
    case 3:
        return 0xff;
    case 2:
        return clothingSetData->armorValues[type];
    }
    return 0;
}

s32 SimBodyPartHair::GetIsDamaged()
{
    return isDamaged;
}

void SimBodyPartHair::SetIsDamaged(bool damaged)
{
    isDamaged = damaged;
}

s32 SimBodyPartHair::GetCurrentHitPoints()
{
    return currentHitPoints;
}

void SimBodyPartHair::SetCurrentHitPoints(int hitPoints)
{
    currentHitPoints = hitPoints;
}

u8 SimBodyPartHair::GetDegradationModelCodeIndex() const
{
    return 0;
}

const char *SimBodyPartHair::GetModelCodeForTexture() const
{
    return bodyPartTypeInfo->modelCodes[0].code;
}

EString SimBodyPartHair::GetTextureName(bool a) const
{
    char buffer[256];
    Sprintf(buffer, "%s_%s_%s", GetCharacterType(), GetModelCodeForTexture(), GetClothingSetName());
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

EString SimBodyPartHair::GetReflectionMaskName(bool a) const
{
    if (clothingSetIndex == GetNudeClothingSetIndex(GetCharacterFlag(0)))
        return "";
    char buffer[256];
    Sprintf(buffer, "%s_%s_%s", GetCharacterType(), GetModelCodeForTexture(), GetClothingSetName());
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

EString SimBodyPartHair::GetIconTextureName() const
{
    char buffer[256];
    Sprintf(buffer, "%s_%s_%s", GetCharacterType(), GetModelCodeForTexture(), GetClothingSetName());
    CalculateNameStyle(buffer + strlen(buffer));
    CalculateNameTextureVariation(buffer + strlen(buffer));
    CalculateNameDegradationState(buffer + strlen(buffer));
    Sprintf(buffer + strlen(buffer), "_ICON");
    EString name;
    name.Format("%s", buffer);
    return name;
}

// Model code 1 is the lower half of long hair, which only flag kHairBottomDisplayed shows.
void SimBodyPartHair::Validate()
{
    if (IsValid(true, false)) {
        if (modelCodeIndex == 1 || CalculateModelCodeIndexFromModelIndex(modelIndex) == 1) {
            char buffer[512];
            Sprintf(buffer, "Hair cannot be set to Hair-Bottom ModelCode - this is controlled only by the "
                            "CasSimDescription::kHairBottomDisplayed flag.  Resetting to HH");
            AssertWithInfo(buffer);
            modelCodeIndex = 0;
            GetNumPossibleStyles(0);
            modelIndex = styleIndex;
        }
    }
}
