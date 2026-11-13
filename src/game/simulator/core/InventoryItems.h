#ifndef GAME_SIMULATOR_CORE_INVENTORYITEMS_H
#define GAME_SIMULATOR_CORE_INVENTORYITEMS_H

#include "common/eastl.h"
#include "common/types.h"
#include "game/cas/CasSimDescription.h"

class ClothingItem;
class Family;
class ReconBuffer;
class cXObject;
struct InventoryTokens;
struct IngredientToken;

// Wii: InventoryItem. Its vptr comes after the first word. The slots are named from the game's
// ObjectItem, TokenItem and ClothingItem vtables (0x4BB460, 0x4BB570 and 0x4BB230), which
// tools/verify_vtables.py checks. Slot 29 is the destructor: InventoryItems::RemoveItem deletes
// through it.
class InventoryItem {
public:
    s16 mInstanceID; // 0x00  Wii: GetInstanceID, given by InventoryItems::SetInstanceID
    u16 pad_0x2;

    s16 GetInstanceID() const { return mInstanceID; }
    void SetInstanceID(s16 id) { mInstanceID = id; }

    virtual s16 GetID() const;                        // 0: the token's or clothing's ID
    virtual s16 GetCount() const;                     // 1
    virtual u32 GetGuid() const;                      // 2
    virtual s8 GetCategory() const;                   // 3: 0 object, 1 ownable, 2 token, 3 clothing
    virtual s16 GetOwnerID() const;                   // 4
    virtual int GetOwnershipPoints(s16 a) const;      // 5
    virtual int GetNumAttributes() const;             // 6
    virtual s16 GetAttribute(int index);              // 7
    virtual void SetAttribute(int index, s16 value);  // 8
    virtual void AddAtribute(s16 value);              // 9 (the game's spelling)
    virtual s8 GetStatusText();                       // 10
    virtual void SetStatusText(s8 text);              // 11
    virtual void IncrementCount(s16 count);           // 12
    virtual void DecrementCount(s16 count);           // 13
    virtual const InventoryTokens *GetToken();        // 14
    virtual const IngredientToken *GetIngredientToken(); // 15
    virtual void DoStream(ReconBuffer *buffer, int a); // 16
    virtual void LoadItemInfo(cXObject *object, Family *family, s16 a); // 17
    virtual int MakeObject(Family *family, s16 a);    // 18
    virtual bool IsFood();                            // 19
    virtual bool IsFastFood();                        // 20
    virtual bool IsTool();                            // 21
    virtual bool IsFastTool();                        // 22
    virtual bool IsResource();                        // 23
    virtual bool CanBePlanted();                      // 24
    virtual bool IsHandObject();                      // 25
    virtual bool IsClothing();                        // 26
    virtual bool IsDye();                             // 27
    virtual bool IsDyable();                          // 28
    virtual ~InventoryItem();                         // 29
    virtual void ClearAttributes();                   // 30
    virtual void SetupOwnershipInfo(cXObject *object); // 31
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
    void AddTokenItem(s16 id, s16 count);
    void AddClothingItem(s16 id);
    void RemoveItem(int index, s16 count);
    void MergeInventories(InventoryItems *other); // 0x1FA4D0
    // Wii keeps these as functions; TryFamilyInventory inlines every one. Inventory.h has them.
    int CountItems();
    u8 GetNonTokenCount() const;
    InventoryItem *GetItem(int index);
    int GetObjectItemIndex(int guid);
    int GetOwnableItemIndex(s16 owner, int guid);
    int GetClothingItemIndex(s16 id);
    int RemoveObjectItem(int guid);
    int RemoveOwnableItem(s16 owner, u32 guid);
    int RemoveTokenItem(s16 id, s16 count);
    int RemoveClothingItem(s16 id);
    int AddOwnableItem(u32 guid);
    static const struct ClothingItemsRow *GetClothingItemsRow(s16 id);
    void GimmeAllFood(s16 count);
    void GimmeAllResources(s16 count);
    void GimmeAllTools();
    void Replenish(s16 amount);
};

#endif
