#include "game/cas/SimBodyPartHair.h"

int SimBodyPartHair::GetType() const
{
    return 0x1c;
}

int SimBodyPartHair::GetBodyPartIndex() const
{
    return 2;
}

int SimBodyPartHair::GetBodyPartDBIndex() const
{
    return 2;
}

int SimBodyPartHair::GetTattooLocation() const
{
    return 4;
}

bool SimBodyPartHair::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartHair::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartHair::GetLayoutMapRect() const
{
    return TRect<u16>(0x40, 0, 0x80, 0x40);
}

int SimBodyPartHair::GetImageModelSetType() const
{
    return 4;
}

u32 SimBodyPartHair::GetReflectionMaskID(bool a) const
{
    return 0;
}

s32 SimBodyPartHair::GetIsDamaged()
{
    return isDamaged;
}

void SimBodyPartHair::SetIsDamaged(bool damaged)
{
    isDamaged = damaged;
}

s32 SimBodyPartHair::GetCurrentHitPoints()
{
    return currentHitPoints;
}

void SimBodyPartHair::SetCurrentHitPoints(int hitPoints)
{
    currentHitPoints = hitPoints;
}

u8 SimBodyPartHair::GetDegradationModelCodeIndex() const
{
    return 0;
}
