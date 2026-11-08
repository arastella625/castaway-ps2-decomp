#include "game/simulator/core/ObjSelector.h"

// Wii: ObjectFolder::GetTreeTable, which loads a selector's tree table.
// Wii: ObjectTypeAttrBlock, the type attributes shared by every object of one type.
struct ObjectTypeAttrBlock {
    u32 field_0x0;
    int mNumAttr;  // 0x04  Wii: GetNumAttr
    s16 *mAttrs;   // 0x08
};

class ObjectFolder {
public:
    void GetTreeTable(ObjSelector *selector);
    ObjectTypeAttrBlock *GetTypeAttrBlock(int id);
};

// The block is found by definition +0xB8, or by the GUID (+0x1C) when that's 0. A definition
// with no block has no type attributes: the count is 0 and the array null.
inline int TypeAttrID(const s16 *definition)
{
    const int *words = (const int *)definition;
    int id = words[0xb8 / 4];
    if (id == 0)
        id = words[0x1c / 4];
    return id;
}

int ObjSelector::CountTypeAttributes()
{
    ObjectTypeAttrBlock *block = mFolder->GetTypeAttrBlock(TypeAttrID(mDefinition));
    if (block == 0)
        return 0;
    return block->mNumAttr;
}

s16 *ObjSelector::GetTypeAttributes()
{
    ObjectTypeAttrBlock *block = mFolder->GetTypeAttrBlock(TypeAttrID(mDefinition));
    if (block == 0)
        return 0;
    return block->mAttrs;
}

// Wii: ObjSelector::GetTreeTable. The table is loaded the first time it's asked for.
TreeTab *ObjSelector::GetTreeTab()
{
    if (!mTreeTab)
        mFolder->GetTreeTable(this);
    return mTreeTab;
}
