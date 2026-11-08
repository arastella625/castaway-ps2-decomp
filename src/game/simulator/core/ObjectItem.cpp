#include "game/simulator/core/ObjectItem.h"

s16 ObjectItem::GetID() const
{
    return 0;
}

u32 ObjectItem::GetGuid() const
{
    return field_0x8;
}

int ObjectItem::GetCategory() const
{
    return 0;
}

s16 ObjectItem::GetCount() const
{
    return 1;
}

int ObjectItem::GetOwnerID() const
{
    return -1;
}

int ObjectItem::GetOwnershipPoints(s16 a) const
{
    return -1;
}

s8 ObjectItem::GetStatusText()
{
    return field_0xc;
}

void ObjectItem::SetStatusText(s8 value)
{
    field_0xc = value;
}

void ObjectItem::IncrementCount(s16 a)
{
}

void ObjectItem::DecrementCount(s16 a)
{
}

const InventoryTokens *ObjectItem::GetToken()
{
    return 0;
}

int ObjectItem::GetIngredientToken()
{
    return 0;
}

bool ObjectItem::IsFood()
{
    return false;
}

bool ObjectItem::IsFastFood()
{
    return false;
}

bool ObjectItem::IsTool()
{
    return false;
}

bool ObjectItem::IsFastTool()
{
    return false;
}

bool ObjectItem::IsResource()
{
    return false;
}

bool ObjectItem::CanBePlanted()
{
    return false;
}

bool ObjectItem::IsHandObject()
{
    return false;
}

bool ObjectItem::IsClothing()
{
    return false;
}

bool ObjectItem::IsDye()
{
    return false;
}

bool ObjectItem::IsDyable()
{
    return false;
}

void ObjectItem::SetupOwnershipInfo(cXObject *a)
{
}
