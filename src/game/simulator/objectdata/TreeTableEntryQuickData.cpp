#include "game/simulator/objectdata/TreeTableEntryQuickData.h"

s16 TreeTableEntryQuickData::GetIndex() const
{
    return field_0x4->field_0x14;
}

s16 TreeTableEntryQuickData::GetOrderIndex() const
{
    return field_0x4->field_0x1a;
}

s16 TreeTableEntryQuickData::GetJoinIndex() const
{
    return field_0x4->field_0x18;
}

int TreeTableEntryQuickData::CountAds() const
{
    return 0x10;
}

s16 TreeTableEntryQuickData::GetActionTreeID() const
{
    return field_0x4->field_0x2;
}

s16 TreeTableEntryQuickData::GetCheckTreeID() const
{
    return field_0x4->field_0x0;
}

u16 TreeTableEntryQuickData::GetAttenuation() const
{
    return field_0x4->field_0xc;
}

f32 TreeTableEntryQuickData::GetCustomAttenuationValue() const
{
    return field_0x4->field_0x8;
}

s16 TreeTableEntryQuickData::GetAutonomyThreshold() const
{
    return field_0x4->field_0x16;
}

u32 TreeTableEntryQuickData::GetFlags() const
{
    return field_0x4->field_0x10;
}

void TreeTableEntryQuickData::SetOrderIndex(int a)
{
}

void TreeTableEntryQuickData::SetAvailableToVisitors(bool a)
{
}

void TreeTableEntryQuickData::SetCanJoin(bool a)
{
}

void TreeTableEntryQuickData::SetImmediate(bool a)
{
}

void TreeTableEntryQuickData::SetAllowConsecutive(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableToGhosts(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableToAdults(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableToCats(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableToLargeDogs(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableToSmallDogs(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableToChimps(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableToBoars(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableInDemo(bool a)
{
}

void TreeTableEntryQuickData::SetAllowOnSelf(bool a)
{
}

void TreeTableEntryQuickData::SetJoinIndex(int a)
{
}

void TreeTableEntryQuickData::SetDebugOnly(bool a)
{
}

void TreeTableEntryQuickData::SetAutoFirstSelect(bool a)
{
}

void TreeTableEntryQuickData::SetAvailableToVisitingPlayers(bool a)
{
}

void TreeTableEntryQuickData::SetIgnorePrefixTree(bool a)
{
}

void TreeTableEntryQuickData::SetMustRun(bool a)
{
}

void TreeTableEntryQuickData::SetAllowDynamicAds(bool a)
{
}

int TreeTableEntryQuickData::GetAd(int a)
{
    return 0;
}

void TreeTableEntryQuickData::SetAd(int a, const ITreeTableAd &b)
{
}

void TreeTableEntryQuickData::SetActionTreeID(s16 a)
{
}

void TreeTableEntryQuickData::SetCheckTreeID(s16 a)
{
}

void TreeTableEntryQuickData::SetCustomAttenuation(f32 a)
{
}

void TreeTableEntryQuickData::SetAutonomyThreshold(s16 a)
{
}

void TreeTableEntryQuickData::SetName(const BString2 &a)
{
}

void TreeTableEntryQuickData::SetUseCustomActivation(bool a)
{
}

void TreeTableEntryQuickData::SetActivationSubtile(u8 a, u8 b)
{
}

void TreeTableEntryQuickData::SetActivationExtents(u8 a, u8 b, u8 c, u8 d)
{
}

void TreeTableEntryQuickData::AddRef()
{
    field_0x18++;
}
