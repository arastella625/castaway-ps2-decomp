#include "game/simulator/core/cXObject.h"

#include "common/EGlobal.h"
#include "common/EVec3.h"
#include "game/animation/SAnimator2.h"

#include "game/simulator/core/Lot.h"
#include "game/simulator/core/Neighborhood.h"
#include "game/simulator/core/cXMTObject.h"
#include "game/simulator/core2/ObjTestSim.h"
#include "game/simulator/core2/cXPerson.h"
#include "game/simulator/objectdata/XObjLang.h"

#define OBJECT_ERROR_CHECK(condition, code) TREE_CHECK(this, condition, code, return -1)

namespace InteractorModule {
class DirectInteractor {
public:
    u32 pad_0x0[0x23];
    int mAutonomous; // 0x8c  Wii: IsAutonomous
};

class InteractorManager {
public:
    static InteractorManager *GetSingleton();
    DirectInteractor *GetPlayerInteractor(int a, int type);
};
}

class AnimRef;
class PassiveInfluenceSystem {
public:
    void SetSkillNameID(cXObject *object, const AnimRef *anim);
    void ClearInfluenceMotiveDeltas(cXObject *object);
    void SetInfluenceMotiveDelta(cXObject *object, u8 motive, f32 delta, s16 limit, s8 type);
};

inline u32 Behavior::GetNamespaceIDInline()
{
    return mSelector->mNamespaceID;
}

inline u32 Behavior::GetSemiGlobalNamespaceIDInline()
{
    return mSelector->mSemiNamespaceID;
}

// Primitive 0xf, TreeBreak.
struct TreeBreakParam {
    s16 mData;  // 0x00
    s16 mScope; // 0x02
    u16 mFlags; // 0x04  bit 0 log only, bit 1 stop
};

// A class in memory, not a plain s16: the game keeps it in the slot FindGoodLocation's tile uses.
class TreeBreakValue {
public:
    s16 mValue;

    TreeBreakValue() {}
    TreeBreakValue(const TreeBreakValue &other);
};

// Primitive 0: wait. The counter is a parameter of this tree, numbered by the first operand.
struct IdleParam {
    s16 mParam; // 0x00
};

long cXObject::TryIdle(StackElem *elem, XPrimParam *primParam)
{
    IdleParam *param = (IdleParam *)primParam;
    TREE_CHECK(this, param->mParam >= 0 && param->mParam < elem->mNumParams, 8, return -1);
    s16 *params = elem->GetParams();
    s16 *counter = &params[param->mParam];
    if (GetIdleStatus() == 0) {
        *counter = 0;
        SetIdleStatus(-1);
    }
    if (*counter < 0)
        *counter = 0;
    if (*counter == 0)
        return 1;
    if (!AllowIdleOptimization()) {
        --*counter;
        return 2;
    }
    SetIdleStatus(*counter - 1);
    return 2;
}

// Wii: GetNextRandomNumber, and SRand, which the PS2 build inlines.
u32 GetNextRandomNumber();

inline u32 SRand(u32 limit)
{
    if (limit != 0)
        return GetNextRandomNumber() % limit;
    return 0;
}

// Primitive 8: a random number below a limit, into an operand.
struct RandomParam {
    s16 mData;       // 0x00
    u16 mScope;      // 0x02
    u16 mLimitData;  // 0x04
    u16 mLimitScope; // 0x06
};

long cXObject::TryRandom(StackElem *elem, XPrimParam *primParam)
{
    RandomParam param = *(RandomParam *)primParam;
    const s16 *ref;
    f32 *floatRef;
    s16 limit;

    if (InterpValue((s16)param.mScope, param.mData, &ref, &floatRef, 0, 0) == -1)
        return -1;
    if (InterpValue((s16)param.mLimitScope, (s16)param.mLimitData, 0, 0, &limit, 0) == -1)
        return -1;
    TREE_CHECK(this, limit, 4, return -1);
    if (ref) {
        // Read as a u16 and narrowed, which is why the game loads the limit a second time here.
        InterpWriteValue((s16)param.mScope, param.mData, &ref, SRand((s16)*(u16 *)&limit));
        return 1;
    }
    if (floatRef) {
        *floatRef = (int)SRand(limit);
    } else {
        TREE_ERROR(0x13);
        return -1;
    }
    return 1;
}

#ifdef NON_MATCHING
// 410 of the 411 words line up. One branch to the bad mode error goes to the error's alignment
// nop in the game and just past it here. The delay slot is the same, so it behaves the same.
// Primitive 0x1c: run a tree found by name.
class CallNamedTreeParam {
public:
    s16 mStringSet; // 0x00
    s16 mWhere;     // 0x02  1 global, 2 semi-global, 3 private, others by the running tree's class
    u8 mString;     // 0x04
    u8 mMode;       // 0x05  0 and 1 run it to the end, on me or the stack object; 2 a subroutine

    bool GetNameGlobal() const { return mWhere == 1; }
    bool GetNameSemiGlobal() const { return mWhere == 2; }
    bool GetNamePrivate() const { return mWhere == 3; }
};

long cXObject::TryCallNamedTree(StackElem *elem, XPrimParam *primParam)
{
    CallNamedTreeParam *param = (CallNamedTreeParam *)primParam;
    if (elem->mWaiting == 1) {
        if (GetLastResult())
            return 1;
        return 0;
    }

    u32 first, second;
    second = first = 0xffffffff;
    if (param->GetNameGlobal()) {
        first = sGlobNamespaceID;
    } else if (param->GetNameSemiGlobal()) {
        first = elem->mBehavior->GetSemiGlobalNamespaceIDInline();
    } else if (param->GetNamePrivate()) {
        first = elem->mBehavior->GetNamespaceIDInline();
    } else {
        s16 treeClass = Behavior::GetTreeClass(elem->GetTreeID());
        if (treeClass == 2) {
            first = elem->mBehavior->GetNamespaceIDInline();
            second = elem->mBehavior->GetSemiGlobalNamespaceIDInline();
        } else if (treeClass == 3) {
            first = elem->mBehavior->GetSemiGlobalNamespaceIDInline();
            second = elem->mBehavior->GetNamespaceIDInline();
        }
    }
    OBJECT_ERROR_CHECK(first != 0xffffffff, 0x2d);

    ConsoleAutoRefCount<StringSet> strings;
    strings = GetObjectFolder()->GetNamespaceSelector(first)->GetStringSet(param->mStringSet);
    const char *name = 0;
    if (strings.mObject)
        name = strings->GetString(param->mString, -1);
    if (!name || !*name) {
        if (second == 0xffffffff) {
            if (TreeSim::sBreakOnCallTreeStringFailure && !strings.mObject) {
                OBJECT_ERROR_CHECK(false, 0x2c);
            }
            return 0;
        }
        strings = GetObjectFolder()->GetNamespaceSelector(second)->GetStringSet(param->mStringSet);
        if (strings.mObject)
            name = strings->GetString(param->mString, -1);
        if (!name || !*name) {
            if (TreeSim::sBreakOnCallTreeStringFailure && !strings.mObject) {
                OBJECT_ERROR_CHECK(false, 0x2c);
            }
            return 0;
        }
    }

    cXObject *object = GetObjectFromID(elem->mStackObject);
    if (!object) {
        OBJECT_ERROR_CHECK(false, 0x15);
    }
    cXObject *runner;
    cXObject *target;
    bool toTheEnd = false;
    switch (param->mMode) {
    case 0:
        runner = this;
        target = GetObjectFromID(elem->mStackObject);
        toTheEnd = true;
        break;
    case 1:
        toTheEnd = true;
        target = GetObjectFromID(elem->mStackObject);
        runner = target;
        break;
    case 2:
        runner = this;
        target = GetObjectFromID(elem->mStackObject);
        break;
    default:
        OBJECT_ERROR_CHECK(false, 0x2d);
    }

    long result;
    Behavior *behavior = target->mSelector->mBehavior;
    s16 tree = behavior->GetTreeIDByNameFast(name);
    if (tree == 0) {
        result = 0;
    } else if (toTheEnd) {
        result = 0;
        if (runner->RunCheckTree(behavior, target->mID, tree, 0))
            result = 1;
    } else if (runner == this) {
        if (!runner->GosubObjectTree(target, 0, tree, false))
            return 0;
        elem->mWaiting = 1;
        result = 3;
    } else {
        result = 0;
    }
    return result;
}

