#ifndef GAME_SIMULATOR_OBJECTDATA_TREETABLEENTRYQUICKDATA_H
#define GAME_SIMULATOR_OBJECTDATA_TREETABLEENTRYQUICKDATA_H

#include "common/types.h"

class BString2;
class ITreeTableAd;

// Whatever TreeTableEntryQuickData::field_0x4 points to. The name is ours.
struct TreeTableEntryQuickData_0x4 {
    s16 field_0x0; // 0x00
    s16 field_0x2; // 0x02
    u8 pad_0x4[0x4];
    f32 field_0x8; // 0x08
    u16 field_0xc; // 0x0c
    u8 pad_0xe[0x2];
    u32 field_0x10; // 0x10
    s16 field_0x14; // 0x14
    s16 field_0x16; // 0x16
    s16 field_0x18; // 0x18
    s16 field_0x1a; // 0x1a
};

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class TreeTableEntryQuickData {
public:
    u8 pad_0x0[0x4];
    TreeTableEntryQuickData_0x4 *field_0x4; // 0x04
    u8 pad_0x8[0x10];
    s32 field_0x18; // 0x18

    s16 GetIndex() const;
    s16 GetOrderIndex() const;
    s16 GetJoinIndex() const;
    int CountAds() const;
    s16 GetActionTreeID() const;
    s16 GetCheckTreeID() const;
    u16 GetAttenuation() const;
    f32 GetCustomAttenuationValue() const;
    s16 GetAutonomyThreshold() const;
    u32 GetFlags() const;
    void SetOrderIndex(int a);
    void SetAvailableToVisitors(bool a);
    void SetCanJoin(bool a);
    void SetImmediate(bool a);
    void SetAllowConsecutive(bool a);
    void SetAvailableToGhosts(bool a);
    void SetAvailableToAdults(bool a);
    void SetAvailableToCats(bool a);
    void SetAvailableToLargeDogs(bool a);
    void SetAvailableToSmallDogs(bool a);
    void SetAvailableToChimps(bool a);
    void SetAvailableToBoars(bool a);
    void SetAvailableInDemo(bool a);
    void SetAllowOnSelf(bool a);
    void SetJoinIndex(int a);
    void SetDebugOnly(bool a);
    void SetAutoFirstSelect(bool a);
    void SetAvailableToVisitingPlayers(bool a);
    void SetIgnorePrefixTree(bool a);
    void SetMustRun(bool a);
    void SetAllowDynamicAds(bool a);
    int GetAd(int a);
    void SetAd(int a, const ITreeTableAd &b);
    void SetActionTreeID(s16 a);
    void SetCheckTreeID(s16 a);
    void SetCustomAttenuation(f32 a);
    void SetAutonomyThreshold(s16 a);
    void SetName(const BString2 &a);
    void SetUseCustomActivation(bool a);
    void SetActivationSubtile(u8 a, u8 b);
    void SetActivationExtents(u8 a, u8 b, u8 c, u8 d);
    void AddRef();
};

#endif
