#include "game/simulator/core2/cXPerson.h"

#include "common/EVec3.h"

EMat4 ObjectRotationTf(int direction);
EVec3 IsoFracsToWorld(const f32 &x, const f32 &y, const f32 &z);
FTilePt WorldToIso(const EVec3 &world);

// Wii: TransformToWorldCoords, file-local there. An offset (x, y) in an object's frame, in
// sixteenths of a tile, turned by the object's direction and added to the origin. Directions 0,
// 2, 4 and 6 turn (x, y) into (x, y), (-y, x), (-x, -y) and (y, -x); the others go through the
// object's rotation matrix in world coordinates.
void TransformToWorldCoords(const FTilePt *origin, f32 x, f32 y, int direction, FTilePt *out)
{
    switch (direction) {
    case 0:
        break;
    case 2: {
        int old = (int)x;
        x = -y;
        y = old;
        break;
    }
    case 4:
        x = -x;
        y = -y;
        break;
    case 6: {
        int old = (int)x;
        x = y;
        y = -old;
        break;
    }
    default: {
        EMat4 rotation = ObjectRotationTf(direction);
        f32 fx = x;
        f32 fy = y;
        f32 z = 0;
        EVec3 world = IsoFracsToWorld(fx, fy, z);
        world = world * rotation;
        FTilePt tile = WorldToIso(world);
        x = tile.mX;
        y = tile.mY;
        break;
    }
    }
    x += origin->mX;
    y += origin->mY;
    out->mX = (int)x;
    out->mY = (int)y;
}

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

// EASTL 1.03.00's vector::DoInsertValue, written as a specialization so it's an ordinary
// function here (an instantiation would be weak, in a linkonce section). A full vector grows to
// twice its size, or to 1.
template <>
void eastl::vector<XRoute, eastl::allocator>::DoInsertValue(XRoute *position, const XRoute &value)
{
    if (mpEnd != mpCapacity) {
        const XRoute *pValue = &value;
        if (pValue >= position && pValue < mpEnd)
            ++pValue;
        new (mpEnd) XRoute(*(mpEnd - 1));
        eastl::copy_backward(position, mpEnd - 1, mpEnd);
        *position = *pValue;
        ++mpEnd;
    } else {
        const unsigned int prevSize = mpEnd - mpBegin;
        const unsigned int newSize = prevSize ? 2 * prevSize : 1;
        XRoute *const newData = newSize ? (XRoute *)mAllocator.allocate(newSize * sizeof(XRoute)) : 0;
        XRoute *newEnd = eastl::uninitialized_copy_ptr(mpBegin, position, newData);
        new (newEnd) XRoute(value);
        newEnd = eastl::uninitialized_copy_ptr(position, mpEnd, ++newEnd);
        DoDestroyValues(mpBegin, mpEnd);
        if (mpBegin)
            mAllocator.deallocate(mpBegin);
        mpBegin = newData;
        mpEnd = newEnd;
        mpCapacity = newData + newSize;
    }
}
