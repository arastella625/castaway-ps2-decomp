#ifndef GAME_SIMULATOR_CORE_OBJECTMODULE_H
#define GAME_SIMULATOR_CORE_OBJECTMODULE_H

#include "common/types.h"

class cXObject;

// Wii: ObjectModule. Holds every object in the world. Only what the decompiled code calls is
// declared so far.
class ObjectModule {
public:
    u32 pad_0x0[0x820];        // 0x0000
    cXObject *mTutorialObject; // 0x2080

    cXObject *GetObjectFromID(int id);
    void ClearIdleStatus(int id);

    cXObject *GetTutorialObject() { return mTutorialObject; }

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

#endif
