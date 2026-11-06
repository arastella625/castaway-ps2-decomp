#include "game/cas/SimBodyPartShoes.h"

int SimBodyPartShoes::GetType() const
{
    return 0x20;
}

int SimBodyPartShoes::GetBodyPartIndex() const
{
    return 6;
}

int SimBodyPartShoes::GetBodyPartDBIndex() const
{
    return 6;
}

int SimBodyPartShoes::GetTattooLocation() const
{
    return 4;
}

bool SimBodyPartShoes::IsLayered() const
{
    return true;
}

bool SimBodyPartShoes::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartShoes::GetLayoutMapNumber() const
{
    return 0;
}

TRect<u16> SimBodyPartShoes::GetLayoutMapRect() const
{
    return TRect<u16>(0x40, 0, 0x80, 0x40);
}

int SimBodyPartShoes::GetImageModelSetType() const
{
    return 8;
}

u8 SimBodyPartShoes::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return styleIndex;
}

u8 SimBodyPartShoes::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex;
}
