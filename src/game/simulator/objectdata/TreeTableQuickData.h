#ifndef GAME_SIMULATOR_OBJECTDATA_TREETABLEQUICKDATA_H
#define GAME_SIMULATOR_OBJECTDATA_TREETABLEQUICKDATA_H

#include "common/types.h"

class ITreeTableEntry;
class ObjectDataID;
class StringBuffer;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class TreeTableQuickData {
public:
    u8 pad_0x0[0x20];
    s32 field_0x20; // 0x20

    int LoadFromIndex(u32 a, int b);
    int GetResourceName(StringBuffer &a) const;
    int SetResourceName(const StringBuffer &a);
    int LoadOnlyNameAndIDFromIndex(u32 a, int b);
    void SwapEntries(int a, int b);
    int GetNthEntry(int a);
    int GetEntryByIndex(int a);
    int GetNthOrderedEntry(int a);
    void SetName(const StringBuffer &a);
    bool AreOrderIndicesValid() const;
    int GetIsTuned() const;
    void SetIsTuned(bool a);
    int AddEntry(s16 a, s16 b);
    int RemoveEntry(const ITreeTableEntry *a);
    void SetPrefixCheckTreeID(s16 a);
    int SaveDataByID(const ObjectDataID &a);
    int GetDataSourceType() const;
    void AddRef();
};

#endif
