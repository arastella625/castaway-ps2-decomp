#ifndef GAME_SIMULATOR_CORE_CLOTHINGITEM_H
#define GAME_SIMULATOR_CORE_CLOTHINGITEM_H

#include "common/types.h"

class Family;
class SimBodyPart;
class cXObject;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
//
// The fields from 0x08 on, and the functions after the blank line, were added by hand from
// the Create-a-Sim code that uses them.
class ClothingItem {
public:
    u8 pad_0x0[0x8];
    s32 currentHitPoints; // 0x08
    s16 field_0xc;        // 0x0c
    s16 field_0xe;        // 0x0e
    u32 bodyPart;         // 0x10  a Character::eBodyPart
    u8 clothingSetIndex;  // 0x14
    int isMale;           // 0x18  a bool on Wii, a word here
    s32 isDamaged;        // 0x1c

    s16 GetID() const;
    int GetCount() const;
    int GetGuid() const;
    int GetCategory() const;
    s16 GetOwnerID() const;
    int GetOwnershipPoints(s16 a) const;
    int GetNumAttributes() const;
    int GetAttribute(int a);
    void SetAttribute(int a, s16 b);
    void AddAtribute(s16 a);
    int GetStatusText();
    void SetStatusText(s8 a);
    void IncrementCount(s16 a);
    void DecrementCount(s16 a);
    int GetToken();
    int GetIngredientToken();
    void LoadItemInfo(cXObject *a, Family *b, s16 c);
    int MakeObject(Family *a, s16 b);
    bool IsFood();
    bool IsFastFood();
    bool IsTool();
    bool IsFastTool();
    bool IsResource();
    bool CanBePlanted();
    bool IsSeed();
    bool CanBecomeSeed();
    bool IsHandObject();
    bool IsClothing();
    bool IsDye();
    void ClearAttributes();
    void SetupOwnershipInfo(cXObject *a);

    ClothingItem(s16 id, bool male);
    static s16 FindClothingItemID(const char *bodyPartName, const char *clothingSetName);
    // The PS2 build inlines these four; the Wii build keeps them as functions.
    void SetNeighborID(s16 id) { field_0xc = id; }
    int IsMale() const { return isMale; }
    u32 GetBodyPart() const { return bodyPart; }
    u8 GetClothingSetIndex() const { return clothingSetIndex; }
    void CopyFromSimBodyPart(SimBodyPart *part);
};

#endif
