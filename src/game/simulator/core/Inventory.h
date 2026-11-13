#ifndef GAME_SIMULATOR_CORE_INVENTORY_H
#define GAME_SIMULATOR_CORE_INVENTORY_H

#include "common/EGlobal.h"
#include "common/ERQuickdata.h"
#include "common/eastl.h"
#include "common/types.h"
#include "game/simulator/core/ClothingItem.h"
#include "game/simulator/core/InventoryItems.h"
#include "game/simulator/core/Neighborhood.h"
#include "game/simulator/core/OwnableItem.h"
#include "game/simulator/core2/cXPerson.h"
#include "game/simulator/objectdata/XObjLang.h"

// Wii: ERQuickdata::GetTable<T>, which fills in a pointer to the typed table.
inline void GetTableOf(ERQuickdata *data, const char *name, const ERQTable **result)
{
    const ERQTable *table = data->GetTable(name);
    if (table)
        *result = table;
    else
        *result = 0;
}

// One row of the "Ingredient" quickdata table, 0x44 bytes. The fields are scope 0x2a's columns.
struct IngredientToken {
    s16 mID;        // 0x00
    s16 field_0x2;  // 0x02
    u32 pad_0x4[2]; // 0x04
    s16 field_0xc;  // 0x0c
    s16 pad_0xe;    // 0x0e
    s16 field_0x10; // 0x10
    s16 pad_0x12;   // 0x12
    s16 field_0x14; // 0x14
    s16 pad_0x16;   // 0x16
    s16 field_0x18; // 0x18
    s16 pad_0x1a;   // 0x1a
    s16 field_0x1c; // 0x1c
    s16 pad_0x1e;   // 0x1e
    u32 pad_0x20[6]; // 0x20
    s16 field_0x38; // 0x38
    s16 pad_0x3a;   // 0x3a
    u32 pad_0x3c[2]; // 0x3c
};
STATIC_ASSERT(IngredientToken_size, sizeof(IngredientToken) == 0x44);

// Wii: IngredientManager. It's made the first time it's asked for.
class IngredientManager {
public:
    u32 pad_0x0[2];

    IngredientManager();

    // 0x24F898; the body under the second name is what InterpValue inlines.
    const IngredientToken *FindIngredientToken(s16 id) const;
    const IngredientToken *FindIngredientTokenInline(s16 id) const
    {
        const ERQTable *rows;
        GetTableOf(GetObjectFolder()->mDatabase, "Ingredient", &rows);
        for (u32 i = 0; i < rows->count; i++) {
            const IngredientToken *token = &((const IngredientToken *)rows->rows)[i];
            if (token->mID == id)
                return token;
        }
        return 0;
    }
};

extern IngredientManager *spIngredientManager;

IngredientManager *GetIngredientManager(); // 0x24F688

inline IngredientManager *GetIngredientManagerInline()
{
    if (spIngredientManager == 0)
        spIngredientManager = new IngredientManager;
    return spIngredientManager;
}

// One row of the "InventoryTokens" quickdata table, 0x24 bytes. The fields are scope 0x42's
// columns.
struct InventoryTokens {
    u32 mGuid;      // 0x00  the object GUID the token stands for, 0 for none
    u32 mFlags;     // 0x04  bit masks: 1 tool, 2 resource, 4 food, 0x20 fast tool, 0x80 fast food,
                    //       0x100 hand object, 0x400 can be planted, 0x4000 dye (TokenItem's tests)
    u32 pad_0x8;    // 0x08
    u16 mID;        // 0x0c
    s16 field_0xe;  // 0x0e
    s16 field_0x10; // 0x10
    s16 pad_0x12;   // 0x12
    s16 field_0x14; // 0x14
    s16 pad_0x16;   // 0x16
    s16 field_0x18; // 0x18
    s16 field_0x1a; // 0x1a
    s16 field_0x1c; // 0x1c
    s16 field_0x1e; // 0x1e
    s16 field_0x20; // 0x20
    s16 field_0x22; // 0x22

    int GetHarvestTool() const;
    // TokenItem's tests go through this: written as (mFlags & mask) != 0 in each, the compiler
    // turns a single-bit test into a shift, and the game has andi then sltu.
    bool HasFlag(u32 mask) const { return (mFlags & mask) != 0; }
};
STATIC_ASSERT(InventoryTokens_size, sizeof(InventoryTokens) == 0x24);

inline const InventoryTokens *InventoryItems::GetTokenFromID(s16 id)
{
    if (id == 0)
        return 0;
    const ERQTable *table = GetObjectFolder()->mDatabase->GetTable("InventoryTokens");
    if (table == 0)
        return 0;
    for (u32 i = 0; i < table->count; i++) {
        const InventoryTokens *token = &((const InventoryTokens *)table->rows)[i];
        if (token->mID == id)
            return token;
    }
    return 0;
}

// The row of the "ClothingItems" table with this ID, or null. Inlined on PS2; the name is ours.
inline const ClothingItemsRow *InventoryItems::GetClothingItemsRow(s16 id)
{
    if (id == 0)
        return 0;
    const ERQTable *table = GetObjectFolder()->mDatabase->GetTable("ClothingItems");
    if (table == 0)
        return 0;
    for (u32 i = 0; i < table->count; i++) {
        if (((const ClothingItemsRow *)table->rows)[i].mID == id)
            return &((const ClothingItemsRow *)table->rows)[i];
    }
    return 0;
}

