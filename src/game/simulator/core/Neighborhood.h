#ifndef GAME_SIMULATOR_CORE_NEIGHBORHOOD_H
#define GAME_SIMULATOR_CORE_NEIGHBORHOOD_H

#include "common/eastl.h"
#include "common/types.h"
#include "game/simulator/core/Family.h"

class ObjSelector;

// Wii: Neighbor. Only what the decompiled code reads is declared so far.
class Neighbor {
public:
    u8 pad_0x0[8];
    int field_0x8;          // 0x08  the argument of Family's membership test
    ObjSelector *mSelector; // 0x0c
    u8 pad_0x10[0x5c];      // 0x10
    s16 mPersonData[0x31];  // 0x6c  scopes 0x34 to 0x39

    Family *GetFamily();
    s16 CountRelationshipsOfType(int type, bool a) const;
    s16 *GetPersonDataArray() { return mPersonData; }

    // The game has this as a function (0x250DE8) and also inlines it in InterpValue. An inline
    // function's copy lands in a linkonce section, so the inline body has its own name.
    inline Family *GetFamilyInline();

    // Wii: TranslasteToNewNeighborPersonDataIndex. -1 is a field that doesn't exist, and the
    // second argument is ignored on PS2, where this is one table lookup.
    static int TranslateIndex(int index, bool toNew);
    static s8 sIndexTable[];
    static bool IsValidField(s16 index) { return TranslateIndex(index, false) != -1; }
};

// Wii: Neighborhood::GetGoalAndUnlockData returns this. It starts with a vptr.
class GoalAndUnlockData {
public:
    virtual void Slot0();
    virtual int Count(int kind);
    virtual void Slot2();
    virtual void Slot3();
    virtual int Get(int kind, int index);
};

// Wii: Neighborhood. Its vptr comes after the fields, at 0x21ec.
class Neighborhood {
public:
    eastl::vector<Neighbor *, eastl::allocator> mNeighbors; // 0x00
    s16 mVars[0x24];           // 0x10  scope 0x22
    u32 pad_0x58[0x12];        // 0x58
    eastl::vector<int, eastl::allocator> mHouses; // 0xa0  lot numbers
    u32 pad_0xb0[5];           // 0xb0
    GoalAndUnlockData mGoals;  // 0xc4
    u32 pad_0xc8[0x849];       // 0xc8

    virtual void Slot0();
    virtual s16 GetStackObjectValue(int id, s16 data, const s16 **ref);


    Neighbor *FindNeighborByID(int id);
    int GetNumFamilies();
    Family *GetFamilyByIndex(int index);

    int GetNumNeighborHouses() { return mHouses.size(); }
    int GetNeighborHouseByIndex(int index)
    {
        if ((unsigned int)index >= mHouses.size())
            index = mHouses.size() - 1;
        return mHouses[index];
    }

    // The ID of the first neighbour after this one, or 0.
    s16 GetNextNeighborID(s16 id)
    {
        while ((unsigned int)id < mNeighbors.size()) {
            id++;
            if (mNeighbors[id - 1])
                return id;
        }
        return 0;
    }

    // The game has this as a function and also inlines it in InterpValue.
    Neighbor *FindNeighborByIDInline(int id)
    {
        if (id <= 0 || mNeighbors.size() < (unsigned int)id)
            return 0;
        return mNeighbors[id - 1];
    }
};

// Wii: pNeighborhood__5Globs, read through GetNeighborhood__Fv, which the PS2 build inlines.
class Globs {
public:
    enum TransitionType { };
    static Neighborhood *pNeighborhood;
};

inline Neighborhood *GetNeighborhood()
{
    return Globs::pNeighborhood;
}

inline Family *Neighbor::GetFamilyInline()
{
    int count = GetNeighborhood()->GetNumFamilies();
    for (int i = 0; i < count; i++) {
        Family *family = GetNeighborhood()->GetFamilyByIndex(i);
        if (family->slot12(field_0x8))
            return family;
    }
    return 0;
}

#endif
