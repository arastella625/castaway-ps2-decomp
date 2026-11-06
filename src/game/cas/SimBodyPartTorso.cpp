#include "game/cas/SimBodyPartTorso.h"

int SimBodyPartTorso::GetType() const
{
    return 0x1e;
}

int SimBodyPartTorso::GetBodyPartIndex() const
{
    return 4;
}

int SimBodyPartTorso::GetBodyPartDBIndex() const
{
    return 4;
}

int SimBodyPartTorso::GetTattooLocation() const
{
    return 1;
}

bool SimBodyPartTorso::IsLayered() const
{
    return true;
}

bool SimBodyPartTorso::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartTorso::GetLayoutMapNumber() const
{
    return 1;
}

TRect<u16> SimBodyPartTorso::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0, 0x80, 0x80);
}

int SimBodyPartTorso::GetImageModelSetType() const
{
    return 6;
}

u8 SimBodyPartTorso::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return styleIndex;
}

u8 SimBodyPartTorso::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex;
}
