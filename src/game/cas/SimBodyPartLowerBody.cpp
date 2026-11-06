#include "game/cas/SimBodyPartLowerBody.h"

int SimBodyPartLowerBody::GetType() const
{
    return 0x1f;
}

int SimBodyPartLowerBody::GetBodyPartIndex() const
{
    return 5;
}

int SimBodyPartLowerBody::GetBodyPartDBIndex() const
{
    return 5;
}

int SimBodyPartLowerBody::GetTattooLocation() const
{
    return 2;
}

bool SimBodyPartLowerBody::IsLayered() const
{
    return true;
}

bool SimBodyPartLowerBody::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartLowerBody::GetLayoutMapNumber() const
{
    return 3;
}

TRect<u16> SimBodyPartLowerBody::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0, 0x80, 0x80);
}

int SimBodyPartLowerBody::GetImageModelSetType() const
{
    return 7;
}

u8 SimBodyPartLowerBody::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return styleIndex;
}

u8 SimBodyPartLowerBody::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex;
}
