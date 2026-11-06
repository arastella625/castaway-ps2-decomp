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
    virtual int HasFunction(s16 index);
};

// An object's type: the namespace IDs that a tree's ID range selects, its definition, and its
// function table. Only the fields the decompiled code reads.
class ObjSelector {
public:
    u32 field_0x0;         // 0x00
    u32 mNamespaceID;      // 0x04
    u32 mSemiNamespaceID;  // 0x08
    u8 pad_0xc[0x2c];      // 0x0c
    s16 *mDefinition;      // 0x38
    u8 pad_0x3c[0x80];     // 0x3c
    void *mSimDescription; // 0xbc

    ObjectFunctionTable *GetFnTable();
    int CountTypeAttributes();
    s16 *GetTypeAttributes();
};

#endif
