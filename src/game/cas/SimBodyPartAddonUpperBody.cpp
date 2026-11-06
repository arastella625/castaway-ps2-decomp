#include "game/cas/SimBodyPartAddonUpperBody.h"

int SimBodyPartAddonUpperBody::GetType() const
{
    return 0x25;
}

int SimBodyPartAddonUpperBody::GetBodyPartIndex() const
{
    return 0xb;
}

int SimBodyPartAddonUpperBody::GetBodyPartDBIndex() const
{
    return 0xb;
}

int SimBodyPartAddonUpperBody::GetTattooLocation() const
{
    return 4;
}

int SimBodyPartAddonUpperBody::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartAddonUpperBody::GetLayoutMapRect() const
{
    return TRect<u16>(0x40, 0x60, 0x80, 0x80);
}

int SimBodyPartAddonUpperBody::GetImageModelSetType() const
{
    return 0xd;
}

s32 SimBodyPartAddonUpperBody::GetIsDamaged()
{
    return 0;
}

BitFlags<u32> SimBodyPartAddonUpperBody::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags<u32>(0);
}
