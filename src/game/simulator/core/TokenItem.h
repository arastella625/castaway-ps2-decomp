#ifndef GAME_SIMULATOR_CORE_TOKENITEM_H
#define GAME_SIMULATOR_CORE_TOKENITEM_H

#include "common/types.h"
#include "game/simulator/core/InventoryItems.h"

class Family;
class cXObject;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class TokenItem : public InventoryItem {
public:
    s16 field_0x8; // 0x08  the token's ID
    s16 field_0xa; // 0x0a  the count
    u8 pad_0xc[0x4];
    const InventoryTokens *mToken; // 0x10  its "InventoryTokens" row. 0x14 bytes in all: InventoryItems::AddTokenItem allocates that

    TokenItem(s16 id, s16 count);

    s16 GetID() const;
    s16 GetCount() const;
    int GetCategory() const;
    int GetOwnerID() const;
    int GetOwnershipPoints(s16 a) const;
    int GetNumAttributes() const;
    int GetAttribute(int a);
    void SetAttribute(int a, s16 b);
    void AddAtribute(s16 a);
    s8 GetStatusText();
    void SetStatusText(s8 a);
    const InventoryTokens *GetToken();
    void LoadItemInfo(cXObject *a, Family *b, s16 c);
    bool IsClothing();
    bool IsDyable();
    void ClearAttributes();
    void SetupOwnershipInfo(cXObject *a);
};

#endif
