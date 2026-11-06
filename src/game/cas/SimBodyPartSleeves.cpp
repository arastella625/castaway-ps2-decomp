#include "game/cas/SimBodyPartSleeves.h"

int SimBodyPartSleeves::GetType() const
{
    return 0x27;
}

int SimBodyPartSleeves::GetBodyPartIndex() const
{
    return 0xd;
}

int SimBodyPartSleeves::GetBodyPartDBIndex() const
{
    return 4;
}

int SimBodyPartSleeves::GetTattooLocation() const
{
    return 0;
}

bool SimBodyPartSleeves::IsLayered() const
{
    return true;
}

bool SimBodyPartSleeves::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartSleeves::GetLayoutMapNumber() const
{
    return 0;
}

TRect<u16> SimBodyPartSleeves::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0x40, 0x80, 0x80);
}

int SimBodyPartSleeves::GetImageModelSetType() const
{
    return 6;
}

u8 SimBodyPartSleeves::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return field_0x24 + styleIndex;
}

u8 SimBodyPartSleeves::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return (modelIndex - field_0x24) % GetNumPossibleStyles(0);
}

void SimBodyPartSleeves::SwitchToFirstAvailableDegradationStage()
{
    SwitchToMatchTorsoSettings();
}

s32 SimBodyPartSleeves::GetIsDamaged()
{
    return 0;
}

BitFlags<u32> SimBodyPartSleeves::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags<u32>(0);
}

BitFlags<u32> SimBodyPartSleeves::Degrade()
{
    return BitFlags<u32>(0);
}

BitFlags<u32> SimBodyPartSleeves::Repair()
{
    return BitFlags<u32>(0);
}
