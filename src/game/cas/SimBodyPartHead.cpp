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

s32 SimBodyPartHead::GetIsDamaged()
{
    return 0;
}

s32 SimBodyPartHead::GetCurrentHitPoints()
{
    return 0;
}

BitFlags SimBodyPartHead::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags(0);
}
