#include "game/simulator/core/TokenItem.h"

s16 TokenItem::GetID() const
{
    return field_0x8;
}

s16 TokenItem::GetCount() const
{
    return field_0xa;
}

s8 TokenItem::GetCategory() const
{
    return 2;
}

s16 TokenItem::GetOwnerID() const
{
    return -1;
}

int TokenItem::GetOwnershipPoints(s16 a) const
{
    return -1;
}

int TokenItem::GetNumAttributes() const
{
    return -1;
}

s16 TokenItem::GetAttribute(int a)
{
    return 0;
}

void TokenItem::SetAttribute(int a, s16 b)
{
}

void TokenItem::AddAtribute(s16 a)
{
}

s8 TokenItem::GetStatusText()
{
    return -1;
}

void TokenItem::SetStatusText(s8 a)
{
}

const InventoryTokens *TokenItem::GetToken()
{
    return mToken;
}

void TokenItem::LoadItemInfo(cXObject *a, Family *b, s16 c)
{
}

bool TokenItem::IsClothing()
{
    return false;
}

bool TokenItem::IsDyable()
{
    return false;
}

void TokenItem::ClearAttributes()
{
}

void TokenItem::SetupOwnershipInfo(cXObject *a)
{
}
