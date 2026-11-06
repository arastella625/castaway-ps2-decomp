#ifndef GAME_SIMULATOR_CORE_BEHAVIOR_H
#define GAME_SIMULATOR_CORE_BEHAVIOR_H

#include "common/types.h"

// One instruction of a tree, 12 bytes, as stored in a BHAV chunk. Field names are ours. The
// offsets and meanings come from TreeSim::DoNodeAction and DoNodeActionPrimitive.
struct BehaviorNode {
    u16 mOpcode;     // 0x00  low 15 bits: below 0x100 a primitive, else a tree to call
    u8 mTrueTarget;  // 0x02  a node index, or 0xfd error, 0xfe return true, 0xff return false
    u8 mFalseTarget; // 0x03
    s16 mParams[4];  // 0x04  eight operand bytes; all 0xff means "no parameters given"

    s16 GetPrimCode() const { return mOpcode & 0x7fff; }
    s16 GetTreeID() const { return mOpcode & 0x7fff; }
};
STATIC_ASSERT(BehaviorNode_size, sizeof(BehaviorNode) == 12);

// Wii: ObjectDataBehaviorTree, the loaded form of one BHAV chunk. mHeader points at the raw
// chunk body: u16 signature, u16 node count, u8 type, u8 params, u8 locals, ... Only what the
// interpreter needs is declared.
class ObjectDataBehaviorTree {
public:
    virtual void Slot1();
    virtual void Slot2();
    virtual int Release();

    u8 GetType() const;
    s16 GetNumNodes() const;
    u8 GetNumParams() const;
    u8 GetNumLocals() const;
};

template <class T>
class ConsoleAutoRefCount {
public:
    T *mObject;

    ConsoleAutoRefCount(T *object = 0) : mObject(object) {}
    ~ConsoleAutoRefCount()
    {
        if (mObject)
            mObject->Release();
    }
    T **AsPPTypeParam() { return &mObject; }
    T *operator->() const { return mObject; }
};

class Behavior {
public:
    int GetNode(s16 treeID, s16 node, BehaviorNode *result);
    int GetNodeRef(s16 treeID, s16 node, BehaviorNode **result);
    int GetTree(s16 treeID, ObjectDataBehaviorTree **result, bool forRun);

    static bool IsDefaultParam(const s16 *param)
    {
        return ((const int *)param)[0] == -1 && ((const int *)param)[1] == -1;
    }
};

#endif