// Wii: GetTokenIDFromGUID. The token whose row's first word is the GUID, or 0.
inline s16 InventoryItems::GetTokenIDFromGUID(int guid)
{
    if (guid == 0)
        return 0;
    const ERQTable *table = GetObjectFolder()->mDatabase->GetTable("InventoryTokens");
    if (table == 0)
        return 0;
    for (u32 i = 0; i < table->count; i++) {
        const InventoryTokens *token = &((const InventoryTokens *)table->rows)[i];
        if (token->mGuid == (u32)guid)
            return token->mID;
    }
    return 0;
}

inline InventoryItems *InventoryItems::GetTribeInventory()
{
    cXPerson *person = _globals.mPlayerPerson;
    if (person == 0)
        return 0;
    return person->mNeighbor->GetFamily()->GetInventory();
}

inline int InventoryItems::GetTokenItemIndex(s16 id)
{
    int index = 0;
    for (InventoryItem **i = mItems.begin(); i != mItems.end(); ++i, ++index) {
        InventoryItem *item = *i;
        if (item->GetCategory() == 2 && item->GetID() == id)
            return index;
    }
    return -1;
}

inline bool InventoryItems::HasTokenEverBeenCollected(s16 id) const
{
    int word = id / 32;
    s16 bit = id % 32;
    bool collected = false;
    if (word <= mCollectedWords && mCollectedBits)
        collected = (mCollectedBits[word] >> bit) & 1;
    return collected;
}

// Wii: CountItems, GetNonTokenCount and GetItem. A place past the end is null.
inline int InventoryItems::CountItems()
{
    return mItems.size();
}

inline u8 InventoryItems::GetNonTokenCount() const
{
    return mNonTokenCount;
}

inline InventoryItem *InventoryItems::GetItem(int index)
{
    if (index >= (int)mItems.size())
        return 0;
    return mItems[index];
}

// Wii: GetObjectItemIndex, GetOwnableItemIndex and GetClothingItemIndex. The place of the first
// object with this GUID; of the ownable with this GUID whose ownership points for the owner are
// the highest above 0; of the first clothing item with this ID. -1 when there is none.
inline int InventoryItems::GetObjectItemIndex(int guid)
{
    int index = 0;
    for (InventoryItem **i = mItems.begin(); i != mItems.end(); ++i, ++index) {
        InventoryItem *item = *i;
        if (item->GetGuid() == guid)
            return index;
    }
    return -1;
}

inline int InventoryItems::GetOwnableItemIndex(s16 owner, int guid)
{
    int best = 0;
    int index = 0;
    int result = -1;
    for (InventoryItem **i = mItems.begin(); i != mItems.end(); ++i, ++index) {
        InventoryItem *item = *i;
        if (item->GetGuid() == guid) {
            int points = item->GetOwnershipPoints(owner);
            if (points > best) {
                best = points;
                result = index;
            }
        }
    }
    return result;
}

inline int InventoryItems::GetClothingItemIndex(s16 id)
{
    int index = 0;
    for (InventoryItem **i = mItems.begin(); i != mItems.end(); ++i, ++index) {
        InventoryItem *item = *i;
        if (item->GetCategory() == 3 && item->GetID() == id)
            return index;
    }
    return -1;
}

// Wii: RemoveObjectItem, RemoveOwnableItem, RemoveTokenItem and RemoveClothingItem. Each finds
// the item's place and removes one of it (count of a token). Removing an object or an ownable
// is always true; a token or clothing remove is false when there was none. Removing a token
// with an ID from 0xD5 to 0x112 also unlocks goal-and-unlock entry 0x70 of kind 2.
inline int InventoryItems::RemoveObjectItem(int guid)
{
    int index = GetObjectItemIndex(guid);
    if (index != -1)
        RemoveItem(index, 1);
    return 1;
}

inline int InventoryItems::RemoveOwnableItem(s16 owner, u32 guid)
{
    int index = GetOwnableItemIndex(owner, guid);
    if (index != -1)
        RemoveItem(index, 1);
    return 1;
}

inline int InventoryItems::RemoveTokenItem(s16 id, s16 count)
{
    int index = GetTokenItemIndex(id);
    if (index != -1) {
        RemoveItem(index, count);
        if (id >= 0xD5 && id <= 0x112)
            GetNeighborhood()->GetGoalAndUnlockData()->Unlock(2, 0x70);
        return 1;
    }
    return 0;
}

inline int InventoryItems::RemoveClothingItem(s16 id)
{
    int index = GetClothingItemIndex(id);
    if (index != -1) {
        RemoveItem(index, 1);
        return 1;
    }
    return 0;
}

// Wii: AddOwnableItem by GUID. Unlike the add by object, no on-add tree runs.
inline int InventoryItems::AddOwnableItem(u32 guid)
{
    if (CanHoldMoreNonTokens()) {
        mNonTokenCount++;
        OwnableItem *item = new OwnableItem(guid);
        SetInstanceID(item);
        mItems.push_back(item);
        return 1;
    }
    return 0;
}

#endif
