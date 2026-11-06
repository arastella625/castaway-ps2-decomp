#ifndef GAME_SIMULATOR_CORE_INTERACTION_H
#define GAME_SIMULATOR_CORE_INTERACTION_H

// The vtable stays an external symbol, so the rebuild links to the game's copy at 0x4BC2B0.
#pragma interface

#include "common/types.h"

class cXObject;

class BString2 {
public:
    u32 field_0x0;

    BString2();
    ~BString2();
};

// One queued action. The layout is from the Wii constructor; PS2 offsets are the same, with
// the vptr last at 0x3c.
class Interaction {
public:
    BString2 mName;         // 0x00
    u32 field_0x4;          // 0x04
    u32 field_0x8;          // 0x08  GetPerson
    cXObject *mStackObject; // 0x0c
    u32 field_0x10;         // 0x10
    s32 mTreeTabIndex;      // 0x14
    s16 field_0x18[4];      // 0x18
    u32 field_0x20;         // 0x20
    char *mExtra;           // 0x24  the destructor frees it
    u32 field_0x28;         // 0x28
    u32 field_0x2c;         // 0x2c
    u32 field_0x30;         // 0x30
    u32 field_0x34;         // 0x34
    s16 field_0x38;         // 0x38

    Interaction()
    {
        field_0x4 = 0;
        field_0x8 = 0;
        mStackObject = 0;
        field_0x10 = 0;
        mTreeTabIndex = -1;
        field_0x20 = 0;
        mExtra = 0;
        field_0x28 = 0;
        field_0x2c = 0;
        field_0x30 = 0;
        field_0x34 = 0;
        field_0x38 = 0;
        field_0x18[0] = 0;
        field_0x18[1] = 0;
        field_0x18[2] = 0;
        field_0x18[3] = 0;
    }
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
