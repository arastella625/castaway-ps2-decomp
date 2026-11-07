#ifndef GAME_SIMULATOR_CORE_INVENTORY_H
#define GAME_SIMULATOR_CORE_INVENTORY_H

#include "common/EGlobal.h"
#include "common/ERQuickdata.h"
#include "common/eastl.h"
#include "common/types.h"
#include "game/simulator/core/InventoryItems.h"
#include "game/simulator/core/Neighborhood.h"
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

    const IngredientToken *FindIngredientToken(s16 id) const
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

inline IngredientManager *GetIngredientManager()
{
    if (spIngredientManager == 0)
        spIngredientManager = new IngredientManager;
    return spIngredientManager;
}

// One row of the "InventoryTokens" quickdata table, 0x24 bytes. The fields are scope 0x42's
// columns.
struct InventoryTokens {
    s16 field_0x0;  // 0x00
    s16 field_0x2;  // 0x02
    s16 field_0x4;  // 0x04
    s16 field_0x6;  // 0x06
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
        if (item->GetKind() == 2 && item->GetID() == id)
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

#endif
