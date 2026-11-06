#include "game/simulator/core2/TTabScratchEntry.h"

s16 TTabScratchEntry::GetIndex() const
{
    return field_0x112;
}

int TTabScratchEntry::GetAvailableToVisitors() const
{
    return field_0x110 & 1;
}

int TTabScratchEntry::GetCanJoin() const
{
    return (field_0x110 >> 1) & 1;
}

int TTabScratchEntry::GetImmediate() const
{
    return (field_0x110 >> 2) & 1;
}

int TTabScratchEntry::GetAllowConsecutive() const
{
    return (field_0x110 >> 3) & 1;
}

int TTabScratchEntry::GetAvailableToGhosts() const
{
    return (field_0x110 >> 4) & 1;
}

int TTabScratchEntry::GetAvailableToCats() const
{
    return (field_0x110 >> 13) & 1;
}

int TTabScratchEntry::GetAvailableToLargeDogs() const
{
    return (field_0x110 >> 5) & 1;
}

int TTabScratchEntry::GetAvailableToSmallDogs() const
{
    return (field_0x110 >> 14) & 1;
}

int TTabScratchEntry::GetAvailableToChimps() const
{
    return field_0x110 >> 15;
}

void TTabScratchEntry::SetAvailableToChimps(bool a)
{
}

int TTabScratchEntry::GetAvailableToBoars() const
{
    return field_0x110 >> 15;
}

void TTabScratchEntry::SetAvailableToBoars(bool a)
{
}

int TTabScratchEntry::GetAvailableInDemo() const
{
    return field_0x110 >> 15;
}

int TTabScratchEntry::GetAllowOnSelf() const
{
    return field_0x110 >> 15;
}

void TTabScratchEntry::SetAllowOnSelf(bool a)
{
}

s16 TTabScratchEntry::GetJoinIndex() const
{
    return field_0x116;
}

int TTabScratchEntry::GetDebugOnly() const
{
    return (field_0x110 >> 7) & 1;
}

int TTabScratchEntry::GetAutoFirstSelect() const
{
    return (field_0x110 >> 8) & 1;
}

int TTabScratchEntry::GetAvailableToVisitingPlayers() const
{
    return (field_0x110 >> 9) & 1;
}

int TTabScratchEntry::GetIgnorePrefixTree() const
{
    return (field_0x110 >> 10) & 1;
}

int TTabScratchEntry::GetMustRun() const
{
    return (field_0x110 >> 11) & 1;
}

int TTabScratchEntry::GetAllowDynamicAds() const
{
    return field_0x110 >> 15;
}

void TTabScratchEntry::SetAllowDynamicAds(bool a)
{
}

int TTabScratchEntry::CountAds() const
{
    return 0x10;
}

s16 TTabScratchEntry::GetActionTreeID() const
{
    return field_0x6;
}

void TTabScratchEntry::SetActionTreeID(s16 value)
{
    field_0x6 = value;
}

s16 TTabScratchEntry::GetCheckTreeID() const
{
    return field_0x4;
}

void TTabScratchEntry::SetCheckTreeID(s16 value)
{
    field_0x4 = value;
}

u32 TTabScratchEntry::GetAttenuation() const
{
    return field_0x108;
}

f32 TTabScratchEntry::GetCustomAttenuationValue() const
{
    return field_0x10c;
}

s16 TTabScratchEntry::GetAutonomyThreshold() const
{
    return field_0x114;
}

void TTabScratchEntry::SetAutonomyThreshold(s16 value)
{
    field_0x114 = value;
}

void TTabScratchEntry::SetName(const BString2 &a)
{
}

int TTabScratchEntry::GetOrderIndex() const
{
    return 0;
}

void TTabScratchEntry::SetOrderIndex(int a)
{
}

int TTabScratchEntry::GetUseCustomActivation() const
{
    return 0;
}

void TTabScratchEntry::SetUseCustomActivation(bool a)
{
}

void TTabScratchEntry::GetActivationSubtile(u8 &a, u8 &b) const
{
}

void TTabScratchEntry::SetActivationSubtile(u8 a, u8 b)
{
}

void TTabScratchEntry::GetActivationExtents(u8 &a, u8 &b, u8 &c, u8 &d) const
{
}

void TTabScratchEntry::SetActivationExtents(u8 a, u8 b, u8 c, u8 d)
{
}

void TTabScratchEntry::AddRef()
{
    field_0x118++;
}