#endif

// Wii: the object's TTAB. Only the slot PushAction calls.
class TreeTab {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void *GetEntry(int index);
};

// Primitive 0xd: put an interaction on a Sim's queue.
class PushActionParam {
public:
    u8 mInteraction; // 0x00
    u8 mObject;      // 0x01
    u8 mPriority;    // 0x02
    u8 mFlags;       // 0x03
    u8 mIconLocal;   // 0x04

    bool GetUseSpecialIcon() const { return mFlags & 1; }
    bool GetUseLocal() const { return (mFlags >> 1) & 1; }
    bool GetContinue() const { return (mFlags >> 2) & 1; }
    bool GetCarryNameOver() const { return (mFlags >> 3) & 1; }
    bool GetPushToHead() const { return (mFlags >> 4) & 1; }
    bool GetRunAsSub() const { return (mFlags >> 5) & 1; }
    bool GetUseTemp() const { return (mFlags >> 6) & 1; }
};

long cXObject::TryPushAction(StackElem *elem, XPrimParam *primParam)
{
    PushActionParam *param = (PushActionParam *)primParam;
    int index = param->mObject;
    s16 id;
    if (param->GetUseLocal()) {
        OBJECT_ERROR_CHECK(index >= 0 && index <= elem->mNumLocals, 0x34);
        id = elem->GetLocals()[index];
    } else {
        OBJECT_ERROR_CHECK(index >= 0 && index <= elem->mNumParams, 8);
        id = elem->GetParams()[index];
    }
    cXObject *object = GetObjectFromID(id);
    if (!object) {
        OBJECT_ERROR_CHECK(false, 0x17);
    }
    if (object->IsMultiTile())
        object = object->GetInteractionLeader();

    int interaction;
    if (param->GetUseTemp()) {
        OBJECT_ERROR_CHECK(param->mInteraction < 8, 2);
        interaction = mTemps[param->mInteraction];
    } else {
        interaction = param->mInteraction;
    }

    TreeTab *tab = object->mSelector->GetTreeTab();
    if (tab) {
        if (!tab->GetEntry(interaction)) {
            OBJECT_ERROR_CHECK(false, 0x16);
        }
    } else {
        OBJECT_ERROR_CHECK(false, 0x1f);
    }

    cXObject *stack = GetObjectFromID(elem->mStackObject);
    if (stack) {
        cXPerson *person = stack->GetPerson();
        if (!(stack->GetType() == 2 && person)) {
            OBJECT_ERROR_CHECK(false, 0x1c);
        }
        int priority = 1;
        switch (param->mPriority) {
        case 0:
            if (GetType() == 2) {
                cXPerson *me = GetPerson();
                if (me)
                    priority = me->mPersonData[0xe];
            }
            break;
        case 1:
            priority = 100;
            break;
        case 2:
            priority = 2;
            break;
        case 3:
            priority = 0x32;
            break;
        default:
            OBJECT_ERROR_CHECK(false, 0x2e);
        }
        Interaction action(person, object, interaction, priority);
        action.SetCarryNameOver(param->GetCarryNameOver());
        if (param->GetUseSpecialIcon()) {
            int local = param->mIconLocal;
            OBJECT_ERROR_CHECK(local >= 0 && local < elem->mNumLocals, 0x34);
            action.SetIconObject(GetObjectFromID(elem->GetLocals()[local]));
        }
        action.SetContinuation(param->GetContinue());
        action.SetPushToHead(param->GetPushToHead());
        action.SetRunAsSub(param->GetRunAsSub());
        if (action.GetContinuation()) {
            OBJECT_ERROR_CHECK(!person->IsIdle(), 0x4b);
        }
        if (person->AddAction(&action))
            return 1;
        return 0;
    }
    OBJECT_ERROR_CHECK(false, 0x15);
}

// The part of a multi-tile object that interactions belong to. Its definition's word 8 is the
// negative of what this part's is.
cXObject *cXObject::GetInteractionLeader()
{
    if (!IsInteractionLeader()) {
        s16 lead = mDef[8];
        lead = -lead;
        cXMTObject *object = GetMTObject();
        if (object) {
            cXMTObject *part = object->GetFirstMultiTileObject();
            while (part) {
                if (part->mDef[8] == lead)
                    return part;
                part = part->mNextPart;
            }
        }
    }
    return this;
}

#ifdef NON_MATCHING
// The logic is complete, from the Wii build, and 563 of the game's 732 words line up on a first
// pass. It's 25 words short: the neighbour lookups need checking case by case against the game.
extern int _gd;

// Wii: an object's or neighbour's relationships. Only the slots Relationship2 calls.
class RelMatrix {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual int GetCount(s16 key);
    virtual void SetCount(s16 key, int count);
    virtual void Slot5();
    virtual void Slot6();
    virtual s16 Get(s16 key, int field);
    virtual void Set(s16 key, int field, s16 value);
};

class H2DTarget {
public:
    void RelationshipChangeOccured(int change);
};

// Primitive 0x1a: read or write one field of a relationship.
class Relationship2Param {
public:
    u8 mField; // 0x00
    u8 mWho;   // 0x01  0 me to stack object, 1 stack object to me, 2 stack object to local, 3 local to stack object
    u8 mFlags; // 0x02  bit 0 don't create, 1 neighbour IDs, 2 assign, 3 pet ownership
    u8 mLocal; // 0x03
    u8 mScope; // 0x04
    u8 pad_0x5;
    s16 mData; // 0x06

    bool GetCreate() const { return (mFlags ^ 1) & 1; }
    bool GetUseNeighbors() const { return (mFlags >> 1) & 1; }
    bool GetAssign() const { return (mFlags >> 2) & 1; }
    bool GetPetOwnership() const { return (mFlags >> 3) & 1; }
};

// Wii: ObjectModule::RelationshipAccessed. An assignment marks both neighbours and their families.
inline void RelationshipAccessed(ObjectModule *module, Neighbor *a, Neighbor *b, bool assign)
{
    if (assign) {
        a->mDirty = 1;
        int *family = GetNeighborhood()->GetFamily(a->mPersonData[0x28]);
        if (family)
            family[9] = 1;
        b->mDirty = 1;
        family = GetNeighborhood()->GetFamily(b->mPersonData[0x28]);
        if (family)
            family[9] = 1;
        module->mRelationsDirty = 1;
    }
}

