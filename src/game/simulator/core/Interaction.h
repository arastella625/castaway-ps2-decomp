#ifndef GAME_SIMULATOR_CORE_INTERACTION_H
#define GAME_SIMULATOR_CORE_INTERACTION_H

// The vtable stays an external symbol, so the rebuild links to the game's copy at 0x4BC2B0.
#pragma interface

#include "common/types.h"

class cXObject;
class cXPerson;

class BString2 {
public:
    u32 field_0x0;

    BString2();
    BString2(const BString2 &other);
    ~BString2();
    void assign(const u16 *text);
    const u16 *c_str() const;
};

// One row of a tree table: only the slots AddAction calls.
class TreeTableEntry {
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
    virtual int GetImmediate();
    virtual void Slot11();
    virtual int GetAllowConsecutive();
};

// One queued action. The layout is from the Wii constructor; PS2 offsets are the same, with
// the vptr last at 0x3c.
class Interaction {
public:
    BString2 mName;         // 0x00
    u32 field_0x4;          // 0x04
    u32 field_0x8;          // 0x08  GetPerson
    cXObject *mStackObject; // 0x0c
    cXObject *mIconObject;  // 0x10
    s32 mTreeTabIndex;      // 0x14
    s16 field_0x18[4];      // 0x18
    u32 field_0x20;         // 0x20
    char *mExtra;           // 0x24  the destructor frees it
    u32 field_0x28;         // 0x28
    u32 field_0x2c;         // 0x2c
    u32 field_0x30;         // 0x30
    int mFlags;             // 0x34  bit 1 continuation, 5 push to head, 6 carry name over, 15 run as sub
    s16 field_0x38;         // 0x38

    Interaction()
    {
        field_0x4 = 0;
        field_0x8 = 0;
        mStackObject = 0;
        mIconObject = 0;
        mTreeTabIndex = -1;
        field_0x20 = 0;
        mExtra = 0;
        field_0x28 = 0;
        field_0x2c = 0;
        field_0x30 = 0;
        mFlags = 0;
        field_0x38 = 0;
        field_0x18[0] = 0;
        field_0x18[1] = 0;
        field_0x18[2] = 0;
        field_0x18[3] = 0;
    }
    Interaction(cXPerson *person, cXObject *object, int index, int priority);
    void SetIconObject(cXObject *object) { mIconObject = object; }
    bool GetContinuation() const { return (mFlags >> 1) & 1; }
    bool GetHidden() const { return (mFlags >> 4) & 1; }
    bool GetPushToHead() const { return (mFlags >> 5) & 1; }
    bool GetCarryNameOver() const { return (mFlags >> 6) & 1; }
    bool GetMustRun() const { return (mFlags >> 10) & 1; }
    bool GetRunAsSub() const { return (mFlags >> 15) & 1; }
    int GetPriority() const { return field_0x28; }
    TreeTableEntry *GetEntry() const;
    void SetUniqueID();
    const BString2 &GetName() const;
    void SetName(const BString2 &name);
    cXObject *GetIconObject() const;
    void SetFlag(int mask, bool on)
    {
        mFlags &= ~mask;
        if (on)
            mFlags |= mask;
    }
    void SetCarryNameOver(bool on) { SetFlag(0x40, on); }
    void SetContinuation(bool on) { SetFlag(2, on); }
    void SetPushToHead(bool on) { SetFlag(0x20, on); }
    void SetRunAsSub(bool on) { SetFlag(0x8000, on); }
    virtual ~Interaction()
    {
        if (mExtra)
            delete[] mExtra;
    }
    virtual void GetCTilePt() const;
    virtual void SetShader(void *icon) const;

    Interaction &operator=(const Interaction &other);
    u32 GetPerson() const;
    cXObject *GetStackObject() const { return mStackObject; }
    s32 GetTreeTabEntryIndex() const { return mTreeTabIndex; }
};

#endif
