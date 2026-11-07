#include "game/simulator/core2/cXPerson.h"

#include "game/animation/SAnimator2.h"

inline int ObjectModule::GetNumGlobalRoutingSlots()
{
    return mGlobalRoutingSlotsEnd - mGlobalRoutingSlots;
}

inline RoutingSlot *ObjectModule::GetGlobalRoutingSlot(int index)
{
    return &mGlobalRoutingSlots[index];
}

// Primitive 0x2d.
class GotoRoutingSlotParam {
public:
    s16 mData;  // 0x00
    s16 mType;  // 0x02  0 mData is a parameter, 1 a slot, 2 a global slot
    u8 mFlags;  // 0x04  bit 0 no failure trees

    bool GetAllowFailureTrees() const { return (mFlags & 1) ^ 1; }
};

u32 cXPerson::ReconType()
{
    return 0x50455253;
}

// The route's state lives in the frame, so a node comes back here each tick until it ends.
long cXPerson::TryGotoRoutingSlot(StackElem *elem, XPrimParam *primParam)
{
    GotoRoutingSlotParam *param = (GotoRoutingSlotParam *)primParam;
    if (!IsModelLoaded())
        return 2;
    cXObject *object = GetObjectFromID(elem->mStackObject);
    if (elem->mWaiting == 0) {
        RoutingSlot *slot;
        int index;
        ((SAnimator2 *)mAnimator)->EnableSmoothRoute();
        TREE_CHECK(this, object, 0x35, return -1);
        if (param->mType == 0) {
            TREE_CHECK(this, param->mData >= 0 || param->mData < elem->mNumParams, 8, return -1);
            index = elem->GetParams()[param->mData];
            if (!(index >= 0 && index < object->GetNumRoutingSlots())) {
                TREE_CHECK(this, false, 0x25, return -1);
            }
            slot = object->GetRoutingSlot(index);
        } else if (param->mType == 1) {
            index = param->mData;
            if (!(index >= 0 && index < object->GetNumRoutingSlots())) {
                TREE_CHECK(this, false, 0x25, return -1);
            }
            slot = object->GetRoutingSlot(index);
        } else {
            if (param->mType != 2) {
                TREE_CHECK(this, false, 0x25, return -1);
            }
            index = param->mData;
            if (!(index >= 0 && index < mModule->GetNumGlobalRoutingSlots())) {
                TREE_CHECK(this, false, 0x25, return -1);
            }
            slot = mModule->GetGlobalRoutingSlot(index);
        }
        slot->SetAllowFailureTrees(param->GetAllowFailureTrees());
        return GotoRoutingSlot(object, slot, (int &)elem->mWaiting);
    }
    return GotoRoutingSlot(object, 0, (int &)elem->mWaiting);
}

int cXPerson::IsIdle()
{
    return field_0x538 ^ 1;
}

int cXPerson::IsModelLoaded()
{
    ESim *sim = (ESim *)mESimPerson;
    if (sim && sim->mModelLoaded)
        return 1;
    return 0;
}

