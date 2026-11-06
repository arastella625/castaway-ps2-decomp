#include "game/simulator/objectdata/TreeTableAdQuickData.h"

s16 TreeTableAdQuickData::GetPersonalityAd() const
{
    return field_0x4->field_0x0;
}

s16 TreeTableAdQuickData::GetMin() const
{
    return field_0x4->field_0x2;
}

s16 TreeTableAdQuickData::GetRange() const
{
    return field_0x4->field_0x4;
}

void TreeTableAdQuickData::SetPersonalityAd(int a)
{
}

void TreeTableAdQuickData::SetMin(int a)
{
}

void TreeTableAdQuickData::SetMax(int a)
{
}

void TreeTableAdQuickData::SetRange(int a)
{
}

void TreeTableAdQuickData::AddRef()
{
    field_0x8++;
}
