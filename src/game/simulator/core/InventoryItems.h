#ifndef GAME_SIMULATOR_CORE_INVENTORYITEMS_H
#define GAME_SIMULATOR_CORE_INVENTORYITEMS_H

#include "common/eastl.h"
#include "common/types.h"
#include "game/cas/CasSimDescription.h"

class ClothingItem;
class Family;
class cXObject;

// Wii: InventoryItem. Its vptr comes after the first word.
class InventoryItem {
public:
    s16 mInstanceID; // 0x00  Wii: GetInstanceID, given by InventoryItems::SetInstanceID
    u16 pad_0x2;

    s16 GetInstanceID() const { return mInstanceID; }
    void SetInstanceID(s16 id) { mInstanceID = id; }

    virtual s16 GetID();
    virtual void Slot1();
    virtual void Slot2();
    virtual s8 GetKind();
};

// Wii: InventoryItems. Only what the decompiled code calls is declared so far.
class InventoryItems {
public:
    u8 mNonTokenCount;                                     // 0x00  at most 0x20
    u8 pad_0x1[3];
    eastl::vector<InventoryItem *, eastl::allocator> mItems; // 0x04
    Family *mFamily;                                       // 0x14
    s32 *mCollectedBits;                                   // 0x18
    int mCollectedWords;                                   // 0x1c

    static const struct InventoryTokens *GetTokenFromID(s16 id);
    static InventoryItems *GetTribeInventory();
    int GetTokenItemIndex(s16 id);
    bool HasTokenEverBeenCollected(s16 id) const;
    void MarkTokenAsBeingCollected(s16 id);

    ClothingItem *GetClothingItem(s16 neighborID, Character::eBodyPart part);
    void AddItem(InventoryItem *item);
    int AddOwnableItem(cXObject *object);
    int AddObjectItem(int guid);
    int AddObjectItem(cXObject *object);
    void CheckForAddGoals(int guid, s16 a);
    static s16 GetTokenIDFromGUID(int guid);
    static void RunOnAddTree(Family *family, cXObject *object, s16 index);
    static s16 s_nNextInstanceID;

    // Wii: CanHoldMoreNonTokens and SetInstanceID, inlined on PS2.
    bool CanHoldMoreNonTokens() const { return mNonTokenCount < 0x20; }
    static void SetInstanceID(InventoryItem *item)
    {
        if (item && item->GetInstanceID() == 0) {
            item->SetInstanceID(s_nNextInstanceID);
            s_nNextInstanceID++;
        }
    }
    void AddToken(s16 id, s16 count);
    void GimmeAllFood(s16 count);
    void GimmeAllResources(s16 count);
    void GimmeAllTools();
    void Replenish(s16 amount);
};

#endif
