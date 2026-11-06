#include "game/simulator/core/ClothingItem.h"

s16 ClothingItem::GetID() const
{
    return field_0xe;
}

int ClothingItem::GetCount() const
{
    return 1;
}

int ClothingItem::GetGuid() const
{
    return 0;
}

int ClothingItem::GetCategory() const
{
    return 3;
}

s16 ClothingItem::GetOwnerID() const
{
    return field_0xc;
}

int ClothingItem::GetOwnershipPoints(s16 a) const
{
    return -1;
}

int ClothingItem::GetNumAttributes() const
{
    return -1;
}

int ClothingItem::GetAttribute(int a)
{
    return 0;
}

void ClothingItem::SetAttribute(int a, s16 b)
{
}

void ClothingItem::AddAtribute(s16 a)
{
}

int ClothingItem::GetStatusText()
{
    return -1;
}

void ClothingItem::SetStatusText(s8 a)
{
}

void ClothingItem::IncrementCount(s16 a)
{
}

void ClothingItem::DecrementCount(s16 a)
{
}

int ClothingItem::GetToken()
{
    return 0;
}

int ClothingItem::GetIngredientToken()
{
    return 0;
}

void ClothingItem::LoadItemInfo(cXObject *a, Family *b, s16 c)
{
}

int ClothingItem::MakeObject(Family *a, s16 b)
{
    return 0;
}

bool ClothingItem::IsFood()
{
    return false;
}

bool ClothingItem::IsFastFood()
{
    return false;
}

bool ClothingItem::IsTool()
{
    return false;
}

bool ClothingItem::IsFastTool()
{
    return false;
}

bool ClothingItem::IsResource()
{
    return false;
}

bool ClothingItem::CanBePlanted()
{
    return false;
}

bool ClothingItem::IsSeed()
{
    return false;
}

bool ClothingItem::CanBecomeSeed()
{
    return false;
}

bool ClothingItem::IsHandObject()
{
    return false;
}

bool ClothingItem::IsClothing()
{
    return true;
}

bool ClothingItem::IsDye()
{
    return false;
}

void ClothingItem::ClearAttributes()
{
}

void ClothingItem::SetupOwnershipInfo(cXObject *a)
{
}
