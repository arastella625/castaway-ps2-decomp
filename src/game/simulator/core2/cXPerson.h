#ifndef GAME_SIMULATOR_CORE2_CXPERSON_H
#define GAME_SIMULATOR_CORE2_CXPERSON_H

#include "common/types.h"
#include "game/simulator/core/Interaction.h"
#include "game/simulator/core/ObjSelector.h"
#include "game/simulator/core/cXObject.h"

class Neighbor;

// Wii: ActionQueue, a Queue<Interaction, 12>: a ring of twelve actions.
class ActionQueue {
public:
    Interaction mActions[12]; // 0x000
    u32 mHead;                // 0x300
    u32 mTail;                // 0x304

    bool Peek(u32 index, Interaction &action) const
    {
        if (index >= mTail - mHead)
            return false;
        action = mActions[(mHead + index) % 12];
        return true;
    }
};

// A Sim. The fields are the ones InterpValue reads.
class cXPerson : public cXObject {
public:
    u32 pad_0x13c[0x11];      // 0x13c
    f32 mMotives[16];         // 0x180  scope 0xe, read through GetMotive
    u32 pad_0x1c0[0x17];      // 0x1c0
    ActionQueue mActionQueue; // 0x21c
    u32 pad_0x524[9];         // 0x524
    s16 mPersonData[0x24];    // 0x548  scopes 0x12, 0x13 and 0x30 to 0x33
    Neighbor *mNeighbor;      // 0x590

    static int sbAllowingCheckTreeAds;


    u32 ReconType();

    s16 *GetPersonDataArray() { return mPersonData; }
    Neighbor *GetNeighbor() const { return mNeighbor; }
    void *GetSimDescription() { return mSelector->mSimDescription; }
    f32 GetMotive(int index);
    f32 *GetMotiveRef(int index);
    static bool isValidPersonDataField(s16 index);
    static int TranslasteToNewPersonDataIndex(int index, bool toNew);
};

#endif