// Primitive 0x1d on a Sim: influence goes to cXObject's version, and the rest is the Sim's own list.
long cXPerson::TrySetMotiveDelta(StackElem *elem, XPrimParam *primParam)
{
    SetMotiveDeltaParam *param = (SetMotiveDeltaParam *)primParam;
    if (param->IsInfluence())
        return cXObject::TrySetMotiveDelta(elem, primParam);
    if (param->GetClearAll()) {
        mMotiveIncs.clear();
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
    if (mMotiveIncs.size() < 0x10) {
        MotiveInc inc;
        inc.mMotive = param->mMotive;
        inc.mDelta = delta * 0.00055555562f;
        inc.mLimit = limit;
        mMotiveIncs.push_back(inc);
    }
    return 1;
}

// Primitive 0x1b: go to a place around the stack object.
class GotoRelativeParam {
public:
    u16 pad_0x0;
    s8 mLocation;  // 0x02
    s8 mDirection; // 0x03
    u16 pad_0x4;
    u8 mFlags;     // 0x06  bit 1 no failure trees, 2 allow different levels

    bool GetAllowFailureTrees() const { return ((mFlags >> 1) ^ 1) & 1; }
    bool GetAllowDifferentAlts() const { return (mFlags >> 2) & 1; }
};

long cXPerson::TryGotoRelative(StackElem *elem, XPrimParam *primParam)
{
    GotoRelativeParam *param = (GotoRelativeParam *)primParam;
    if (!IsModelLoaded())
        return 2;
    cXObject *object = GetObjectFromID(elem->mStackObject);
    if (elem->mWaiting) {
        ((SAnimator2 *)mAnimator)->EnableSmoothRoute();
        return GotoRoutingSlot(object, 0, (int &)elem->mWaiting);
    }
    RoutingSlot slot;
    slot.mMultiplier = 1;
    slot.SetTileDistances(1.0f, 1.0f, 1.0f);
    slot.SetAllowFailureTrees(param->GetAllowFailureTrees());
    slot.SetAllowDifferentAlts(param->GetAllowDifferentAlts());
    switch (param->mLocation) {
    case -2:
        slot.SetIsOnTopOfObject();
        break;
    case -1:
        slot.AllowDirection(0);
        slot.AllowDirection(2);
        slot.AllowDirection(4);
        slot.AllowDirection(6);
        slot.SetTileDistances(1.0f, 2.0f, 1.0f);
        break;
    case 0:
    case 2:
    case 4:
    case 6:
        slot.AllowDirection(param->mLocation);
        break;
    case 1:
        slot.AllowDirection(0);
        slot.AllowDirection(2);
        break;
    case 3:
        slot.AllowDirection(2);
        slot.AllowDirection(4);
        break;
    case 5:
        slot.AllowDirection(4);
        slot.AllowDirection(6);
        break;
    case 7:
        slot.AllowDirection(6);
        slot.AllowDirection(0);
        break;
    default:
        slot.AllowDirection(0);
        slot.AllowDirection(2);
        slot.AllowDirection(4);
        slot.AllowDirection(6);
        slot.SetTileDistances(1.0f, 2.0f, 1.0f);
        break;
    }
    switch (param->mDirection) {
    case -2:
        slot.mFacing = -2;
        break;
    case -1:
        slot.mFacing = -3;
        break;
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        slot.SetFacingDirection(param->mDirection);
        break;
    default:
        return 0;
    }
    return GotoRoutingSlot(object, &slot, (int &)elem->mWaiting);
}

// Primitive 0x11: count a parameter down, and watch for an interruption.
struct IdleForInputParam {
    s16 mParam; // 0x00
    s16 mAllow; // 0x02
};

long cXPerson::TryIdleForInput(StackElem *elem, XPrimParam *primParam)
{
    IdleForInputParam *param = (IdleForInputParam *)primParam;
    int interrupt = ShouldInterrupt();
    TREE_CHECK(this, param->mParam >= 0 && param->mParam < elem->mNumParams, 8, return -1);
    s16 *counter = &elem->GetParams()[param->mParam];
    if (!interrupt) {
        if (*counter == 0) {
            s16 flags = mVars[8];
            mVars.WriteVar(8, flags & ~0x40);
            return 1;
        }
        --*counter;
        return 2;
    }
    return executeInterruptOnIdlePrimitve(elem, counter, param->mAllow != 0);
}

extern Interaction *sImmediateAction;
extern cXPerson *sImmediatePerson;

int cXPerson::AddAction(Interaction *action)
{
    TreeTableEntry *entry = action->GetEntry();
    if (!entry)
        return 0;
    if (entry->GetImmediate())
        return RunImmediateAction(action);

    if (!entry->GetAllowConsecutive() && mActionQueue.Count()) {
        Interaction *last = mActionQueue.At(mActionQueue.Count() - 1);
        if (last->GetIconObject() == action->GetIconObject()
            && last->mTreeTabIndex == action->mTreeTabIndex)
            return 0;
    }

    int priority = action->GetPriority();
    action->SetUniqueID();
    int toFront = 0;
    if (action->GetContinuation() || action->GetPushToHead() || action->GetRunAsSub())
        toFront = 1;
    if (action->GetContinuation() && action->GetCarryNameOver())
        action->SetName(GetCurrentAction()->GetName());

    int visible = mActionQueue.CountVisible();
    if (visible >= 8) {
        int full = 1;
        if (!toFront) {
            int lower = 0;
            u32 i = 0;
            if (field_0x538 != 0)
                i = 1;
            for (; i < mActionQueue.Count(); i++) {
                Interaction *queued = mActionQueue.At(i);
                if ((int)queued->field_0x28 < priority && !queued->GetMustRun()
                    && mActionQueue.IsVisibleInline(*queued))
                    lower++;
            }
            if (visible - lower <= 7)
                full = 0;
        }
        if (full) {
            if (!action->GetMustRun())
                return 0;
            action->mFlags |= 0x10;
        }
    }

    if (action->GetRunAsSub())
        return mActionQueue.Insert(*action, 0);
    if (toFront)
        return mActionQueue.Insert(*action, GetPendingAction());

    for (u32 i = GetPendingAction(); i < mActionQueue.Count();) {
        Interaction *queued = mActionQueue.At(i);
        if ((int)queued->field_0x28 < priority && !queued->GetMustRun())
            ActionSkipped(*queued);
        else
            i++;
    }

    if (sImmediateAction && this == sImmediatePerson) {
        if (action->GetCarryNameOver())
            action->SetName(sImmediateAction->GetName());
        *sImmediateAction = *action;
    }
    return mActionQueue.Insert(*action, mActionQueue.Count());
}
