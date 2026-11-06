#include "game/cas/SimBodyPartHead.h"

int SimBodyPartHead::GetType() const
{
    return 0x1a;
}

int SimBodyPartHead::GetBodyPartIndex() const
{
    return 0;
}

int SimBodyPartHead::GetBodyPartDBIndex() const
{
    return 0;
}

int SimBodyPartHead::GetTattooLocation() const
{
    return 3;
}

bool SimBodyPartHead::IsLayered() const
{
    return true;
}

bool SimBodyPartHead::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartHead::GetLayoutMapNumber() const
{
    return 0;
}

TRect<u16> SimBodyPartHead::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0, 0x40, 0x40);
}

int SimBodyPartHead::GetImageModelSetType() const
{
    return 2;
}

#ifdef NON_MATCHING
// Two of 28 words differ: the last add puts its result in v0, and the game uses v1
// (addu v1,v1,v0 then andi v0,v1,0xff). Five phrasings tried; this is the closest.
u8 SimBodyPartHead::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    if (modelCodeIndex == 0)
        return 0;
    // The cast to plain char, which is signed here, is what the game does.
    int index = GetNumPossibleStyles(modelCodeIndex) * (modelCodeIndex - 1) + 1;
    index += (char)styleIndex;
    return index;
}
#endif

u8 SimBodyPartHead::CalculateModelCodeIndexFromModelIndex(u8 modelIndex) const
{
    if (modelIndex == 0)
        return 0;
    return (modelIndex - 1) / GetNumPossibleStyles(1) + 1;
}

u8 SimBodyPartHead::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    if (modelIndex == 0)
        return 0;
    return (modelIndex - 1) % GetNumPossibleStyles(1);
}

s32 SimBodyPartHead::GetIsDamaged()
{
    return 0;
}

s32 SimBodyPartHead::GetCurrentHitPoints()
{
    return 0;
}

BitFlags<u32> SimBodyPartHead::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags<u32>(0);
}