long cXObject::TryRelationship2(StackElem *elem, XPrimParam *primParam)
{
    Relationship2Param *param = (Relationship2Param *)primParam;
    Neighborhood *neighborhood = GetNeighborhood();
    Neighbor *from = 0;
    RelMatrix *matrix;
    s16 key;

    if (param->GetPetOwnership()) {
        cXObject *owner = 0;
        cXObject *other = 0;
        Neighbor *neighbor = 0;
        switch (param->mWho) {
        case 0:
            owner = this;
            if (param->GetUseNeighbors())
                neighbor = neighborhood->FindNeighborByIDInline(elem->mStackObject);
            else
                other = GetObjectFromID(elem->mStackObject);
            break;
        case 1:
            if (param->GetUseNeighbors()) {
                OBJECT_ERROR_CHECK(false, 0x5b);
            }
            owner = GetObjectFromID(elem->mStackObject);
            neighbor = neighborhood->FindNeighborByIDInline(mID);
            break;
        case 2:
            OBJECT_ERROR_CHECK(param->mLocal < elem->mNumLocals, 0x34);
            owner = GetObjectFromID(elem->mStackObject);
            if (param->GetUseNeighbors())
                neighbor = neighborhood->FindNeighborByIDInline(elem->GetLocals()[param->mLocal]);
            else
                other = GetObjectFromID(elem->GetLocals()[param->mLocal]);
            break;
        case 3:
            OBJECT_ERROR_CHECK(param->mLocal < elem->mNumLocals, 0x34);
            owner = GetObjectFromID(elem->GetLocals()[param->mLocal]);
            if (param->GetUseNeighbors())
                neighbor = neighborhood->FindNeighborByIDInline(elem->mStackObject);
            else
                other = GetObjectFromID(elem->mStackObject);
            break;
        }
        if (!param->GetUseNeighbors()) {
            if (!(other && other->GetPerson())) {
                OBJECT_ERROR_CHECK(false, 0x5c);
            }
            neighbor = other->GetPerson()->GetNeighbor();
        }
        if (!(owner && neighbor)) {
            OBJECT_ERROR_CHECK(false, 0x15);
        }
        matrix = owner->mRelMatrix;
        key = neighbor->GetID();
    } else if (param->GetUseNeighbors()) {
        Neighbor *to = 0;
        switch (param->mWho) {
        case 0: {
            cXPerson *person = GetPerson();
            OBJECT_ERROR_CHECK(person, 0x1c);
            from = neighborhood->FindNeighborByIDInline(person->GetNeighborID());
            to = neighborhood->FindNeighborByIDInline(elem->mStackObject);
            break;
        }
        case 1: {
            cXPerson *person = GetPerson();
            OBJECT_ERROR_CHECK(person, 0x1c);
            from = neighborhood->FindNeighborByIDInline(elem->mStackObject);
            to = neighborhood->FindNeighborByIDInline(person->GetNeighborID());
            break;
        }
        case 2:
            OBJECT_ERROR_CHECK(param->mLocal < elem->mNumLocals, 0x34);
            from = neighborhood->FindNeighborByIDInline(elem->mStackObject);
            to = neighborhood->FindNeighborByIDInline(elem->GetLocals()[param->mLocal]);
            break;
        case 3:
            OBJECT_ERROR_CHECK(param->mLocal < elem->mNumLocals, 0x34);
            from = neighborhood->FindNeighborByIDInline(elem->GetLocals()[param->mLocal]);
            to = neighborhood->FindNeighborByIDInline(elem->mStackObject);
            break;
        }
        if (!(from && to)) {
            OBJECT_ERROR_CHECK(false, 0x15);
        }
        OBJECT_ERROR_CHECK(param->mField != 2, 0x56);
        matrix = from->mRelations;
        key = to->GetID();
        RelationshipAccessed(mModule, from, to, param->GetAssign());
    } else {
        cXObject *first = 0;
        cXObject *second = 0;
        switch (param->mWho) {
        case 0:
            first = this;
            second = GetObjectFromID(elem->mStackObject);
            break;
        case 1:
            second = this;
            first = GetObjectFromID(elem->mStackObject);
            break;
        case 2:
            OBJECT_ERROR_CHECK(param->mLocal < elem->mNumLocals, 0x34);
            second = GetObjectFromID(elem->GetLocals()[param->mLocal]);
            first = GetObjectFromID(elem->mStackObject);
            break;
        case 3:
            OBJECT_ERROR_CHECK(param->mLocal < elem->mNumLocals, 0x34);
            first = GetObjectFromID(elem->GetLocals()[param->mLocal]);
            second = GetObjectFromID(elem->mStackObject);
            break;
        }
        if (!(first && second)) {
            OBJECT_ERROR_CHECK(false, 0x15);
        }
        matrix = first->mRelMatrix;
        key = second->mID;
        if (first->GetType() == 2 && second->GetType() == 2) {
            from = neighborhood->FindNeighborByIDInline(first->GetPerson()->GetNeighborID());
            matrix = from->mRelations;
            key = second->GetPerson()->GetNeighborID();
            OBJECT_ERROR_CHECK(param->mField != 2, 0x56);
            RelationshipAccessed(mModule, from, neighborhood->FindNeighborByIDInline(key), param->GetAssign());
        }
    }

    if (matrix->GetCount(key) <= param->mField) {
        if (!param->GetCreate())
            return 0;
        matrix->SetCount(key, param->mField + 1);
        if (matrix->GetCount(key) <= param->mField)
            return 1;
    }
    const s16 *ref;
    s16 value;
    if (InterpValue(param->mScope, param->mData, &ref, 0, &value, 0) == -1)
        return -1;
    if (param->GetAssign()) {
        s16 before = matrix->Get(key, param->mField);
        matrix->Set(key, param->mField, value);
        if (_globals.mPlayerPerson && from && from == _globals.mPlayerPerson->GetNeighbor()) {
            H2DTarget *target = *(H2DTarget **)((char *)&_gd + 0x54);
            if (target && before)
                target->RelationshipChangeOccured(value - before);
        }
        return 1;
    }
    OBJECT_ERROR_CHECK(ref, 0x13);
    InterpWriteValue(param->mScope, param->mData, &ref, matrix->Get(key, param->mField));
    return 1;
}

#endif

// Primitive 0x12: kill me, the stack object, or nothing.
class KillObjectParam {
public:
    s16 mWho;  // 0x00  0 me, 1 the stack object, 2 the ID the caller left in killed
    u8 mFlags; // 0x02  bit 0 return immediately, 1 don't clean up everything

    bool GetReturnImmediately() const { return mFlags & 1; }
    bool GetCleanupAll() const { return ((mFlags >> 1) ^ 1) & 1; }
};

long cXObject::TryKillObject(StackElem *elem, XPrimParam *primParam, s16 *killed)
{
    KillObjectParam *param = (KillObjectParam *)primParam;
    if (elem->mWaiting == 1)
        return 1;
    OBJECT_ERROR_CHECK(param->mWho >= 0 && param->mWho <= 2, 9);
    switch (param->mWho) {
    case 0:
        *killed = mID;
        break;
    case 1:
        *killed = elem->mStackObject;
        break;
    case 2:
        *killed = 0;
        break;
    }
    cXObject *object = GetObjectFromID(*killed);
    if (object) {
        mModule->AddToKillQueue(*killed, param->GetCleanupAll());
        cXPerson *person = object->GetPerson();
        if (person)
            GetNeighborhood()->SavePersistentData(person);
        bool self = false;
        if (*killed == mID && TreeSim::sInMainSim)
            self = true;
        if (self && !param->GetReturnImmediately()) {
            elem->mWaiting = 1;
            return 2;
        }
        return 1;
    }
    return 0;
}

// Primitive 0x31: wake the stack object if its main tree is waiting.
long cXObject::TryNotifyStackObject(StackElem *elem, XPrimParam *primParam)
{
    cXObject *stack = GetObjectFromID(elem->mStackObject);
    OBJECT_ERROR_CHECK(stack, 0x15);
    StackElem *frame = stack->GetMainSimElem();
    if (frame) {
        BehaviorNode node;
        if (frame->mBehavior->GetNodeInline(frame->GetTreeID(), frame->mNodeIndex, &node)) {
            s16 prim = node.GetPrimCode();
            IdleParam *idle = (IdleParam *)node.mParams;
            if (prim == 0) {
                OBJECT_ERROR_CHECK(idle->mParam >= 0 && idle->mParam < frame->mNumParams, 8);
                frame->GetParams()[idle->mParam] = 0;
                stack->SetIdleStatus(0);
            } else if (prim == 0x11) {
                OBJECT_ERROR_CHECK(idle->mParam >= 0 && idle->mParam < frame->mNumParams, 8);
                frame->GetParams()[idle->mParam] = 0;
            } else if (prim == 0x40) {
                cXPerson *person = stack->GetPerson();
                OBJECT_ERROR_CHECK(person, 0x1c);
                person->mIdleLoopCount = 0;
            }
        }
    }
    return 1;
}

long cXObject::TrySetMotiveDelta(StackElem *elem, XPrimParam *primParam)
{
    SetMotiveDeltaParam *param = (SetMotiveDeltaParam *)primParam;
    if (!param->IsInfluence())
        return -1;
    if (param->GetClearAll()) {
        cXObject *object = 0;
        if (param->IsInfluenceOnMe())
            object = this;
        else if (param->IsInfluenceOnStackObject())
            object = GetObjectModule()->GetObjectFromIDInline(elem->mStackObject);
        GetSimulator()->mPassiveInfluence->ClearInfluenceMotiveDeltas(object);
        return 1;
    }
    switch (param->mMotive) {
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 12:
    case 14:
    case 15:
        break;
    default:
        TREE_ERROR(0x1a);
        return -1;
    }
    s16 delta;
    s16 limit;
    if (InterpValue(param->mDeltaScope, param->mDeltaData, 0, 0, &delta, 0) == -1)
        return -1;
    if (InterpValue(param->mLimitScope, param->mLimitData, 0, 0, &limit, 0) == -1)
        return -1;
    f32 perTick = delta * 0.00055555562f;
    cXObject *object = 0;
    if (param->IsInfluenceOnMe())
        object = this;
    else if (param->IsInfluenceOnStackObject())
        object = GetObjectModule()->GetObjectFromIDInline(elem->mStackObject);
    s8 type = param->GetFlagsInTypedMotiveIncFormat();
    if (!type)
        type = 1;
    GetSimulator()->mPassiveInfluence->SetInfluenceMotiveDelta(object, param->mMotive, perTick, limit, type);
    return 1;
}

