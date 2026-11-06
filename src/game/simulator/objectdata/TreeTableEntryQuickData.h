#ifndef GAME_SIMULATOR_OBJECTDATA_TREETABLEENTRYQUICKDATA_H
#define GAME_SIMULATOR_OBJECTDATA_TREETABLEENTRYQUICKDATA_H

#include "common/types.h"

class BString2;
class ITreeTableAd;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class TreeTableEntryQuickData {
public:
    int CountAds() const;
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
};

#endif
