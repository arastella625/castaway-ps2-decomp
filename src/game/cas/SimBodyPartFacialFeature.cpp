#include "game/cas/SimBodyPartFacialFeature.h"

void SimBodyPartFacialFeature::CopyFrom(const SimBodyPart *other)
{
    flags = other->flags;
    SetClothingSetIndex(other->clothingSetIndex);
    modelCodeIndex = other->modelCodeIndex;
    modelIndex = other->modelIndex;
    styleIndex = other->styleIndex;
    textureIndex = other->textureIndex;
    degradationState = other->degradationState;
    damageState = other->damageState;
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