#ifdef NON_MATCHING
// 162 of the 164 words line up. The two that differ are swapped: the game stores 0 into the
// AUTOPTR before calling DestroyInstance, and here that store is in the call's delay slot.
// Primitive 0x32: the words a tree shows on a menu, or above a Sim.
class MakeActionStringParam {
public:
    s16 mStringSet; // 0x00
    u16 mFlags;     // 0x02  bit 0 the string comes from an operand
    u8 mString;     // 0x04
    u8 mScope;      // 0x05
    s16 mData;      // 0x06

    bool GetUseOwnerCombo() const { return mFlags & 1; }
};

long cXObject::TryMakeActionString(StackElem *elem, XPrimParam *primParam)
{
    MakeActionStringParam *param = (MakeActionStringParam *)primParam;
    int menu = ObjTestSim::IsMenuInProgress();
    int mainSim = TreeSim::sInMainSim;
    if (!menu && !mainSim)
        return 1;

    s16 index;
    if (param->GetUseOwnerCombo()) {
        if (InterpValue(param->mScope, param->mData, 0, 0, &index, 0) == -1)
            return -1;
        index++;
    } else {
        index = param->mString;
    }

    AUTOPTR<StringSet> strings;
    strings.CreateInstance();
    strings->Load(elem->mBehavior->GetNamespaceIDInline(), param->mStringSet);
    s16 wanted = index;
    if (!(wanted >= 0 && strings->GetStringCount(-1) >= wanted)) {
        OBJECT_ERROR_CHECK(false, 0x62);
    }

    BString2 text;
    ELocString loc = strings->GetLocString(index);
    if ((const u16 *)loc)
        text.assign((const u16 *)loc);

    s16 object[4];
    memset(object, 0, sizeof(object));
    ObjSelector *selector;
    ParseUIString(text, elem, object, &selector, false);
    if (menu) {
        if (object[0] == 0) {
            if (elem)
                object[0] = elem->mStackObject;
            else
                object[0] = 0;
        }
        ObjTestSim::MakeNewMenuItem(text.c_str(), object, elem, mVars[0x32]);
    } else if (mainSim) {
        cXPerson *person = GetPerson();
        if (person)
            person->SetCurrentActionText(text);
    }
    return 1;
}
#endif

// A whole tile: x, y, level. Built from numbers, it turns anything that doesn't fit in a byte to 0.
class CTilePt {
public:
    s8 mX;     // 0x00
    s8 mY;     // 0x01
    s8 mLevel; // 0x02

    CTilePt() { mLevel = 0; }
    CTilePt(const CTilePt &other)
    {
        mX = other.mX;
        mY = other.mY;
        mLevel = other.mLevel;
    }
    CTilePt(int x, int y, int level)
    {
        mX = x;
        mY = y;
        if (mX != x)
            mX = 0;
        if (mY != y)
            mY = 0;
        mLevel = level;
    }
    // Wii: CTilePt(const FTilePt &, int), which doesn't check.
    CTilePt(const cXObject *object, int level)
    {
        mX = object->mTileX >> 4;
        mY = object->mTileY >> 4;
        mLevel = level;
    }
    ~CTilePt() {}
    CTilePt &operator=(const CTilePt &other)
    {
        mX = other.mX;
        mY = other.mY;
        mLevel = other.mLevel;
        return *this;
    }
    CTilePt &operator+=(const CTilePt &other)
    {
        mX += other.mX;
        mY += other.mY;
        return *this;
    }
    CTilePt operator-(const CTilePt &other) const { return CTilePt(mX - other.mX, mY - other.mY, mLevel); }
    void Set(int x, int y)
    {
        mX = x;
        mY = y;
    }
    void Set(int x, int y, int level)
    {
        mX = x;
        mY = y;
        mLevel = level;
    }
    int GetX() const { return mX; }
    int GetY() const { return mY; }
    int GetLevel() const { return mLevel; }
};

inline CTilePt cXObject::GetCTilePt() const
{
    return CTilePt(mTileX >> 4, mTileY >> 4, mLevel);
}

inline s16 GetTileObjectID(ObjectModule *module, const CTilePt &tile)
{
    int x = tile.GetX();
    int y = tile.GetY();
    if (x >= 0x40 || y >= 0x40 || x < 0 || y < 0)
        return 0;
    return module->mTileObjects[y][x];
}

class cFixedWorld {
public:
    int OutOfGrid(const CTilePt &tile) const;
};
extern cFixedWorld *gFixedWorld;

// Walks the objects on a tile.
class ObjectIterator {
public:
    enum IterateType { kAll };

    u32 field_0x0;     // 0x00
    cXObject *mObject; // 0x04  0 at the end
    u32 field_0x8;     // 0x08

    ObjectIterator(const CTilePt &tile, IterateType type);
    ObjectIterator &operator++();
    cXObject *operator*() const { return mObject; }
    bool finished() const { return mObject == 0 ? true : false; }
};

// Primitive 0x1f: step a value through a list of things.
class SetToNextParam {
public:
    int mGUID;   // 0x00
    u8 mFlags;   // 0x04  low 4 bits the search type, 5 GUID from temps, 6 skip non-leads, 7 target
    u8 mOwner;   // 0x05
    u8 mLocal;   // 0x06
    u8 mData;    // 0x07

    int GetTargetOwner() const { if (mFlags & 0x80) return mOwner; return 0xa; }
    int GetTargetData() const { if (mFlags & 0x80) return mData; return 0; }
    s16 GetSearchType() const { return mFlags & 0xf; }
    bool GetSkipNonLeads() const { return (mFlags >> 6) & 1; }
    bool GetGuidFromTemps() const { return (mFlags >> 5) & 1; }
};

