#ifndef GAME_SIMULATOR_CORE_NEIGHBORHOOD_H
#define GAME_SIMULATOR_CORE_NEIGHBORHOOD_H

#include "common/eastl.h"
#include "common/types.h"
#include "game/simulator/core/Family.h"

class ObjSelector;

// Wii: Neighbor. Only what the decompiled code reads is declared so far.
class Neighbor {
public:
    u32 pad_0x0;            // 0x00
    s16 mID;                // 0x04
    int field_0x8;          // 0x08  the argument of Family's membership test
    ObjSelector *mSelector; // 0x0c
    class RelMatrix *mRelations; // 0x10
    u32 pad_0x14;           // 0x14
    class cXPerson *mPerson; // 0x18  Wii: GetPerson
    u32 pad_0x1c;           // 0x1c
    int mDirty;             // 0x20
    u8 pad_0x24[0x48];      // 0x24
    s16 mPersonData[0x31];  // 0x6c  scopes 0x34 to 0x39

    Family *GetFamily();
    s16 CountRelationshipsOfType(int type, bool a) const;
    s16 *GetPersonDataArray() { return mPersonData; }
    s16 GetID() const { return mID; }

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
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Unlock(int kind, int index);
    virtual s16 GetStatus(int kind, int index);
    virtual void SetStatus(int kind, int index, int status);
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual s16 TryEvaluateGoals(int book); // Wii GoalUnlock slot 15; GenericSimCall 0x42
    virtual void Slot16();
    virtual void Slot17();
    virtual void Slot18();
    virtual void Slot19();
    virtual void Slot20(); // GenericSimCall 7
};

// Wii: Neighborhood. Its vptr comes after the fields, at 0x21ec.
class Neighborhood {
public:
    eastl::vector<Neighbor *, eastl::allocator> mNeighbors; // 0x00
    s16 mVars[0x24];           // 0x10  scope 0x22
    u32 pad_0x58[0x12];        // 0x58
    eastl::vector<int, eastl::allocator> mHouses; // 0xa0  lot numbers
    eastl::vector<Family *, eastl::allocator> mFamilies; // 0xb0
    u32 pad_0xc0;              // 0xc0
    GoalAndUnlockData mGoals;  // 0xc4
    u32 pad_0xc8[0x849];       // 0xc8

    virtual void Slot0();
    virtual s16 GetStackObjectValue(int id, s16 data, const s16 **ref);


    GoalAndUnlockData *GetGoalAndUnlockData() { return &mGoals; }
    Neighbor *FindNeighborByID(int id);
    int RemoveFromFamily(Neighbor *neighbor, bool a); // GenericSimCall 6: 0 when it worked
    int AddToFamily(Neighbor *neighbor, Family *family); // 0x2055D8, 0 when it worked
    Family *MakeNewFamily(bool freePlay, int number);    // 0x205250, our name
    // Wii: MakeNewFreePlayFamily(int), inlined on PS2 (GenericSimCall 4 and 0x37). Defined after
    // GetFamilyInline's body, or GCC 2.95 calls an out-of-line copy of that instead of inlining.
    inline Family *MakeNewFreePlayFamily(int number);
    void SetNeighborhoodVar(int index, s16 value) { mVars[index] = value; }
    int GetNumFamilies();
    Family *GetFamilyByIndex(int index);

    int *GetFamily(s16 id);
    Family *GetFamilyInline(int number); // Wii GetFamily(int), inlined in GenericSimCall 5
    void SavePersistentData(class cXPerson *person);
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
    // TryRelationship2's form of the lookup: two separate tests line up better there than one.
    Neighbor *FindNeighborByIDJoined(int id)
    {
        if (id <= 0)
            return 0;
        if (mNeighbors.size() < (unsigned int)id)
            return 0;
        Neighbor *result = mNeighbors[id - 1];
        return result;
    }
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
    static class House *pHouse;
};

class House {
public:
    void ComputeAndStoreLotData();
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
