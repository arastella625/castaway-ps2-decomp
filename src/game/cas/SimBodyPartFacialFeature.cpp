#include "game/cas/SimBodyPartFacialFeature.h"

#include "common/ReconBuffer.h"
#include "game/cas/CasSimDescription.h"
#include "game/cas/CasUtils.h"
#include "game/simulator/core/Neighborhood.h"

void SimBodyPartFacialFeature::DoStream(ReconBuffer *buffer, int version)
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

void SimBodyPartFacialFeature::CopyFrom(const SimBodyPart *other)
{
    flags = other->flags;
    SetClothingSetIndex(other->clothingSetIndex);
    modelCodeIndex = other->modelCodeIndex;
    modelIndex = other->modelIndex;
    styleIndex = other->styleIndex;
    textureIndex = other->textureIndex;
    degradationState = other->degradationState;
    clothingItem = other->clothingItem;
    if (other->GetType() == 0x1b) {
        const SimBodyPartFacialFeature *source = (const SimBodyPartFacialFeature *)other;
        currentHitPoints = source->currentHitPoints;
        isDamaged = source->isDamaged;
    }
    UpdateRelatedData();
}

int SimBodyPartFacialFeature::GetType() const
{
    return 0x1b;
}

int SimBodyPartFacialFeature::GetBodyPartIndex() const
{
    return 1;
}

int SimBodyPartFacialFeature::GetBodyPartDBIndex() const
{
    return 1;
}

int SimBodyPartFacialFeature::GetTattooLocation() const
{
    return 4;
}

int SimBodyPartFacialFeature::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartFacialFeature::GetLayoutMapRect() const
{
    return TRect<u16>(0x40, 0x40, 0x80, 0x60);
}

int SimBodyPartFacialFeature::GetImageModelSetType() const
{
    return 3;
}

u32 SimBodyPartFacialFeature::GetReflectionMaskID(bool a) const
{
    return 0;
}

u8 SimBodyPartFacialFeature::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return styleIndex;
}

u8 SimBodyPartFacialFeature::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex;
}

void SimBodyPartFacialFeature::SwitchToFirstAvailableDegradationStage()
{
    SimBodyPart::SwitchToFirstAvailableDegradationStage();
    currentHitPoints = clothingSetData->hitPointsPerDegradationState;
    isDamaged = false;
}

u8 SimBodyPartFacialFeature::GetArmorValue(Weather::eDamageType type)
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

s32 SimBodyPartFacialFeature::GetIsDamaged()
{
    return isDamaged;
}

void SimBodyPartFacialFeature::SetIsDamaged(bool damaged)
{
    isDamaged = damaged;
}

s32 SimBodyPartFacialFeature::GetCurrentHitPoints()
{
    return currentHitPoints;
}

void SimBodyPartFacialFeature::SetCurrentHitPoints(int hitPoints)
{
    currentHitPoints = hitPoints;
}

int SimBodyPartFacialFeature::GetValidClothingSetIndexes(
    eastl::vector<Sim::ClothingSetIndexInfo, eastl::allocator> &indexes)
{
    int count = 0;
    bool flag = GetCharacterFlag(0);
    bool useArchetype = CasUtils::UseArchetype();
    SimBodyPart *temp = CreateInstance((Character::eBodyPart)GetType(), flag);
    u8 total = GetNumPossibleClothingSets();
    u8 nude = GetNudeClothingSetIndex(flag);
    if (nude < total) {
        temp->SetClothingSetIndex(nude);
        temp->SwitchToFirstAvailableDegradationStage();
        Sim::ClothingSetIndexInfo info;
        info.clothingSetIndex = nude;
        info.itemID = 0;
        info.flags = 1;
        info.iconTextureID = CasUtils::GetNoneIconTextureID();
        indexes.push_back(info);
        count++;
    }
    s16 personData = 0;
    if (!useArchetype) {
        int neighborID = UIDB::UIDBGetInt("CAS_neighborID");
        Neighbor *neighbor = GetNeighborhood()->FindNeighborByID(neighborID);
        if (neighbor != 0)
            personData = neighbor->GetNeighborPersonData(0xe);
    }
    for (u8 i = 0; i < total; i++) {
        Sim::BodyPartGeometryTSC6 *set = &(*geometryVector)[i];
        if (i != nude && strncmp("generic", set->name, 10) != 0
            && (useArchetype || (int)((set->flags & 0xf00) >> 8) * 100 <= personData)) {
            temp->SetClothingSetIndex(i);
            temp->SwitchToFirstAvailableDegradationStage();
            if (useArchetype || temp->StyleIndex() <= styleIndex) {
                // Redundant second setup, kept to match the original code.
                temp->SetClothingSetIndex(i);
                temp->SwitchToFirstAvailableDegradationStage();
                Sim::ClothingSetIndexInfo info;
                info.clothingSetIndex = i;
                info.itemID = 0;
                info.flags = 1;
                info.iconTextureID = temp->GetIconTextureID();
                indexes.push_back(info);
                count++;
            }
        }
    }
    if (temp != 0)
        delete temp;
    return count;
}

u8 SimBodyPartFacialFeature::GetClothingSetUIIndex() const
{
    bool flag = GetCharacterFlag(0);
    bool useArchetype = CasUtils::UseArchetype();
    SimBodyPart *temp = CreateInstance((Character::eBodyPart)GetType(), flag);
    u8 total = GetNumPossibleClothingSets();
    u8 nude = GetNudeClothingSetIndex(flag);
    // BUG: the early returns below leak `temp`.
    if (clothingSetIndex == nude)
        return 0;
    s16 personData = 0;
    if (!useArchetype) {
        int neighborID = UIDB::UIDBGetInt("CAS_neighborID");
        Neighbor *neighbor = GetNeighborhood()->FindNeighborByID(neighborID);
        if (neighbor != 0)
            personData = neighbor->GetNeighborPersonData(0xe);
    }
    int uiIndex = 1;
    for (u8 i = 0; i < total; i++) {
        Sim::BodyPartGeometryTSC6 *set = &(*geometryVector)[i];
        if (i != nude && strncmp("generic", set->name, 10) != 0
            && (useArchetype || (int)((set->flags & 0xf00) >> 8) * 100 <= personData)) {
            temp->SetClothingSetIndex(i);
            temp->SwitchToFirstAvailableDegradationStage();
            if (useArchetype || temp->StyleIndex() <= styleIndex) {
                if (i == clothingSetIndex)
                    return uiIndex;
                uiIndex++;
            }
        }
    }
    if (temp != 0)
        delete temp;
    return 0;
}

// Like SimBodyPart's, but the empty case is a sim without character flag 0.
EString SimBodyPartFacialFeature::GetTextureName(bool a) const
{
    if (!GetCharacterFlag(0))
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