long cXObject::TrySetToNext(StackElem *elem, XPrimParam *primParam)
{
    SetToNextParam *param = (SetToNextParam *)primParam;
    const s16 *ref;
    cXObject *object;

    if (InterpValue(param->GetTargetOwner(), param->GetTargetData(), &ref, 0, 0, 0) == -1)
        return -1;
    OBJECT_ERROR_CHECK(ref, 0x13);
    s16 type = param->GetSearchType();
    bool skip = param->GetSkipNonLeads();
    if (type == 12)
        skip = false;
    switch (type) {
    case 3: {
        // The parts of the multi-tile object holding the target.
        object = GetObjectFromID(*ref);
        if (object) {
            if (object->IsMultiTile()) {
                cXMTObject *part = object->GetMTObject();
                if (skip) {
                    cXMTObject *start = part;
                    do {
                        if (part->mNextPart)
                            part = part->mNextPart;
                        else
                            part = part->mFirstPart;
                        if (part == start)
                            return 0;
                    } while (!part->IsInteractionTile(false));
                } else {
                    if (part->mNextPart)
                        part = part->mNextPart;
                    else
                        part = part->mFirstPart;
                }
                if (!part) {
                    InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, 0);
                    return 0;
                }
                InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, part->GetID());
                return 1;
            }
            InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, object->GetID());
            return 1;
        }
        InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, 0);
        return 0;
    }
    case 5: {
        Neighborhood *neighborhood = GetNeighborhood();
        InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, neighborhood->GetNextNeighborID(*ref));
        if (*ref)
            return 1;
        return 0;
    }
    case 7: {
        Neighborhood *neighborhood = GetNeighborhood();
        InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, neighborhood->GetNextNeighborID(*ref));
        while (*ref != 0) {
            Neighbor *neighbor = neighborhood->FindNeighborByIDInline(*ref);
            if (neighbor) {
                int guid = neighbor->mSelector->GetGUID();
                if (param->GetGuidFromTemps() && (mTemps[1] << 16 | (u16)mTemps[2]) == param->mGUID)
                    return 1;
                if (guid == param->mGUID)
                    return 1;
            }
            InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, neighborhood->GetNextNeighborID(*ref));
        }
        return 0;
    }
    case 8: {
        // The objects on the target's tile, wrapping round.
        if (*ref == 0)
            return 0;
        object = GetObjectFromID(*ref);
        OBJECT_ERROR_CHECK(object, 0x33);
        if (!object->IsInWorld())
            return 0;
        CTilePt tile(object, object->mLevel);
        int found = 0;
        ObjectIterator first(tile, ObjectIterator::kAll);
        ObjectIterator it(first);
        while (!it.finished()) {
            if ((*it)->GetID() == *ref) {
                found = 1;
                break;
            }
            ++it;
        }
        if (!found) {
            it = first;
            while (!it.finished()) {
                GetObjectFromID((*it)->GetID());
                ++it;
            }
            return 0;
        }
        if (skip) {
            do {
                ++it;
                if (it.finished())
                    it = first;
                if ((*it)->GetID() == *ref)
                    return 0;
            } while (!(*it)->IsInteractionTile(true));
        } else {
            ++it;
            if (it.finished())
                it = first;
        }
        InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, (*it)->GetID());
        return 1;
    }
    case 9: {
        // The four tiles around the object in a local.
        object = GetObjectFromID(*ref);
        int local = param->mLocal;
        OBJECT_ERROR_CHECK(local >= 0 && local < elem->mNumLocals, 8);
        cXObject *centre = GetObjectFromID(elem->GetLocals()[local]);
        OBJECT_ERROR_CHECK(centre, 0xa);
        CTilePt centreTile = centre->GetCTilePt();
        CTilePt offset;
        int first;
        if (object) {
            first = 0;
            offset = object->GetCTilePt() - centreTile;
        } else {
            first = 1;
            offset.Set(1, 0, centreTile.GetLevel());
        }
        for (;;) {
            if (!first) {
                if (offset.GetX() > 0) {
                    offset.Set(0, 1);
                } else if (offset.GetX() < 0) {
                    offset.Set(0, -1);
                } else if (offset.GetY() > 0) {
                    offset.Set(-1, 0);
                } else {
                    return 0;
                }
            }
            first = 0;
            CTilePt next(centreTile);
            next += offset;
            if (gFixedWorld->OutOfGrid(next))
                continue;
            object = GetObjectFromID(GetTileObjectID(GetObjectModule(), next));
            bool found = false;
            if (skip) {
                if (object && object->GetRoom() == centre->GetRoom() && object->IsInteractionTile(true))
                    found = true;
            } else {
                found = false;
                if (object && object->GetRoom() == centre->GetRoom())
                    found = true;
            }
            if (found) {
                InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, object->GetID());
                return 1;
            }
        }
    }
    case 1: {
        // The Sims.
        int count = mModule->GetNumPeople();
        int index = 0;
        if (*ref != 0 && count > 0) {
            do {
                if (((cXObject *)mModule->GetPeople(index))->GetID() == *ref) {
                    index++;
                    break;
                }
                index++;
            } while (index < count);
        }
        if (index == count) {
            InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, 0);
        } else {
            InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, ((cXObject *)mModule->GetPeople(index))->GetID());
        }
        if (*ref)
            return 1;
        return 0;
    }
    case 4: {
        // Objects of one type, by position.
        ObjSelector *selector;
        if (param->GetGuidFromTemps())
            selector = GetObjectFolder()->GetSelectorByGUID(mTemps[1] << 16 | (u16)mTemps[2]);
        else
            selector = GetObjectFolder()->GetSelectorByGUID(param->mGUID);
        int count = mModule->GetNumObjects();
        int id = *ref;
        int index = 0;
        if ((unsigned int)(id - 1) < (unsigned int)count)
            index = id;
        for (; index < count; index++) {
            cXObject *candidate = mModule->GetObject(index);
            if (skip) {
                if (candidate && candidate->mSelector == selector && candidate->IsInteractionTile(true))
                    break;
            } else if (candidate && candidate->mSelector == selector) {
                break;
            }
        }
        if (index >= count) {
            InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, 0);
        } else {
            InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, index + 1);
        }
        if (*ref)
            return 1;
        return 0;
    }
    case 11: {
        // Lots in the neighbourhood.
        Neighborhood *neighborhood = GetNeighborhood();
        int count = neighborhood->GetNumNeighborHouses();
        int index = 0;
        int house = 0;
        if (*ref != 0) {
            for (; index < count; index++) {
                house = neighborhood->GetNeighborHouseByIndex(index);
                if (house == *ref) {
                    index++;
                    if (index >= count)
                        index = 0;
                    house = neighborhood->GetNeighborHouseByIndex(index);
                    break;
                }
            }
        } else {
            house = neighborhood->GetNeighborHouseByIndex(0);
        }
        if (index == count)
            return 0;
        InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, house);
        return 1;
    }
    case 10:
        return 0;
    default: {
        // The object list.
        if (*ref != 0) {
            object = GetObjectFromID(*ref);
            if (!object)
                return 0;
            object = object->GetNext();
        } else {
            object = mModule->GetFirst();
        }
        if (object) {
            // The gotos and the check with a false condition aren't style. Nothing may run off
            // the end of this switch, or the compiler carries what it knows about object into
            // the else branch and drops the null test the game has in case 2.
            if (skip) {
                switch (type) {
                case 0:
                    while (object && !object->IsInteractionTile(true))
                        object = object->GetNext();
                    goto walked;
                case 2:
                    while (object && (object->GetType() == 2 || !object->IsInteractionTile(true)))
                        object = object->GetNext();
                    goto walked;
                case 6: {
                    s16 value = elem->GetParams()[0];
                    while (object && (object->mVars[0x3b] != value || !object->IsInteractionTile(true)))
                        object = object->GetNext();
                    goto walked;
                }
                default:
                    OBJECT_ERROR_CHECK(false, 0x1e);
                }
            } else {
                switch (type) {
                case 0:
                    break;
                case 2:
                    while (object) {
                        if (object->GetType() != 2)
                            break;
                        object = object->GetNext();
                    }
                    break;
                case 6: {
                    s16 value = elem->GetParams()[0];
                    while (object && object->mVars[0x3b] != value)
                        object = object->GetNext();
                    break;
                }
                case 12:
                    while (object && !object->IsInWorld())
                        object = object->GetNext();
                    break;
                default:
                    OBJECT_ERROR_CHECK(false, 0x1e);
                }
            }
        }
        break;
    }
    }
walked:
    if (object) {
        InterpWriteValue(param->GetTargetOwner(), param->GetTargetData(), &ref, object->GetID());
        return 1;
    }
    return 0;
}

// Primitive 0x35: questions about the object in an operand.
struct CheckObjectParam {
    s16 mData;  // 0x00
    s16 mScope; // 0x02
    s16 mTest;  // 0x04
};

long cXObject::TryCheckObject(StackElem *elem, XPrimParam *primParam)
{
    CheckObjectParam *param = (CheckObjectParam *)primParam;
    s16 id = -1;

    if (InterpValue(param->mScope, param->mData, 0, 0, &id, 0) == -1)
        return -1;
    switch (param->mTest) {
    case 0: {
        // The player's Sim, under direct control.
        cXObject *object = GetObjectFromID(id);
        if (!object)
            return 0;
        cXPerson *person = object->GetPerson();
        if (!person)
            return 0;
        InteractorModule::InteractorManager *manager = InteractorModule::InteractorManager::GetSingleton();
        if (_globals.mPlayerPerson == person) {
            InteractorModule::DirectInteractor *interactor = manager->GetPlayerInteractor(0, 0);
            if (interactor) {
                if (!interactor->mAutonomous)
                    return 1;
            }
        }
        break;
    }
    case 1:
        if (GetObjectFromID(id))
            return 1;
        break;
    case 2: {
        cXObject *object = GetObjectFromID(id);
        if (object) {
            if (object->IsPerson())
                return 1;
        }
        break;
    }
    case 3:
        if (GetNeighborhood()->FindNeighborByIDInline(id))
            return 1;
        break;
    case 4: {
        // How many relationships of the type in temp 0 the Sim has, back into temp 0.
        s16 count = 0;
        OBJECT_ERROR_CHECK(mTemps[0] > 0 && mTemps[0] < 10, 0x14);
        s16 type = mTemps[0];
        cXObject *object = GetObjectFromID(id);
        if (object) {
            cXPerson *person = object->GetPerson();
            if (person) {
                s16 neighbor = person->GetNeighborID();
                count = GetNeighborhood()->FindNeighborByIDInline(neighbor)->CountRelationshipsOfType(type, false);
            }
        }
        mTemps[0] = count;
        return 1;
    }
    case 5: {
        // The player's Sim with no interactor.
        cXObject *object = GetObjectFromID(id);
        if (!object)
            return 0;
        cXPerson *person = object->GetPerson();
        if (!person)
            return 0;
        InteractorModule::InteractorManager *manager = InteractorModule::InteractorManager::GetSingleton();
        if (_globals.mPlayerPerson == person) {
            if (!manager->GetPlayerInteractor(0, 0))
                return 1;
        }
        break;
    }
    }
    return 0;
}

