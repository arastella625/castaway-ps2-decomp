#ifndef GAME_SIMULATOR_CORE_OBJECTMODULE_H
#define GAME_SIMULATOR_CORE_OBJECTMODULE_H

#include "common/eastl.h"
#include "common/types.h"

class cXObject;
class cXPerson;
class RoutingSlot;

// Wii: ObjectModule. Holds every object in the world. Only what the decompiled code calls is
// declared so far.
class ObjectModule {
public:
    u32 pad_0x0[5];            // 0x0000
    int mRelationsDirty;       // 0x0014
    u32 pad_0x18[4];           // 0x0018
    cXObject *mFirst;          // 0x0028  the object list, linked through cXObject::mNext
    u32 pad_0x2c;              // 0x002c
    s16 mTileObjects[0x40][0x40]; // 0x0030
    u32 pad_0x2030[8];         // 0x2030
    eastl::vector<cXPerson *, eastl::allocator> mPeople; // 0x2050
    u32 pad_0x2060[4];         // 0x2060
    RoutingSlot *mGlobalRoutingSlots;    // 0x2070
    RoutingSlot *mGlobalRoutingSlotsEnd; // 0x2074
    u32 pad_0x2078[2];         // 0x2078
    cXObject *mTutorialObject; // 0x2080
    u32 pad_0x2084;            // 0x2084
    eastl::vector<cXObject *, eastl::allocator> mObjects; // 0x2088
    cXObject *mSpecialObjects[0x10]; // 0x2098  numbered from 0x4000

    cXObject *GetObjectFromID(int id);

    // The game has this as a function and also inlines it in TryAnimate.
    cXObject *GetObjectFromIDInline(int id)
    {
        if (id <= 0 || (int)mObjects.size() < id)
            return 0;
        return mObjects[id - 1];
    }
    void ClearIdleStatus(int id);
    void AddToKillQueue(s16 id, bool cleanupAll);
    int GetIdleStatus(int id);
    void SetIdleStatus(int id, int status);

    cXObject *GetTutorialObject() { return mTutorialObject; }
    cXObject *GetFirst() { return mFirst; }
    inline int GetNumGlobalRoutingSlots();
    inline RoutingSlot *GetGlobalRoutingSlot(int index);
    int GetNumPeople() { return mPeople.size(); }
    cXPerson *GetPeople(int index) { return mPeople[index]; }

    // The objects in the list by position, then the special ones.
    int GetNumObjects()
    {
        int special = 0;
        for (int i = 0; i < 0x10; i++) {
            if (mSpecialObjects[i])
                special++;
        }
        return mObjects.size() + special;
    }
    cXObject *GetObject(int index)
    {
        if ((unsigned int)(index - 0x4000) < 0x10)
            return mSpecialObjects[index - 0x4000];
        return mObjects[index];
    }

    // SetTutorialObject(0), as the game inlines it where the object is known to hold it.
    bool ClearTutorialObject()
    {
        if (mTutorialObject)
            mTutorialObject = 0;
        asm volatile("");
        return true;
    }

    // The tutorial belongs to one object at a time, and 0 releases it.
    bool SetTutorialObject(cXObject *object)
    {
        if (mTutorialObject && object)
            return false;
        if (mTutorialObject == object) {
            // Emits nothing. The game has two copies of this return, and without this our
            // compiler merges them. Whatever kept them apart in the original isn't known.
            asm volatile("");
            return true;
        }
        mTutorialObject = object;
        return true;
    }
};

extern ObjectModule *gObjectModule;

inline ObjectModule *GetObjectModule()
{
    return gObjectModule;
}

#endif
