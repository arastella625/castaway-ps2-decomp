#ifndef GAME_SIMULATOR_CORE_OBJSELECTOR_H
#define GAME_SIMULATOR_CORE_OBJSELECTOR_H

#include "common/types.h"

// Wii: the table read through ObjSelector::GetFnTable. Only the slot InterpValue calls is
// declared; the ones before it keep it in place.
class ObjectFunctionTable {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    // The tree ID at that entry point; 0 is none. The index is an int: cXObject::RunTree passes
    // its ObjEntryPoint without narrowing it, which this compiler always does for a short.
    virtual int HasFunction(int index);
};

// An object's type: the namespace IDs that a tree's ID range selects, its definition, and its
// function table. Only the fields the decompiled code reads.
// Wii: the object's TTAB. Only the slot PushAction and Interaction::GetEntry call.
class TreeTab {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void *GetEntry(int index);
};

class ObjectFolder;

class ObjSelector {
public:
    u32 field_0x0;         // 0x00
    u32 mNamespaceID;      // 0x04
    u32 mSemiNamespaceID;  // 0x08
    class Behavior *mBehavior; // 0x0c
    u8 pad_0x10[0x28];     // 0x10
    s16 *mDefinition;      // 0x38
    TreeTab *mTreeTab;     // 0x3c  Wii: a ConsoleAutoRefCount<ITreeTable>
    u8 pad_0x40[0x40];     // 0x40
    ObjectFolder *mFolder; // 0x80

    u32 GetNamespaceID() const { return mNamespaceID; }
    // Wii: GetIsPerson, whether the definition is a Sim's.
    int GetIsPerson(); // 0x1DA268
    u32 GetSemiNamespaceID() const { return mSemiNamespaceID; }
    s16 *GetDefinition() const { return mDefinition; }

    u8 pad_0x84[0x28];     // 0x84
    int mFlags;            // 0xac  bit 1: GetNeedsReset
    class CatalogResource *mCachedCatalogResource; // 0xb0  Wii: GetCachedSelectorCatalogResource
    u8 pad_0xb4[8];        // 0xb4
    void *mSimDescription; // 0xbc
    u32 pad_0xc0[5];       // 0xc0
    ObjSelector *mNext;    // 0xd4  the next selector in ObjectFolder's hash bucket

    ObjectFunctionTable *GetFnTable();
    int GetGUID() const;
    const class BString2 &GetUserLastName() const;    // 0x1DA068
    void SetUserLastName(const class BString2 &name); // 0x1DA0C8
    bool compare(int guid) const { return guid == GetGUID(); }
    // Wii keeps this as a function; cXObject::PostLoad inlines it.
    int GetNeedsReset() const { return (mFlags >> 1) & 1; }
    ObjSelector *GetMasterSelector();
    class TreeTab *GetTreeTab();
    int CountTypeAttributes();
    s16 *GetTypeAttributes();
};

#endif
