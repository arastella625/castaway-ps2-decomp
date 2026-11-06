#ifndef GAME_SIMULATOR_OBJECTDATA_XOBJLANG_H
#define GAME_SIMULATOR_OBJECTDATA_XOBJLANG_H

#include "common/ERQuickdata.h"
#include "common/types.h"
#include "game/simulator/core/Behavior.h"

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

// Wii: ObjectFolder, reached through a global on PS2.
class ObjectFolder {
public:
    u32 pad_0x0[0x1b2];       // 0x000
    ERQuickdata *mDatabase;   // 0x6c8  Wii: GetObjectsDatabase()

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
