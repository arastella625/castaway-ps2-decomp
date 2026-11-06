#include "game/cas/SimBodyPartBelt.h"

int SimBodyPartBelt::GetType() const
{
    return 0x24;
}

int SimBodyPartBelt::GetBodyPartIndex() const
{
    return 0xa;
}

int SimBodyPartBelt::GetBodyPartDBIndex() const
{
    return 0xa;
}

int SimBodyPartBelt::GetTattooLocation() const
{
    return 4;
}

int SimBodyPartBelt::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartBelt::GetLayoutMapRect() const
{
    return TRect<u16>(0x20, 0, 0x40, 0x20);
}

int SimBodyPartBelt::GetImageModelSetType() const
{
    return 0xc;
}

s32 SimBodyPartBelt::GetIsDamaged()
{
    return 0;
}

BitFlags SimBodyPartBelt::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags(0);
}