// Primitive 0x20: is the object in an operand of a given type?
struct TestObjectTypeParam {
    int mGUID;  // 0x00
    s16 mData;  // 0x04
    u8 mScope;  // 0x06
    u8 mFlags;  // 0x07  bit 0 same semi-global group, bit 1 store the GUID, bit 2 GUID from temps
};

long cXObject::TryTestObjectType(StackElem *elem, XPrimParam *primParam)
{
    TestObjectTypeParam *param = (TestObjectTypeParam *)primParam;
    s16 id;
    int guid = param->mGUID;

    if (InterpValue(param->mScope, param->mData, 0, 0, &id, 0) == -1)
        return -1;
    cXObject *object = GetObjectFromID(id);
    TREE_CHECK(this, object, 0x17, return -1);
    ObjSelector *selector = object->mSelector;
    if (!selector)
        return 0;
    u8 flags = param->mFlags;
    if (flags & 4) {
        int wanted = (mTemps[1] << 16) | (u16)mTemps[2];
        if (selector->GetGUID() == wanted)
            return 1;
    } else if (flags & 1) {
        ObjSelector *other = GetObjectFolder()->GetSelectorByGUID(guid);
        if (other) {
            if (selector->mSemiNamespaceID == other->mSemiNamespaceID)
                return 1;
        }
    } else if (flags & 2) {
        mTemps[2] = guid;
        mTemps[1] = guid >> 16;
        return 1;
    } else {
        if (selector->GetGUID() == guid)
            return 1;
        if (selector->GetMasterSelector()->GetGUID() == guid)
            return 1;
    }
    return 0;
}

// Wii: resolveSkillForPassiveInfluence.
long resolveSkillForPassiveInfluence(cXObject *object, StackElem *elem, const AnimateNewParam *param,
                                     const AnimRef *&anim, bool &a);

// Primitive 0x2c on an object that isn't a Sim. A Sim's animation is SAnimator2::TryAnimate;
// this only picks me or the stack object and records the skill for passive influence.
long cXObject::TryAnimate(StackElem *elem, const AnimateNewParam *param)
{
    const AnimRef *anim = 0;
    int behavior = param->GetBehavior();
    cXObject *object = 0;
    if (behavior == 7) {
        object = this;
    } else if (behavior == 8) {
        object = GetObjectModule()->GetObjectFromIDInline(elem->mStackObject);
    } else {
        OBJECT_ERROR_CHECK(false, 5);
    }
    bool skill = 0;
    long result = resolveSkillForPassiveInfluence(object, elem, param, anim, skill);
    if (object)
        GetSimulator()->mPassiveInfluence->SetSkillNameID(object, anim);
    return result;
}

// Primitive 0x3d. Wii keeps this as a function; PS2 inlines it.
inline void cXObject::TryParticleEffect(ParticleEffectParam effect, long &result)
{
    cXPerson *person = GetPerson();
    TREE_CHECK(this, person, 0x1c, result = -1; return);
    int played = person->mAnimator->PlayEffect(effect.field_0x0, effect.field_0x4);
    result = 1;
    if (played != 1)
        result = 0;
}

// The switch over primitive numbers. Cases are in the order of the game's code; the source order
// sets the order of the blocks, and the numbers show which are together. cXPerson::TryElement
// handles the Sim-only primitives and passes the others here.
// TryElement's local for TryFindGoodLocation. Four bytes with a copy constructor, not an FTilePt:
// that keeps the stack slots shared as they are in the game.
class OpaqueTile {
public:
    u32 field_0x0;

    OpaqueTile() {}
    OpaqueTile(const OpaqueTile &other);
};

long cXObject::TryElement(StackElem *elem, s16 opcode, XPrimParam *param)
{
    long result;

    switch (opcode) {
    case 0x20:
        result = TryTestObjectType(elem, param);
        break;
    case 0x13:
        result = TryIsPersonInArea(elem, param);
        break;
    case 0x10: {
        OpaqueTile tile;
        s16 slot;
        TFGL_ACTIONS action;
        int found;
        result = TryFindGoodLocation(elem, param, &action, (FTilePt *)&tile, &found, &slot);
        break;
    }
    case 0x1a:
        result = TryRelationship2(elem, param);
        break;
    case 0x29:
        result = TrySetBalloon(elem, param);
        break;
    case 0x1c:
        result = TryCallNamedTree(elem, param);
        break;
    case 0x32:
        result = TryMakeActionString(elem, param);
        break;
    case 0x01:
        result = TryGenericSimCall(elem, param);
        break;
    case 0x24:
        result = TryDialog(elem, param, false);
        break;
    case 0x2e:
        result = TrySnap(elem, param);
        break;
    case 0x31:
        result = TryNotifyStackObject(elem, param);
        break;
    case 0x0e:
        result = TryFindFunctionalObject(elem, param);
        break;
    case 0x14:
        result = TryCallFunctionalTree(elem, param);
        break;
    case 0x0d:
        result = TryPushAction(elem, param);
        break;
    case 0x2b:
        result = TryDropOnto(elem, param);
        break;
    case 0x48:
        result = TrySnapObject(elem, param);
        break;
    case 0x19:
        result = TryBudget(elem, param);
        break;
    case 0x18:
        result = TryRelationship(elem, param);
        break;
    case 0x2a:
        result = TryCreateObject(elem, param);
        break;
    case 0x33:
        result = TryPreloadObject(elem, param);
        break;
    case 0x12: {
        s16 killed = -1;
        result = TryKillObject(elem, param, &killed);
        break;
    }
    case 0x21:
        result = TryFind5WorstMotives(elem, param);
        break;
    case 0x02:
        result = TryExpression((ExpressionParam *)param);
        break;
    case 0x00:
        result = TryIdle(elem, param);
        break;
    case 0x07:
        result = TryUpdate(elem, param);
        break;
    case 0x05:
        result = TryDrop();
        break;
    case 0x04:
        result = TryGrab(elem, param);
        break;
    case 0x0f: {
        // Stops the tree if the value is non-zero, unless bit 0 asks for logging only.
        TreeBreakValue value;
        TreeBreakParam *breakParam = (TreeBreakParam *)param;
        if (InterpValue(breakParam->mScope, breakParam->mData, 0, 0, &value.mValue, 0) == -1) {
            result = -1;
            break;
        }
        if (value.mValue == 0) {
            result = 1;
            break;
        }
        if (breakParam->mFlags & 1) {
            result = 1;
            break;
        }
        if ((breakParam->mFlags >> 1) & 1) {
            g_bTreeAssert = 1;
            SetError(0x3e);
            Error(0x3e);
            result = -1;
            break;
        }
        result = 1;
        break;
    }
    case 0x08:
        result = TryRandom(elem, param);
        break;
    case 0x0b:
        result = TryDistanceTo(elem, param);
        break;
    case 0x0c:
        result = TryDirectionTo(elem, param);
        break;
    case 0x17:
        result = TryPlaySound(elem, (PlaySoundParam *)param);
        break;
    case 0x30:
        result = TryKillSounds(elem, (KillSoundsParam *)param);
        break;
    case 0x1f:
        result = TrySetToNext(elem, param);
        break;
    case 0x09:
        result = TryBurn(elem, param);
        break;
    case 0x0a:
    {
        // Claims or releases the tutorial for this object.
        bool ok;
        switch (((TutorialParam *)param)->mMode) {
        case 0:
            ok = mModule->SetTutorialObject(this);
            break;
        case 1:
            if (this == mModule->GetTutorialObject())
                ok = mModule->ClearTutorialObject();
            else
                ok = false;
            break;
        default:
            TREE_ERROR_LOOP(0x36);
            result = -1;
            goto finished;
        }
        result = ok;
        break;
    }
    case 0x23:
        result = TryUserEvent(elem, param);
        break;
    case 0x34:
        result = TryRoomEffect(elem, param);
        break;
    case 0x35:
        result = TryCheckObject(elem, param);
        break;
    case 0x37:
        result = TryAnimateObject(elem, param);
        break;
    case 0x38:
        result = TryCompleteGoal(elem, *(CompleteGoalParam *)param);
        break;
    case 0x27:
        result = TryListenForInput(elem, param);
        break;
    case 0x28:
        result = TryCameraControl(elem, param);
        break;
    case 0x39:
        result = TryHUDCreate(elem, param);
        break;
    case 0x3a:
        result = TryHUDAddItem(elem, param);
        break;
    case 0x3b:
        result = TryHUDManage(elem, param);
        break;
    case 0x3c:
        result = TryHUDData(elem, param);
        break;
    case 0x3d:
        TryParticleEffect(*(ParticleEffectParam *)param, result);
        break;
    case 0x3f:
        result = TryMemory(elem, param);
        break;
    case 0x41:
        result = TryPassiveInfluence(elem, param);
        break;
    case 0x1d:
        result = TrySetMotiveDelta(elem, param);
        break;
    case 0x2c:
        result = TryAnimate(elem, (const AnimateNewParam *)param);
        break;
    case 0x42:
    case 0x43:
        result = 1;
        break;
    case 0x44:
        result = TryCustomUI(elem, param);
        break;
    case 0x45:
        result = TryLotTransition(elem, param);
        break;
    case 0x46:
        result = TryFamilyInventory(elem, param);
        break;
    case 0x15:
        result = TrySyncAudio(elem, param);
        break;
    case 0x2f:
        result = TryNotifyPlans(elem, param);
        break;
    case 0x36:
        result = TryManageClothing(elem, param);
        break;
    default:
        TREE_ERROR(5);
        return -1;
    }
finished:
    return result;
}

