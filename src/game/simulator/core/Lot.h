#ifndef GAME_SIMULATOR_CORE_LOT_H
#define GAME_SIMULATOR_CORE_LOT_H

#include "common/types.h"

// Wii: Room and RoomManager. Only what InterpValue reads.
class Room {
public:
    u8 pad_0x0[0x84];
    f32 mBasicScore; // 0x84  Wii: BasicScore(), inlined here

    int IsOutside();
    f32 GetAmbientLight() const;
    int GetArea();
};

class RoomManager {
public:
    u8 pad_0x0[0x40];
    f32 mOutsideScore; // 0x40

    static RoomManager *sRoomMgr;

    Room *GetRoom(u16 id);
    void RoomScoreChanged(int id);

    f32 GetRoomEnvironmentScore(u16 id)
    {
        Room *room = GetRoom(id);
        if (room) {
            if (room->IsOutside())
                return mOutsideScore;
            return room->mBasicScore;
        }
        return 0.0f;
    }
};

// Wii: cSimulator. Reached through a global on PS2.
class cSimulator {
public:
    u32 pad_0x0[0xe];  // 0x00
    s16 mGlobals[0x30]; // 0x38  scope 6
    u32 pad_0x98;      // 0x98
    int mTicks;        // 0x9c  Wii: GetTicks
    u32 pad_0xa0[2];   // 0xa0
    class PassiveInfluenceSystem *mPassiveInfluence; // 0xa8
    u32 pad_0xac[3];   // 0xac
    int mFunds;        // 0xb8  Wii: GetFunds
    u32 pad_0xbc[4];   // 0xbc
    int mGameTime;     // 0xcc  Wii: GetCurrentGameTime, read as an int

    int GetPlayerCheatsValue();
    void SetFunds(int funds); // 0x1E0E98
    void AddFunds(int amount) { SetFunds(mFunds + amount); }

    bool GetGlobalRef(s16 index, const s16 **ref)
    {
        if (index < 0 || index >= 0x30 || index == 3 || index == 0x2e || index == 0x20 || index == 0xb)
            return false;
        *ref = &mGlobals[index];
        return true;
    }

    s16 GetGlobal(s16 index)
    {
        if (index == 0x2e)
            return 0;
        return mGlobals[index];
    }
};

extern cSimulator *gSimulator;

inline cSimulator *GetSimulator()
{
    return gSimulator;
}

// One lot's entry in LotManager. Wii reads the flags as a 16-bit value.
struct LotEntry {
    u16 mFlags;   // 0x00  bit 0: visited
    u16 pad_0x2[3];
};

// Wii: LotManager. It is made the first time it's asked for.
class LotManager {
public:
    LotEntry mLots[0x1b];  // 0x00
    s16 mHomeTiki;         // 0xd8
    u8 mTransitions[0x48]; // 0xda  UnlockEverything writes all 0x48
    u8 pad_0x122[2];
    void *mGraph;          // 0x124  the lot graph CanSimGetToLot searches

    LotManager();

    static LotManager *s_LotManager;

    static LotManager *Instance()
    {
        if (s_LotManager == 0)
            s_LotManager = new LotManager;
        return s_LotManager;
    }

    // With the mask as an argument the test stays an AND and a compare, not a shift.
    static bool TestFlag(u32 flags, u32 mask) { return (flags & mask) != 0; }

    s16 WhereIsHomeTiki() const { return mHomeTiki; }

    // GenericSimCall 0x17, inlined there: every lot visited, and bits 0 to 2 set on every
    // transition.
    void UnlockEverything()
    {
        for (int i = 0; i < 0x1b; i++)
            mLots[i].mFlags |= 1;
        for (int i = 0; i < 0x48; i++)
            mTransitions[i] |= 7;
    }

    // These three return the flag, not a bool: their callers compare with zero.
    int HasLotBeenVisited(s16 lot) const
    {
        if (lot <= 0 || lot > 0x1b)
            return 0;
        return mLots[lot - 1].mFlags & 1;
    }

    int IsTransitionOpen(int id, bool both) const
    {
        if (id <= 0 || id >= 0x48)
            return 0;
        u8 flags = mTransitions[id - 1];
        if (both) {
            int open = 0;
            if (flags & 1) {
                if (flags & 2)
                    open = 1;
            }
            return open;
        }
        return flags & 1;
    }

    // GenericSimCall 0x40. Partly inlined on PS2: the graph and the target lot having been
    // visited are tested inline, and the search is 0x1FDD30 (its name and arguments are ours).
    bool FindRoute(u8 from, u8 to, s16 id, s16 *steps, float *distance, s16 *a, u8 *path);
    bool CanSimGetToLot(s16 id, s8 from, s8 to, s16 &steps, float &distance, u8 *path)
    {
        s16 a = 0;
        if (mGraph && HasLotBeenVisited(to))
            return FindRoute(from, to, id, &steps, &distance, &a, path);
        return false;
    }

    bool HasTransitionBeenUsed(s16 id) const
    {
        if (id <= 0 || id >= 0x48)
            return false;
        return TestFlag(mTransitions[id - 1], 4);
    }
};

#endif
