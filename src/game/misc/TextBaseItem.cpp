#include "game/misc/TextBaseItem.h"

void TextBaseItem::Reset()
{
    field_0x4 = 1;
}

u32 TextBaseItem::GetColor()
{
    return field_0x44;
}

void TextBaseItem::SetShadowAlpha(f32 value)
{
    field_0x48 = value;
}

f32 TextBaseItem::GetShadowAlpha()
{
    return field_0x48;
}

void TextBaseItem::SetShadowSize(f32 value)
{
    field_0x4c = value;
}

f32 TextBaseItem::GetShadowSize()
{
    return field_0x4c;
}

void TextBaseItem::SetPosX(f32 value)
{
    field_0x24 = value;
}

f32 TextBaseItem::GetPosX()
{
    return field_0x24;
}

void TextBaseItem::SetPosY(f32 value)
{
    field_0x28 = value;
}

f32 TextBaseItem::GetPosY()
{
    return field_0x28;
}
