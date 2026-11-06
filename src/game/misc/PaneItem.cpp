#include "game/misc/PaneItem.h"

void PaneItem::Shutdown()
{
}

void PaneItem::Reset()
{
    field_0x4 = 1;
}

void PaneItem::CalculateSize()
{
}

void PaneItem::SetPosX(f32 value)
{
    field_0xc = value;
}

f32 PaneItem::GetPosX() const
{
    return field_0xc;
}

void PaneItem::SetPosY(f32 value)
{
    field_0x10 = value;
}

f32 PaneItem::GetPosY() const
{
    return field_0x10;
}

void PaneItem::Startup()
{
}

void PaneItem::Draw(ERC *a, f32 b)
{
}
