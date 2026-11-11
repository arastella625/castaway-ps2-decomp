#ifndef GAME_GAMEDATA_H
#define GAME_GAMEDATA_H

#include "common/types.h"

class BString2;
class ERShader;
class Interaction;
class Neighbor;

class Player {
public:
    Neighbor *GetPrimaryNeighbor();
};

// Wii: ActionMenu, the interaction menu (the Flash movie tsc5_interaction_menu). See
// docs/formats/ui.md.
class ActionMenu {
public:
    u8 pad_0x0[0x1cc];
    int mHarvestCount; // 0x1cc  the movie's IMU_count, shown by imuShowCount

    void SetupHarvestCount(int count);
};

// Wii: H2DTarget, the in-game HUD. Wii reaches it at _gd +0x48, PS2 at +0x54.
class H2DTarget {
public:
    u32 pad_0x0[0x24];
    ActionMenu *mActionMenu; // 0x90  Wii +0xAC
    u32 pad_0x94[0x71];
    int mGameOver;           // 0x258

    // Wii: TriggerGameOver, inlined on PS2.
    void TriggerGameOver() { mGameOver = 1; }
    void RelationshipChangeOccured(int change);
    int CountModelessDialogs();
    void CleanupModelessDialogs();

    void AddAction(const Interaction *action, const BString2 &name, u32 icon, ERShader *shader,
                   u32 insertGUID) const;
};

// Wii: HUDTarget. GetHudTransitionTarget is inlined on PS2 (GenericSimCall 0x22).
class HUDTarget {
public:
    u32 pad_0x0[0x30];
    int mHudTransitionTarget; // 0xc0

    int GetHudTransitionTarget() const { return mHudTransitionTarget; }
};

struct LoadInfo {
    u32 pad_0x0[4];
    int field_0x10; // 0x10  GenericSimCall 9 clears it
};

// Wii: GameData and its instance _gd. Only what the decompiled code uses is declared.
// game/cas/CasUtils.h has its own view of the same object.
class GameData {
public:
    u32 pad_0x0[0x14];
    HUDTarget *mHudTarget;    // 0x50  Wii +0x44
    H2DTarget *mHud;          // 0x54
    u32 pad_0x58[0x11];
    LoadInfo *mLoadInfo;      // 0x9c
    u32 pad_0xa0[2];
    int mInitialSimSelection; // 0xa8
    int mSelectedFamily;      // 0xac  Wii: GetSelectedFamily
    u32 pad_0xb0[5];
    int mBuildBuyMode;        // 0xc4  Wii: IsBuildBuyMode
    Player mPlayer;           // 0xc8

    Player *GetPlayer() { return &mPlayer; }
    int GetSelectedFamily() const { return mSelectedFamily; }
    void SetInitialSimSelection(int id) { mInitialSimSelection = id; }
};
extern GameData _gd;

#endif
