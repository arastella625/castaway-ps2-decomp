#ifndef GAME_SIMULATOR_OBJECTDATA_XOBJLANG_H
#define GAME_SIMULATOR_OBJECTDATA_XOBJLANG_H

#include "common/ERQuickdata.h"
#include "common/types.h"
#include "game/simulator/core/Behavior.h"
#include "game/simulator/core/ObjSelector.h"

// A tree constant table. Wii: ObjectDataBehaviorConstants; slots run in Wii vtable order.
class ObjectDataBehaviorConstants : public IConsoleUnknown {
public:
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void SetValueCount(int count);
    virtual s16 GetValue(int index) const;

    int CountValues() const;
};

// Wii: NamespaceSelector. What a namespace ID resolves to.
class NamespaceSelector {
public:
    ObjectDataBehaviorConstants *GetBehaviorConstants(s16 id);
};

// Wii: HashList<ObjSelector, int, 256>. A find moves what it finds to the front of its bucket.
class SelectorHashList {
public:
    ObjSelector *mBuckets[256];

    ObjSelector *findItem(const int &key)
    {
        unsigned int index = key & 0xff;
        ObjSelector *selector = mBuckets[index];
        ObjSelector *previous = 0;
        while (selector) {
            if (selector->compare(key)) {
                if (previous) {
                    previous->mNext = selector->mNext;
                    selector->mNext = mBuckets[index];
                    mBuckets[index] = selector;
                }
                break;
            }
            previous = selector;
            selector = selector->mNext;
        }
        return selector;
    }
};

// Wii: ObjectFolder, reached through a global on PS2.
class ObjectFolder {
public:
    u32 pad_0x0[0x15];          // 0x000
    SelectorHashList mSelectors; // 0x054
    u32 pad_0x454[0x9d];        // 0x454
    ERQuickdata *mDatabase;     // 0x6c8  Wii: GetObjectsDatabase()

    ObjSelector *GetSelectorByGUID(int guid) { return mSelectors.findItem(guid); }

    NamespaceSelector *GetNamespaceSelector(u32 namespaceID);
};

extern ObjectFolder *gObjectFolder;

inline ObjectFolder *GetObjectFolder()
{
    return gObjectFolder;
}

inline ObjectDataBehaviorConstants *Behavior::GetConstants(s16 id, ObjectDataBehaviorConstants **constants)
{
    u32 namespaceID = GetResFileID(id);
    *constants = GetObjectFolder()->GetNamespaceSelector(namespaceID)->GetBehaviorConstants(id);
    if (*constants)
        (*constants)->AddRef();
    return *constants;
}

// Wii: GetConstantsID picks the table and index out of a packed operand.
class XObjLang {
public:
    static int GetConstantsID(s16 packed, s16 *tableID, int *index);
};

#endif
