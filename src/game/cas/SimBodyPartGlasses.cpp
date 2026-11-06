#include "game/cas/SimBodyPartGlasses.h"

int SimBodyPartGlasses::GetType() const
{
    return 0x1d;
}

int SimBodyPartGlasses::GetBodyPartIndex() const
{
    return 3;
}

int SimBodyPartGlasses::GetBodyPartDBIndex() const
{
    return 3;
}

int SimBodyPartGlasses::GetTattooLocation() const
{
    return 4;
}

int SimBodyPartGlasses::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartGlasses::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0x20, 0x20, 0x40);
}

int SimBodyPartGlasses::GetImageModelSetType() const
{
    return 5;
}

s32 SimBodyPartGlasses::GetIsDamaged()
{
    return 0;
}

BitFlags<u32> SimBodyPartGlasses::ApplyDamage(const Weather::DamageEvent &event, s16 a)
{
    return BitFlags<u32>(0);
}
