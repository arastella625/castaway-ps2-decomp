#include "game/cas/SimBodyPartAddonLowerBody.h"

int SimBodyPartAddonLowerBody::GetType() const
{
    return 0x26;
}

int SimBodyPartAddonLowerBody::GetBodyPartIndex() const
{
    return 0xc;
}

int SimBodyPartAddonLowerBody::GetBodyPartDBIndex() const
{
    return 0xc;
}

int SimBodyPartAddonLowerBody::GetTattooLocation() const
{
    return 4;
}

int SimBodyPartAddonLowerBody::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartAddonLowerBody::GetLayoutMapRect() const
{
    return TRect<u16>(0x40, 0x40, 0x80, 0x60);
}

int SimBodyPartAddonLowerBody::GetImageModelSetType() const
{
    return 0xe;
}

s32 SimBodyPartAddonLowerBody::GetIsDamaged()
{
    return 0;
}

BitFlags<u32> SimBodyPartAddonLowerBody::ApplyDamage(const Weather::DamageEvent &event, s16 a)
{
    return BitFlags<u32>(0);
}
