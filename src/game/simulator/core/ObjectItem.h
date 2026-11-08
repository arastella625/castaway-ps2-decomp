#ifndef GAME_SIMULATOR_CORE_OBJECTITEM_H
#define GAME_SIMULATOR_CORE_OBJECTITEM_H

#include "common/types.h"
#include "game/simulator/core/InventoryItems.h"

class cXObject;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class ObjectItem : public InventoryItem {
public:
    u32 field_0x8; // 0x08  the GUID
    s8 field_0xc; // 0x0c
    u8 pad_0xd[0x17]; // 0x24 bytes in all: InventoryItems::AddObjectItem allocates that

    ObjectItem(int guid);
    ObjectItem(cXObject *object);

    s16 GetID() const;
    u32 GetGuid() const;
    int GetCategory() const;
    s16 GetCount() const;
    int GetOwnerID() const;
    int GetOwnershipPoints(s16 a) const;
    s8 GetStatusText();
    void SetStatusText(s8 value);
    void IncrementCount(s16 a);
    void DecrementCount(s16 a);
    int GetToken();
    int GetIngredientToken();
    bool IsFood();
    bool IsFastFood();
    bool IsTool();
    bool IsFastTool();
    bool IsResource();
    bool CanBePlanted();
    bool IsHandObject();
    bool IsClothing();
    bool IsDye();
    bool IsDyable();
    void SetupOwnershipInfo(cXObject *a);
};

#endif
