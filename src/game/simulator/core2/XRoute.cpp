#include "game/simulator/core2/cXPerson.h"

// The game inlines RoutingSlot's constructor here and also has it as a function (0x1E15F0, in
// cXPerson.cpp). Declaring it inline in this file only gives the inlined copy without a second
// definition.
inline RoutingSlot::RoutingSlot()
{
    mOffsetX = 0;
    mOffsetY = 0;
    mOffsetZ = 0;
    mFlags = 0;
    mMultiplier[0] = 0;
    mMultiplier[1] = 0;
    mMultiplier[2] = 0;
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

// Wii's is the one the compiler writes: every member in order, the halfword pads skipped.
XRoute::XRoute(const XRoute &other)
    : mGoals(other.mGoals), mSlot(other.mSlot), mDest(other.mDest), mStart(other.mStart),
      mCurrentGoal(other.mCurrentGoal), field_0x58(other.field_0x58), mTrapCount(other.mTrapCount),
      mWaitStartTicks(other.mWaitStartTicks), mLastLocation(other.mLastLocation),
      mCurPortal(other.mCurPortal), mStartPt(other.mStartPt), mExitDirFlag(other.mExitDirFlag),
      mValid(other.mValid), field_0x80(other.field_0x80), mMovingPerson(other.mMovingPerson),
      mMoveSuccess(other.mMoveSuccess), mResult(other.mResult),
      mBlockingObjectID(other.mBlockingObjectID), field_0x94(other.field_0x94),
      field_0x98(other.field_0x98), field_0x9c(other.field_0x9c), field_0xa0(other.field_0xa0),
      field_0xa4(other.field_0xa4)
{
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
