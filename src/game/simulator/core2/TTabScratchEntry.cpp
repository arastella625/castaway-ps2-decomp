#include "game/simulator/core2/TTabScratchEntry.h"

s16 TTabScratchEntry::GetIndex() const
{
    return field_0x112;
}

void TTabScratchEntry::SetAvailableToChimps(bool a)
{
}

void TTabScratchEntry::SetAvailableToBoars(bool a)
{
}

void TTabScratchEntry::SetAllowOnSelf(bool a)
{
}

s16 TTabScratchEntry::GetJoinIndex() const
{
    return field_0x116;
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

s16 TTabScratchEntry::GetFlags() const
{
    return field_0x110;
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
