#include "game/cas/SimBodyPartNecklaceEarring.h"

int SimBodyPartNecklaceEarring::GetType() const
{
    return 0x22;
}

int SimBodyPartNecklaceEarring::GetBodyPartIndex() const
{
    return 8;
}

int SimBodyPartNecklaceEarring::GetBodyPartDBIndex() const
{
    return 8;
}

int SimBodyPartNecklaceEarring::GetTattooLocation() const
{
    return 4;
}

int SimBodyPartNecklaceEarring::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartNecklaceEarring::GetLayoutMapRect() const
{
    return TRect<u16>(0x20, 0x20, 0x40, 0x40);
}

int SimBodyPartNecklaceEarring::GetImageModelSetType() const
{
    return 0xa;
}

s32 SimBodyPartNecklaceEarring::GetIsDamaged()
{
    return 0;
}

BitFlags<u32> SimBodyPartNecklaceEarring::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags<u32>(0);
}
