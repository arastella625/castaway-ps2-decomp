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
