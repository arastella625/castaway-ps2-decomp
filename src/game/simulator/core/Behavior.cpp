#include "game/simulator/core/Behavior.h"

// The namespace IDs that a tree's ID range selects. Only the fields these functions read.
class ObjSelector {
public:
    u32 field_0x0;       // 0x00
    u32 mNamespaceID;    // 0x04
    u32 mSemiNamespaceID; // 0x08
};

// Wii: ObjectFolder's global namespace ID, a static read by GetGlobNamespaceID.
extern u32 sGlobNamespaceID;

u32 Behavior::GetNamespaceID()
{
    return mSelector->mNamespaceID;
}

u32 Behavior::GetGlobNamespaceID()
{
    return sGlobNamespaceID;
}

// The most recently used trees, newest first. Its size is set elsewhere and stays fixed: a miss
// reuses the last entry.
int Behavior::GetNodeRef(s16 treeID, s16 node, BehaviorNode **result)
{
    if (result == 0)
        return 0;
    *result = 0;
    ObjectDataBehaviorTree *tree = 0;
    ObjectDataID id(GetResFileID(treeID), (u16)treeID);
    u32 i;
    for (i = 0; i < s_cachedTrees.size(); i++) {
        if (s_cachedTrees[i]->mID == id) {
            tree = s_cachedTrees[i];
            for (; i != 0; i--)
                s_cachedTrees[i] = s_cachedTrees[i - 1];
            s_cachedTrees[0] = tree;
            break;
        }
    }
    if (tree == 0) {
        i = s_cachedTrees.size() - 1;
        tree = s_cachedTrees[i];
        if (GetTree(treeID, &tree, false) == 0)
            return 0;
        for (; i != 0; i--)
            s_cachedTrees[i] = s_cachedTrees[i - 1];
        s_cachedTrees[0] = tree;
    }
    if (node < 0 || node >= tree->GetNumNodes())
        return 0;
    BehaviorNode *found = tree->GetNode(node);
    if (found != 0) {
        *result = found;
        return 1;
    }
    return 0;
}

int Behavior::GetNode(s16 treeID, s16 node, BehaviorNode *result)
{
    BehaviorNode *found = 0;
    int ok = GetNodeRef(treeID, node, &found);
    if (ok)
        *result = *found;
    return ok;
}

u32 Behavior::GetResFileID(s16 treeID)
{
    if (treeID < 0x100 || treeID > 0x2328)
        return -1;
    if (treeID < 0x1000)
        return sGlobNamespaceID;
    if (treeID < 0x2000)
        return mSelector->mNamespaceID;
    if (treeID < 0x2329)
        return mSelector->mSemiNamespaceID;
    return -1;
}

int Behavior::GetTree(s16 treeID, ObjectDataBehaviorTree **result, bool load)
{
    u32 resFile = GetResFileID(treeID);
    if (resFile == (u32)-1)
        return 0;

    ObjectDataID id(resFile, (u16)treeID);
    if (*result == 0)
        GetSmartDataFactory()->GetData(0x2beb2426, id, result);
    if (*result != 0) {
        if (load)
            return (*result)->LoadFromDataID(id);
        return (*result)->GetFromDataID(id);
    }
    return 0;
}

u8 ObjectDataBehaviorTree::GetType() const
{
    return mHeader->mType;
}

s16 ObjectDataBehaviorTree::GetNumNodes() const
{
    return mHeader->mNumNodes;
}

u8 ObjectDataBehaviorTree::GetNumParams() const
{
    const BehaviorHeader *header = mHeader;
    if (header->mSignature >= (s16)0x8002)
        return header->mNumParams;
    if (header->mSignature == (s16)0x8001) {
        if (header->mNumParams < 5)
            return header->mNumParams;
        return 4;
    }
    return 4;
}

u8 ObjectDataBehaviorTree::GetNumLocals() const
{
    const BehaviorHeader *header = mHeader;
    if (header->mSignature >= (s16)0x8002)
        return header->mNumLocals;
    if (header->mSignature == (s16)0x8001) {
        if (header->mNumLocals < 5)
            return header->mNumLocals;
        return 4;
    }
    return 0;
}

BehaviorNode *ObjectDataBehaviorTree::GetNode(u32 index) const
{
    return (BehaviorNode *)((u8 *)mHeader + 0xc + index * sizeof(BehaviorNode));
}