// Wii: Commander, a list of things that hear about global events.
class Commander {
public:
    Commander *mNext; // 0x00
    u32 field_0x4;    // 0x04
    u32 field_0x8;    // 0x08

    virtual void Slot0();
    virtual int Dispatch(s16 message, int value);

    static Commander *sList;
};

inline int GlobalDispatch(s16 message, int value)
{
    int handled = 0;
    Commander *commander = Commander::sList;
    while (commander) {
        Commander *next = commander->mNext;
        if (commander->Dispatch(message, value))
            handled = 1;
        commander = next;
    }
    return handled;
}

// The part of an ISimInstance at 0x380 that redraws it.
class SimInstancePart {
public:
    virtual void Slot0();
    virtual void Refresh();
};

class UpdateParam {
public:
    s16 mWho;  // 0x00  0 me, 1 the stack object
    s16 mWhat; // 0x02  below
};

long cXObject::TryUpdate(StackElem *elem, XPrimParam *primParam)
{
    UpdateParam *param = (UpdateParam *)primParam;
    cXObject *object;
    switch (param->mWho) {
    case 0:
        object = this;
        break;
    case 1:
        object = GetObjectFromID(elem->mStackObject);
        if (!object) {
            OBJECT_ERROR_CHECK(false, 0x15);
        }
        break;
    default:
        OBJECT_ERROR_CHECK(false, 3);
    }
    OBJECT_ERROR_CHECK(object, 3);

    switch (param->mWhat) {
    case 0:
        break;
    case 1:
        GlobalDispatch(0x8D, object->mID);
        break;
    case 2:
        if (object->GetRoom() != 0xFFFB) {
            RoomManager *rooms = RoomManager::sRoomMgr;
            rooms->RoomScoreChanged(object->GetRoom());
        }
        break;
    case 3: {
        IBaseSimInstance *instance = object->GetISimInstance();
        if (instance)
            ((SimInstancePart *)((char *)instance + 0x380))->Refresh();
        break;
    }
    case 4:
        if (Globs::pHouse)
            Globs::pHouse->ComputeAndStoreLotData();
        break;
    default:
        OBJECT_ERROR_CHECK(false, 3);
    }
    return 1;
}

// Not matched: 2 words of 216 differ. The game ends the "completed" arms with result = 1 and a
// shared movn, and every form tried here gives sltiu, a store-flag instead.
#ifdef NON_MATCHING
class CompleteGoalParam {
public:
    s16 mOperation; // 0x00  1 open, 2 satisfy, 3 complete
    s16 mData;      // 0x02
    u8 pad_0x4;     // 0x04
    u8 mOwner;      // 0x05
    u8 mFlags;      // 0x06  bits 0 and 1 are the old primitive's
    u8 mType;       // 0x07  0, 1 and 2 goals, 4 unlocks

    bool IsOldPrim() const { return mFlags & 3; }
    bool GetPutStatusInTemp0() const { return (mFlags >> 2) & 1; }
    bool GetReturnTrueIfLocked() const { return (mFlags >> 3) & 1; }
    bool GetReturnTrueIfOpen() const { return (mFlags >> 4) & 1; }
    bool GetReturnTrueIfSatisfied() const { return (mFlags >> 5) & 1; }
    bool GetReturnTrueIfCompleted() const { return (mFlags >> 6) & 1; }
};

long cXObject::TryCompleteGoal(StackElem *elem, CompleteGoalParam &param)
{
    if (param.IsOldPrim())
        return 1;
    GoalAndUnlockData *goals = GetNeighborhood()->GetGoalAndUnlockData();
    u8 type = param.mType;
    OBJECT_ERROR_CHECK(type == 4 || type == 0 || type == 1 || type == 2, 0x6E);
    if (!goals)
        return 0;

    s16 value;
    if (InterpValue(param.mOwner, param.mData, 0, 0, &value, 0) == -1)
        return -1;
    s16 operation = param.mOperation;
    s16 status = goals->GetStatus(type, value);
    if (status == -1)
        return 0;

    if (operation == 1) {
        if (type == 2) {
        } else if (type <= 1 && status == 0) {
            goals->SetStatus(type, value, 1);
            status = goals->GetStatus(type, value);
        }
    } else if (operation == 2) {
        if (status <= 1 && type == 2) {
            goals->SetStatus(type, value, 2);
            status = goals->GetStatus(type, value);
        } else if (type == 4) {
            goals->Unlock(type, value);
            status = goals->GetStatus(type, value);
        }
    } else if (operation == 3) {
        if (type == 2) {
            if ((u16)(status - 1) <= 1) {
                goals->SetStatus(type, value, 3);
                status = goals->GetStatus(type, value);
            }
        } else if (type <= 1) {
            if (status == 1) {
                goals->SetStatus(type, value, 3);
                status = goals->GetStatus(type, value);
            }
        } else {
            goals->Unlock(type, value);
            status = goals->GetStatus(type, value);
        }
    }

    int result = 0;
    if (param.GetPutStatusInTemp0())
        mTemps[0] = status;
    if (param.GetReturnTrueIfCompleted()) {
        if (type == 4) {
            if (status == 1)
                result = 1;
        } else if (type == 2) {
            if (status == 3)
                result = 1;
        } else {
            if (status == 3)
                result = 1;
        }
    }
    if (param.GetReturnTrueIfLocked() && status == 0)
        result = 1;
    if (param.GetReturnTrueIfOpen()) {
        if (type == 4) {
            if (status == 0)
                result = 1;
        } else {
            if (status == 1)
                result = 1;
        }
    }
    if (param.GetReturnTrueIfSatisfied() && type == 2 && status == 2)
        result = 1;
    return result;
}
#endif

