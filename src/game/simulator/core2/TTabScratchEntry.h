#ifndef GAME_SIMULATOR_CORE2_TTABSCRATCHENTRY_H
#define GAME_SIMULATOR_CORE2_TTABSCRATCHENTRY_H

#include "common/types.h"

class BString2;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class TTabScratchEntry {
public:
    u8 pad_0x0[0x4];
    s16 field_0x4; // 0x04
    s16 field_0x6; // 0x06
    u8 pad_0x8[0x100];
    u32 field_0x108; // 0x108
    f32 field_0x10c; // 0x10c
    u16 field_0x110; // 0x110
    s16 field_0x112; // 0x112
    s16 field_0x114; // 0x114
    s16 field_0x116; // 0x116
    s32 field_0x118; // 0x118

    s16 GetIndex() const;
    int GetAvailableToVisitors() const;
    int GetCanJoin() const;
    int GetImmediate() const;
    int GetAllowConsecutive() const;
    int GetAvailableToGhosts() const;
    int GetAvailableToCats() const;
    int GetAvailableToLargeDogs() const;
    int GetAvailableToSmallDogs() const;
    int GetAvailableToChimps() const;
    void SetAvailableToChimps(bool a);
    int GetAvailableToBoars() const;
    void SetAvailableToBoars(bool a);
    int GetAvailableInDemo() const;
    int GetAllowOnSelf() const;
    void SetAllowOnSelf(bool a);
    s16 GetJoinIndex() const;
    int GetDebugOnly() const;
    int GetAutoFirstSelect() const;
    int GetAvailableToVisitingPlayers() const;
    int GetIgnorePrefixTree() const;
    int GetMustRun() const;
    int GetAllowDynamicAds() const;
    void SetAllowDynamicAds(bool a);
    int CountAds() const;
    s16 GetActionTreeID() const;
    void SetActionTreeID(s16 value);
    s16 GetCheckTreeID() const;
    void SetCheckTreeID(s16 value);
    u32 GetAttenuation() const;
    f32 GetCustomAttenuationValue() const;
    s16 GetAutonomyThreshold() const;
    void SetAutonomyThreshold(s16 value);
    void SetName(const BString2 &a);
    int GetOrderIndex() const;
    void SetOrderIndex(int a);
    int GetUseCustomActivation() const;
    void SetUseCustomActivation(bool a);
    void GetActivationSubtile(u8 &a, u8 &b) const;
    void SetActivationSubtile(u8 a, u8 b);
    void GetActivationExtents(u8 &a, u8 &b, u8 &c, u8 &d) const;
    void SetActivationExtents(u8 a, u8 b, u8 c, u8 d);
    void AddRef();
};

#endif
