#include "game/simulator/core/TokenItem.h"

#include "common/ReconBuffer.h"
#include "game/simulator/core/Inventory.h"

s16 TokenItem::GetID() const
{
    return field_0x8;
}

s16 TokenItem::GetCount() const
{
    return field_0xa;
}

s8 TokenItem::GetCategory() const
{
    return 2;
}

s16 TokenItem::GetOwnerID() const
{
    return -1;
}

int TokenItem::GetOwnershipPoints(s16 a) const
{
    return -1;
}

int TokenItem::GetNumAttributes() const
{
    return -1;
}

s16 TokenItem::GetAttribute(int a)
{
    return 0;
}

void TokenItem::SetAttribute(int a, s16 b)
{
}

void TokenItem::AddAtribute(s16 a)
{
}

s8 TokenItem::GetStatusText()
{
    return -1;
}

void TokenItem::SetStatusText(s8 a)
{
}

const InventoryTokens *TokenItem::GetToken()
{
    return mToken;
}

void TokenItem::LoadItemInfo(cXObject *a, Family *b, s16 c)
{
}

bool TokenItem::IsClothing()
{
    return false;
}

bool TokenItem::IsDyable()
{
    return false;
}

void TokenItem::ClearAttributes()
{
}

void TokenItem::SetupOwnershipInfo(cXObject *a)
{
}

// The GUID of the token's row. Unlike the flag tests below, this doesn't check for a row.
u32 TokenItem::GetGuid() const
{
    return mToken->mGuid;
}

// Adds to the count and caps it at 999.
void TokenItem::IncrementCount(s16 count)
{
    field_0xa += count;
    if (field_0xa >= 1000)
        field_0xa = 999;
}

void TokenItem::DecrementCount(s16 count)
{
    IncrementCount(-count);
}

// Not matched: 2 words longer than the game's 23. The game leaves GetIngredientManager's delay
// slot empty and reloads mToken after the call; this build hoists that reload into the slot,
// and its two `return 0` exits leave a jump from the call path to the epilogue where the game
// falls through. Tried: the calls nested, a ternary, two early returns, the manager in a local.
#ifdef NON_MATCHING
// The ingredient row of a food token, or null.
const IngredientToken *TokenItem::GetIngredientToken()
{
    if (mToken && mToken->HasFlag(4)) {
        IngredientManager *manager = GetIngredientManager();
        return manager->FindIngredientToken(mToken->field_0xe);
    }
    return 0;
}
#endif

// Streams the ID and the count, then finds the token's row again.
void TokenItem::DoStream(ReconBuffer *buffer, int a)
{
    buffer->Recon16(&field_0x8, 1);
    buffer->Recon16(&field_0xa, 1);
    mToken = InventoryItems::GetTokenFromID(field_0x8);
}

// The flag tests, each a bit of the row's flags. A token without a row is none of these.
bool TokenItem::IsFood()
{
    if (mToken)
        return mToken->HasFlag(4);
    return false;
}

bool TokenItem::IsFastFood()
{
    if (mToken)
        return (mToken->mFlags >> 7) & 1;
    return false;
}

bool TokenItem::IsTool()
{
    if (mToken)
        return mToken->mFlags & 1;
    return false;
}

bool TokenItem::IsFastTool()
{
    if (mToken)
        return mToken->HasFlag(0x20);
    return false;
}

bool TokenItem::IsResource()
{
    if (mToken)
        return mToken->HasFlag(2);
    return false;
}

bool TokenItem::CanBePlanted()
{
    if (mToken)
        return mToken->HasFlag(0x400);
    return false;
}

bool TokenItem::IsHandObject()
{
    if (mToken)
        return mToken->HasFlag(0x100);
    return false;
}

bool TokenItem::IsDye()
{
    if (mToken)
        return mToken->HasFlag(0x4000);
    return false;
}
