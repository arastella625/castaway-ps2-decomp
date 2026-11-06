#ifndef GAME_SIMULATOR_CORE_INVENTORYITEMS_H
#define GAME_SIMULATOR_CORE_INVENTORYITEMS_H

#include "common/types.h"
#include "game/cas/CasSimDescription.h"

class ClothingItem;
class InventoryItem;

// Wii: InventoryItems. Only what the decompiled code calls is declared so far.
class InventoryItems {
public:
    ClothingItem *GetClothingItem(s16 neighborID, Character::eBodyPart part);
    void AddItem(InventoryItem *item);
};

#endif
