#include "game/simulator/core2/cXPerson.h"

// The game inlines RoutingSlot's constructor here and also has it as a function (0x1E15F0, in
// cXPerson.cpp). Declaring it inline in this file only gives the inlined copy without a second
// definition.
inline RoutingSlot::RoutingSlot()
{
    mOffset[0] = 0;
    mOffset[1] = 0;
    mOffset[2] = 0;
    mFlags = 0;
    mMultiplier = 0;
    mMultiplier2 = 0;
    mMultiplier3 = 0;
    mSnapTarget = -1;
    mMin = 16;
    mMax = 16;
    mOptimal = 16;
    mGradient = 0.1875f;
    mFacing = -2;
    mResolution = 16;
}

XRoute::XRoute(cXObject *start, cXObject *dest, const RoutingSlot *slot)
    : mGoals(eastl::allocator())
{
    Construct(start, dest, slot);
}

void XRoute::Construct(cXObject *start, cXObject *dest, const RoutingSlot *slot)
{
    mStart = start;
    mDest = dest;
    field_0x80 = 0;
    mMovingPerson = 0;
    if (slot)
        mSlot = *slot;
    mCurrentGoal = -1;
    field_0x58 = -1;
    mTrapCount = 0;
    mWaitStartTicks = 0;
    if (mStart)
        mLastLocation = mStart->GetLocationRef();
    mValid = 1;
    mCurPortal = 0;
    mMoveSuccess = 0;
    mResult = 0;
    mBlockingObjectID = 0;
    field_0x94 = 0;
    field_0x9c = 0;
    if (mStart) {
        cXPerson *person = mStart->GetPerson();
        if (person) {
            field_0x9c = person->mPersonData[0x20];
            person->field_0x5e0 = 0;
        }
    }
    field_0xa0 = 0x30;
}

bool XRoute::HasCurrentGoal()
{
    return mCurrentGoal != -1;
}

RouteGoal *XRoute::GetCurrentGoal()
{
    return &mGoals[mCurrentGoal];
}
