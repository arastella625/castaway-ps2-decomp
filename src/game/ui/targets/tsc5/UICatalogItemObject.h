#ifndef GAME_UI_TARGETS_TSC5_UICATALOGITEMOBJECT_H
#define GAME_UI_TARGETS_TSC5_UICATALOGITEMOBJECT_H

#include "common/types.h"

class InventoryItems;

namespace UISimulatorSupport {
// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class UICatalogItemObject {
public:
    u8 pad_0x0[0x8];
    u32 field_0x8; // 0x08

    u32 GetSelector();
    bool IsEatFromInventory() const;
    bool IsCultivatable() const;
    bool IsClothing() const;
    void AddToInventory(InventoryItems *a, int b) const;
    bool IsRecentlyUnlocked() const;
    void SetRecentlyUnlockedStatus(bool a);
};
}

#endif
