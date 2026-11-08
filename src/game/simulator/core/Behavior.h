#ifndef GAME_SIMULATOR_CORE_BEHAVIOR_H
#define GAME_SIMULATOR_CORE_BEHAVIOR_H

#include "common/eastl.h"
#include "common/types.h"

// One instruction of a tree, 12 bytes, as stored in a BHAV chunk. Field names are ours. The
// offsets and meanings come from TreeSim::DoNodeAction and DoNodeActionPrimitive; tools/bhav.py
// checks them against every tree on the disc.
struct BehaviorNode {
    u16 mOpcode;     // 0x00  low 15 bits: below 0x100 a primitive, else a tree to call
    u8 mTrueTarget;  // 0x02  a node index, or 0xfd error, 0xfe return true, 0xff return false
    u8 mFalseTarget; // 0x03
    s16 mParams[4];  // 0x04  eight operand bytes; all 0xff means "no parameters given"

    s16 GetPrimCode() const { return mOpcode & 0x7fff; }
    s16 GetTreeID() const { return mOpcode & 0x7fff; }
};
STATIC_ASSERT(BehaviorNode_size, sizeof(BehaviorNode) == 12);

// The first bytes of a BHAV chunk's body, as ObjectDataBehaviorTree reads them. Names are ours.
struct BehaviorHeader {
    s16 mSignature; // 0x00  0x8000 to 0x8002; older signatures limit the two counts below
    s16 mNumNodes;  // 0x02
    u8 mType;       // 0x04  4 makes TreeSim::Gosub start at the node in the first parameter
    u8 mNumParams;  // 0x05
    u8 mNumLocals;  // 0x06
    u8 field_0x7;   // 0x07  0 on every tree on the disc
    u8 field_0x8;   // 0x08
};

// Wii: IConsoleUnknown, the reference-counted base. A base with only a vptr puts it at offset 0,
// where a class with no base would put it last.
class IConsoleUnknown {
public:
    virtual void QueryInterface();
    virtual int AddRef();
    virtual int Release();
};

class ObjectDataID {
public:
    u32 field_0x0; // 0x00
    u32 field_0x4; // 0x04

    ObjectDataID(u32 resFile, u16 id);
    bool operator==(const ObjectDataID &other) const;
};

// Wii: ObjectDataBehaviorTree, one loaded tree. Virtual functions are in Wii vtable order as far
// as the interpreter reaches. The class is abstract; the resource and IFF forms derive from it.
class ObjectDataBehaviorTree : public IConsoleUnknown {
public:
    virtual void GetDataID();
    virtual int LoadFromDataID(const ObjectDataID &id);
    virtual void LoadFromIndex();
    virtual void SaveDataByID();
    virtual void GetResourceName();
    virtual void SetResourceName();
    virtual void GetDataSourceType();
    virtual void LoadOnlyNameAndIDFromIndex();
    virtual ~ObjectDataBehaviorTree();
    virtual int GetFromDataID(const ObjectDataID &id);

    u8 pad_0x4[0x50];
    BehaviorHeader *mHeader; // 0x54
    ObjectDataID mID;        // 0x58

    u8 GetType() const;
    s16 GetNumNodes() const;
    u8 GetNumParams() const;
    u8 GetNumLocals() const;
    BehaviorNode *GetNode(u32 index) const;
};

// Wii: ConsoleAutoRefCount, a smart pointer. It releases its object when it goes out of scope.
template <class T>
class ConsoleAutoRefCount {
public:
    T *mObject;

    ConsoleAutoRefCount(T *object = 0) : mObject(object) {}
    ConsoleAutoRefCount(const ConsoleAutoRefCount &other);
    ~ConsoleAutoRefCount()
    {
        if (mObject)
            mObject->Release();
    }
    // For a function that fills the pointer in: what it held is released first.
    T **AsPPTypeParam()
    {
        if (mObject) {
            mObject->Release();
            mObject = 0;
        }
        return &mObject;
    }
    T *operator->() const { return mObject; }
    void operator=(T *object)
    {
        if (object == mObject)
            return;
        if (object)
            object->AddRef();
        if (mObject)
            mObject->Release();
        mObject = object;
    }
};

class ISmartDataFactory {
public:
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual int GetData(u32 kind, const ObjectDataID &id, void *result);
};
ISmartDataFactory *GetSmartDataFactory();

class ObjSelector;
class ObjectDataBehaviorConstants;

// Finds trees for an object: its own (private), its group's (semi-global) and the global ones.
extern u32 sGlobNamespaceID;

class Behavior : public IConsoleUnknown {
public:
    virtual ~Behavior();
    virtual void Init();
    virtual u32 GetResFileID(s16 treeID);
    virtual void Slot6();
    virtual void Slot7();
    virtual s16 GetTreeIDByNameFast(const char *name);

    ObjSelector *mSelector; // 0x04

    static eastl::vector<ObjectDataBehaviorTree *, eastl::allocator> s_cachedTrees;

    int GetNode(s16 treeID, s16 node, BehaviorNode *result);
    int GetNodeRef(s16 treeID, s16 node, BehaviorNode **result);
    // The game has GetNode as a function and also inlines it.
    int GetNodeInline(s16 treeID, s16 node, BehaviorNode *result)
    {
        BehaviorNode *ref = 0;
        int found = GetNodeRef(treeID, node, &ref);
        if (found)
            *result = *ref;
        return found;
    }
    int GetTree(s16 treeID, ObjectDataBehaviorTree **result, bool load);
    u32 GetNamespaceID();
    u32 GetGlobNamespaceID();
    inline u32 GetNamespaceIDInline();
    inline u32 GetSemiGlobalNamespaceIDInline();
    // 0 a primitive, 1 global, 2 private, 3 semi-global.
    static s16 GetTreeClass(s16 id)
    {
        if ((u16)id < 0x100)
            return 0;
        if ((u16)(id - 0x100) < 0xf00)
            return 1;
        if ((u16)(id - 0x2000) < 0x329)
            return 3;
        if ((u16)(id - 0x1000) < 0x1000)
            return 2;
        return 0;
    }
    inline ObjectDataBehaviorConstants *GetConstants(s16 id, ObjectDataBehaviorConstants **constants);

    static bool IsDefaultParam(const s16 *param)
    {
        return ((const int *)param)[0] == -1 && ((const int *)param)[1] == -1;
    }
};

#endif