// Not matched: 610 of 726 words. The flow and frame are the game's, but it keeps direction in a
// saved register with other and level on the stack, and here other gets the register. Most of
// the differing words follow from that.
#ifdef NON_MATCHING
extern int gPlacementError;
extern cXObject *gPlacementConflict;

void ObjectRotationTf(EMat4 *matrix, int direction);
EVec3 IsoFracsToWorld(const f32 &x, const f32 &y, const f32 &z);
EVec3 IsoToWorld(const FTilePt &tile, const f32 &z);
FTilePt WorldToIso(const EVec3 &world);

// Wii: OffsetCoordsInDirection.
inline void OffsetCoordsInDirection(int direction, int &x, int &y)
{
    switch (direction & 7) {
    case 1:
        x += 1;
    case 0:
        y -= 1;
        break;
    case 3:
        y += 1;
    case 2:
        x += 1;
        break;
    case 5:
        x -= 1;
    case 4:
        y += 1;
        break;
    case 7:
        y -= 1;
    case 6:
        x -= 1;
        break;
    }
}

// Wii: FTilePt::FTilePt(int), one tile in a direction.
class DirectionTile : public FTilePt {
public:
    DirectionTile(int direction)
    {
        mX = 0;
        mY = 0;
        OffsetCoordsInDirection(direction, mX, mY);
        mX <<= 4;
        mY <<= 4;
    }
};

class SnapParam {
public:
    s16 mSlot;   // 0x00
    s16 mWhere;  // 0x02
    u16 mFlags;  // 0x04

    bool GetOriginOnly() const { return mFlags & 1; }
    bool GetAskPersonToMove() const { return (mFlags >> 1) & 1; }
    bool GetUseFootprint() const { return (mFlags >> 2) & 1; }
};

long cXObject::TrySnap(StackElem *elem, XPrimParam *primParam)
{
    SnapParam *param = (SnapParam *)primParam;
    cXObject *object = GetObjectFromID(elem->mStackObject);
    OBJECT_ERROR_CHECK(object, 0x15);

    FTilePt tile;
    int level;
    cXObject *other;
    int slotIndex;
    bool ignoreRooms;
    int direction;

    if (param->mWhere == 2) {
        tile = object->GetLocationRef();
        slotIndex = 0;
        level = object->mLevel;
        direction = mVars[1];
        other = 0;
        ignoreRooms = false;
        DirectionTile ahead(object->mVars[1]);
        tile.mX += ahead.mX;
        tile.mY += ahead.mY;
    } else if (param->mWhere == 1) {
        tile = object->GetLocationRef();
        level = object->mLevel;
        other = object;
        slotIndex = 0;
        ignoreRooms = false;
        direction = mVars[1];
    } else {
        RoutingSlot *slot;
        if (param->mWhere == 0) {
            if (param->mSlot < 0 || param->mSlot >= elem->mNumParams) {
                OBJECT_ERROR_CHECK(false, 8);
            }
            slotIndex = elem->GetParams()[param->mSlot];
            if (slotIndex < 0 || (u32)slotIndex >= (u32)object->GetNumRoutingSlots()) {
                OBJECT_ERROR_CHECK(false, 0x1B);
            }
            slot = object->GetRoutingSlot(slotIndex);
        } else if (param->mWhere == 3) {
            slotIndex = param->mSlot;
            if (slotIndex < 0 || (u32)slotIndex >= (u32)object->GetNumRoutingSlots()) {
                OBJECT_ERROR_CHECK(false, 0x1B);
            }
            slot = object->GetRoutingSlot(slotIndex);
        } else {
            OBJECT_ERROR_CHECK(param->mWhere == 4, 0x24);
            slotIndex = param->mSlot;
            if (slotIndex < 0 || (u32)slotIndex >= (u32)object->mModule->GetNumGlobalRoutingSlots()) {
                OBJECT_ERROR_CHECK(false, 0x1B);
            }
            slot = object->mModule->GetGlobalRoutingSlot(slotIndex);
        }
        OBJECT_ERROR_CHECK(slot, 0x1B);

        if (slot->SnapsToDirection()) {
            u16 base = object->mVars[1];
            direction = ((s16)base + slot->GetSnapDirection()) & 7;
        } else {
            direction = mVars[1];
        }

        if (slot->mSnapTarget >= 0) {
            ObjectSlot *objectSlot = object->GetObjectSlot(slot->mSnapTarget);
            if (!objectSlot) {
                OBJECT_ERROR_CHECK(false, 0x20);
            }
            tile = object->GetLocationRef();
            if (objectSlot->IsTS2ObjectSlot() && GetPerson()) {
                EMat4 rotation;
                ObjectRotationTf(&rotation, object->mVars[1]);
                EVec3 offset = IsoFracsToWorld(objectSlot->GetXOffset(), objectSlot->GetYOffset(), 0.0f);
                EVec3 world = rotation * offset;
                FTilePt moved = WorldToIso(world);
                tile.mX += moved.mX;
                tile.mY += moved.mY;
            }
            other = object;
            level = object->mLevel;
            ignoreRooms = slot->GetIgnoreRooms();
            slotIndex = slot->mSnapTarget;
        } else {
            if (slot && slot->IsTS2RoutingSlot()) {
                cXMTObject *part = object->GetMTObject();
                if (part) {
                    cXObject *anchor = part->GetAnchorTile();
                    if (anchor)
                        object = anchor;
                }
            }
            tile = object->GetLocationRef();
            level = object->mLevel;
            EMat4 rotation;
            ObjectRotationTf(&rotation, object->mVars[1]);
            EVec3 world = IsoToWorld(tile, 0.0f);
            int simType = 0;
            if (GetPerson())
                simType = GetPerson()->GetSimType();
            other = 0;
            slotIndex = 0;
            EVec3 offset = IsoFracsToWorld(slot->GetXOffset(simType), slot->GetYOffset(simType), 0.0f);
            offset = offset * rotation;
            world += offset;
            tile = WorldToIso(world);
            ignoreRooms = slot->GetIgnoreRooms();
            if (slot->mFacing >= 0)
                direction = (object->mVars[1] + slot->mFacing) & 7;
        }
    }

    int offsetX = 0;
    int offsetY = 0;
    int offsetDirection = 0;
    if (param->GetOriginOnly()) {
        offsetX = mTileX - tile.mX;
        offsetY = mTileY - tile.mY;
        offsetDirection = mVars[1] - direction;
    }
    if (other) {
        cXObject *owner = other;
        ObjectSlot *objectSlot = owner->GetObjectSlot(slotIndex);
        if (objectSlot) {
            f32 x = objectSlot->mXOffset;
            f32 y = objectSlot->mYOffset;
            s16 turn = other->mVars[1];
            f32 turnedY = 0;
            f32 turnedX = turnedY;
            switch (turn & 7) {
            case 0:
                turnedX = x;
                turnedY = y;
                break;
            case 2:
                turnedY = x;
                turnedX = -y;
                break;
            case 4:
                turnedX = -x;
                turnedY = -y;
                break;
            case 6:
                turnedY = -x;
                turnedX = y;
                break;
            }
            offsetX += (int)turnedX;
            offsetY += (int)turnedY;
        }
    }

    gPlacementError = 0;
    gPlacementConflict = 0;
    long result = TrySnap(tile, level, other, slotIndex, ignoreRooms, direction, param->GetUseFootprint());
    cXPerson *person = GetPerson();
    mVars.WriteVar(0x34, 0);
    if (result == 1) {
        if (person)
            person->mAnimator->Slot11Offset(offsetX, offsetY, offsetDirection);
        return result;
    }
    if (person && param->GetAskPersonToMove() && gPlacementError == 0xB) {
        cXObject *conflict = gPlacementConflict;
        if (conflict && conflict->GetType() == 2) {
            cXPerson *blocker = conflict->GetPerson();
            if (blocker && blocker->MoveOutOfWay(person->GetCurrentAction()->GetPriority()))
                mVars.WriteVar(0x34, 1);
        }
    }
    if (mVars[0x34] == 0 && gPlacementError == 1)
        mVars.WriteVar(0x34, 2);
    if (gPlacementConflict)
        mVars.WriteVar(0x36, gPlacementConflict->mID);
    else
        mVars.WriteVar(0x36, 0);
    return result;
}
#endif
