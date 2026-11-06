#include "game/simulator/core2/FamilyImpl.h"

s32 FamilyImpl::GetNumber()
{
    return field_0x8;
}

void FamilyImpl::SetNumber(int value)
{
    field_0x8 = value;
}

s32 FamilyImpl::GetHouseNumber()
{
    return field_0xc;
}

void FamilyImpl::SetHouseNumber(int value)
{
    field_0xc = value;
}

s32 FamilyImpl::GetCreationOrder()
{
    return field_0x10;
}

void FamilyImpl::SetCreationOrder(int value)
{
    field_0x10 = value;
}

u32 FamilyImpl::GetFunds()
{
    return field_0x14;
}

u32 FamilyImpl::GetPetPoints()
{
    return field_0x18;
}

s32 FamilyImpl::GetHouseValue()
{
    return field_0x1c;
}

void FamilyImpl::SetHouseValue(int value)
{
    field_0x1c = value;
}

int FamilyImpl::GetStatValue(int a) const
{
    return 0;
}

int FamilyImpl::GetStatRef(int a, const s16 * *b)
{
    return 0;
}

bool FamilyImpl::ShouldSerializeMotives()
{
    return false;
}

int FamilyImpl::MyDoCommand(s16 a, int b)
{
    return 0;
}
