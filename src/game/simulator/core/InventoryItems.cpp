#include "game/simulator/core/InventoryItems.h"

#include "game/simulator/core/Inventory.h"
#include "game/simulator/core/ObjSelector.h"
#include "game/simulator/core/cXObject.h"
#include "game/simulator/core/ObjectItem.h"
#include "game/simulator/core/OwnableItem.h"

// Sets the token's bit in the collected bits. Like HasTokenEverBeenCollected, it lets the word
// one past the end through.
void InventoryItems::MarkTokenAsBeingCollected(s16 id)
{
    int word = id / 32;
    s16 bit = id % 32;
    if (word <= mCollectedWords && mCollectedBits)
        mCollectedBits[word] |= 1 << bit;
}

// An object by GUID. A GUID that names a token adds one of that token instead.
int InventoryItems::AddObjectItem(int guid)
{
    s16 token = GetTokenIDFromGUID(guid);
    if (token) {
        AddToken(token, 1);
        return 1;
    }
    if (CanHoldMoreNonTokens()) {
        mNonTokenCount++;
        ObjectItem *item = new ObjectItem(guid);
        SetInstanceID(item);
        mItems.push_back(item);
        CheckForAddGoals(guid, 0);
        return 1;
    }
    return 0;
}

// Wii: object->GetSelector()->GetDefinition(), whose word at +0x1C is the GUID.
static inline int GetObjectGUID(cXObject *object)
{
    return *(int *)&object->mSelector->mDefinition[0xe];
}

// An object made from an object in the world. As by GUID, a token's GUID adds the token. The
// family's on-add tree runs with the new item's place.
int InventoryItems::AddObjectItem(cXObject *object)
{
    s16 token = GetTokenIDFromGUID(GetObjectGUID(object));
    if (token) {
        AddToken(token, 1);
        return 1;
    }
    if (CanHoldMoreNonTokens()) {
        mNonTokenCount++;
        ObjectItem *item = new ObjectItem(object);
        SetInstanceID(item);
        mItems.push_back(item);
        RunOnAddTree(mFamily, object, mItems.size() - 1);
        CheckForAddGoals(GetObjectGUID(object), 0);
        return 1;
    }
    return 0;
}

// An ownable made from an object. A family holds at most 0x20 items that aren't tokens; the
// family's on-add tree runs with the new item's place.
int InventoryItems::AddOwnableItem(cXObject *object)
{
    if (CanHoldMoreNonTokens()) {
        mNonTokenCount++;
        OwnableItem *item = new OwnableItem(object);
        SetInstanceID(item);
        mItems.push_back(item);
        RunOnAddTree(mFamily, object, mItems.size() - 1);
        return 1;
    }
    return 0;
}
