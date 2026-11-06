#include "game/cas/SimBodyPartArmAccessory.h"

int SimBodyPartArmAccessory::GetType() const
{
    return 0x23;
}

int SimBodyPartArmAccessory::GetBodyPartIndex() const
{
    return 9;
}

int SimBodyPartArmAccessory::GetBodyPartDBIndex() const
{
    return 9;
}

int SimBodyPartArmAccessory::GetTattooLocation() const
{
    return 0;
}

bool SimBodyPartArmAccessory::UseStyleCodeInTextureName() const
{
    return false;
}

int SimBodyPartArmAccessory::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartArmAccessory::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0, 0x20, 0x20);
}

int SimBodyPartArmAccessory::GetImageModelSetType() const
{
    return 0xb;
}

u8 SimBodyPartArmAccessory::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return styleIndex;
}

u8 SimBodyPartArmAccessory::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex;
}

s32 SimBodyPartArmAccessory::GetIsDamaged()
{
    return 0;
}

BitFlags SimBodyPartArmAccessory::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags(0);
}
