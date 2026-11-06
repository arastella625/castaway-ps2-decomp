#ifndef GAME_SIMULATOR_CORE2_QUICKDATASLOTDESCLIST_H
#define GAME_SIMULATOR_CORE2_QUICKDATASLOTDESCLIST_H

#include "common/types.h"

class ObjectDataID;
class SlotDescriptor;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class QuickDataSlotDescList {
public:
    u8 pad_0x0[0x5c];
    s32 field_0x5c; // 0x5c

    int AddSlot(SlotDescriptor &a);
    int RemoveSlot(int a);
    int GetStringSetID() const;
    void SetStringSetID(int a);
    int SaveDataByID(const ObjectDataID &a);
    int GetDataSourceType() const;
    int LoadOnlyNameAndIDFromIndex(u32 a, int b);
    void AddRef();
};

#endif
