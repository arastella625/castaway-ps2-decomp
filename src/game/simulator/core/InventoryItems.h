#ifndef GAME_SIMULATOR_CORE_INVENTORYITEMS_H
#define GAME_SIMULATOR_CORE_INVENTORYITEMS_H

#include "common/eastl.h"
#include "common/types.h"
#include "game/cas/CasSimDescription.h"

class ClothingItem;
// Wii: InventoryItem. Its vptr comes after the first word.
class InventoryItem {
public:
    u32 field_0x0;

    virtual s16 GetID();
    virtual void Slot1();
    virtual void Slot2();
    virtual s8 GetKind();
};

// Wii: InventoryItems. Only what the decompiled code calls is declared so far.
class InventoryItems {
public:
    u32 field_0x0;                                         // 0x00
    eastl::vector<InventoryItem *, eastl::allocator> mItems; // 0x04
    u32 field_0x14;                                        // 0x14
    s32 *mCollectedBits;                                   // 0x18
    int mCollectedWords;                                   // 0x1c

    static const struct InventoryTokens *GetTokenFromID(s16 id);
    static InventoryItems *GetTribeInventory();
    int GetTokenItemIndex(s16 id);
    bool HasTokenEverBeenCollected(s16 id) const;

    ClothingItem *GetClothingItem(s16 neighborID, Character::eBodyPart part);
    void AddItem(InventoryItem *item);
};

#endif
