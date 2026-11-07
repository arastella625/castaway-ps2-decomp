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

#ifdef NON_MATCHING
// 150 of the 151 words line up. The game has one more: a nop aligning the error block that the
// last two checks share. The two checks merge here too, but the label isn't aligned.
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
            TREE_CHECK(this, index >= 0 && index < object->GetNumRoutingSlots(), 0x25, return -1);
            slot = object->GetRoutingSlot(index);
        } else if (param->mType == 1) {
            index = param->mData;
            TREE_CHECK(this, index >= 0 && index < object->GetNumRoutingSlots(), 0x25, return -1);
            slot = object->GetRoutingSlot(index);
        } else {
            TREE_CHECK(this, param->mType == 2, 0x25, return -1);
            index = param->mData;
            TREE_CHECK(this, index >= 0 && index < mModule->GetNumGlobalRoutingSlots(), 0x25, return -1);
            slot = mModule->GetGlobalRoutingSlot(index);
        }
        slot->SetAllowFailureTrees(param->GetAllowFailureTrees());
        return GotoRoutingSlot(object, slot, (int &)elem->mWaiting);
    }
    return GotoRoutingSlot(object, 0, (int &)elem->mWaiting);
}
#endif
