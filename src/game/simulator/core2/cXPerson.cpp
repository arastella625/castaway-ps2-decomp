#include "game/simulator/core2/cXPerson.h"

#include "game/GameData.h"
#include "game/simulator/core/Behavior.h"
#include "game/simulator/core/Lot.h"

#include "game/animation/SAnimator2.h"

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

class EResource {
public:
    void DelRef();
};
class ERShader : public EResource {};

// Wii: GetShaderForObject. Returns the icon, and the shader through the pointer.
u32 GetShaderForObject(cXObject *object, ERShader **shader);

// Wii: ActionQueue::Insert, which calls Queue<Interaction, 12>::Insert and AddActionToHUD. PS2
// inlines both, and Interaction::GetID.
int ActionQueue::Insert(const Interaction &action, u32 index)
{
    if (!QueueInsert(action, index))
        return 0;
    if (At(index)->field_0x30 == 0)
        At(index)->SetUniqueID();

    GameData *gd = &_gd;
    if (gd->mHud)
        mHudOff = 0;
    if (mHudOff)
        return 1;
    Interaction *queued = At(index);
    if (!IsVisibleAction(*queued))
        return 1;
    ERShader *shader;
    u32 icon = GetShaderForObject(queued->GetIconObject(), &shader);
    if (!icon && !shader)
        return 1;
    u32 guid = FindVisualInsertGUID(index);
    if (gd->mHud)
        gd->mHud->AddAction(queued, queued->GetName(), icon, shader, guid);
    if (shader)
        shader->DelRef();
    return 1;
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

void RoutingSlot::WriteSlot(SlotDescriptor *descriptor) const
{
    descriptor->mType = 3;
    descriptor->mFlags = mFlags;
    descriptor->mMultiplier[0] = mMultiplier;
    descriptor->mMultiplier[1] = mMultiplier2;
    descriptor->mMultiplier[2] = mMultiplier3;
    descriptor->mSnapTarget = mSnapTarget;
    descriptor->mMin = mMin;
    descriptor->mMax = mMax;
    descriptor->mOptimal = mOptimal;
    descriptor->mGradient = mGradient;
    descriptor->mFacing = mFacing;
    descriptor->mResolution = mResolution;
    descriptor->mOffset[0] = mOffset[0];
    descriptor->mOffset[1] = mOffset[1];
    descriptor->mOffset[2] = mOffset[2];
}

void RoutingSlot::SetDistances(int min, int max, int optimal)
{
    mMin = min;
    mMax = max;
    mOptimal = optimal;
}

f32 RoutingSlot::GetXOffset(int simType) const
{
    f32 extra;
    if (mFacing <= 0 || mFacing == 4)
        return mOffset[0];
    if ((mFacing & 1) == 0) {
        if (mFacing == 2)
            extra = -kStraightOffset[simType];
        else
            extra = kStraightOffset[simType];
    } else if (mFacing == 1 || mFacing == 3) {
        extra = -kCornerOffset[simType];
    } else {
        extra = kCornerOffset[simType];
    }
    return mOffset[0] + extra;
}

f32 RoutingSlot::GetYOffset(int simType) const
{
    f32 extra;
    if (mFacing < 0 || mFacing == 2 || mFacing == 6)
        return mOffset[1];
    if ((mFacing & 1) == 0) {
        if (mFacing == 4)
            extra = -kStraightOffset[simType];
        else
            extra = kStraightOffset[simType];
    } else if (mFacing == 3 || mFacing == 5) {
        extra = -kCornerOffset[simType];
    } else {
        extra = kCornerOffset[simType];
    }
    return mOffset[1] + extra;
}

void RoutingSlot::AllowDirection(int direction)
{
    if ((u32)direction < 8)
        mFlags |= 1 << direction;
}

void RoutingSlot::SetFacingDirection(int direction)
{
    if ((u32)direction < 8)
        mFacing = direction;
}

void RoutingSlot::SetTileDistances(f32 min, f32 max, f32 preferred)
{
    mMin = (int)(min * 16.0f);
    mMax = (int)(max * 16.0f);
    mOptimal = (int)(preferred * 16.0f);
}

RoutingSlot::RoutingSlot()
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

// Wii: cXPortal::FailedPortalTree, static.
class cXPortal {
public:
    static void FailedPortalTree(ObjectModule *module, cXObject *object, cXPortal *portal);
};

// Through a portal: done once the Sim is in the destination's room, else route again.
cXPerson::RoutingState cXPerson::Routing_WaitingForPortal(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    if (ShouldInterrupt() && mAnimator->IsInterruptable()) {
        route->mResult = 4;
        return kRoutingFailed;
    }
    s16 destRoom = route->mDest->mVars[0x1d];
    if (destRoom != mVars[0x1d]) {
        cXPortal *portal = route->mCurPortal;
        if (portal && !GetLastResult())
            cXPortal::FailedPortalTree(mModule, this, portal);
        if (TryRoomRouting(route)) {
            SetPersonData(0x20, 0);
            code = 3;
            done = true;
            return kRoutingWaitingForPortal;
        }
        route->mResult = 2;
        return kRoutingNeedFailureFeedback;
    }
    SetPersonData(0x20, route->field_0x9c);
    return kRoutingInitial;
}

// One step of the walk. The animator's answer is also the tree's code.
cXPerson::RoutingState cXPerson::Routing_Walking(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    if (ShouldInterrupt() && mAnimator->IsInterruptable()) {
        route->mResult = 4;
        return kRoutingWalkTerminated;
    }
    if (mRoutes.size() == 0)
        return kRoutingWalkTerminated;
    if (mPath.size() == 0)
        return kRoutingWalkTerminated;
    code = mAnimator->FollowOneStep();
    switch (code) {
    case 2:
        done = true;
        return kRoutingWalking;
    case 1:
        return kRoutingWalkingSucceeded;
    case 0:
        return kRoutingWalkingFailed;
    }
    return kRoutingWalkingFailed;
}

// For up to 30 ticks the Sim plays its "sg_route waiting animation" tree, then routes again.
cXPerson::RoutingState cXPerson::Routing_Waiting(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    if (ShouldInterrupt() && mAnimator->IsInterruptable()) {
        route->mResult = 4;
        return kRoutingFailed;
    }
    int start = route->mWaitStartTicks;
    if (!start)
        start = GetSimulator()->mTicks;
    if (GetSimulator()->mTicks - start < 30) {
        s16 tree = mSelector->mBehavior->GetTreeIDByNameFast("sg_route waiting animation");
        if (tree && GosubObjectTree(this, 0, tree, false)) {
            code = 3;
            done = true;
            return kRoutingWaiting;
        }
    }
    route->mWaitStartTicks = 0;
    mAnimator->field_0x664 = 0;
    return kRoutingInitial;
}

// The route failed. If the slot allows it, the target's "gl_route failure feedback" tree runs
// with the result and the blocking object as its parameters.
cXPerson::RoutingState cXPerson::Routing_NeedFailureFeedback(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    if (route->mResult == 0)
        route->mResult = 1;
    SetPersonData(0x1c, route->mResult);
    if ((route->GetRoutingSlot()->mFlags >> 14) & 1) {
        s16 tree = route->mDest->mSelector->mBehavior->GetTreeIDByNameFast("gl_route failure feedback");
        s16 params[4] = {route->GetResult(), route->mBlockingObjectID, 0, 0};
        if (tree && GosubObjectTree(route->mDest, params, tree, false)) {
            SetPersonData(0x20, 0);
            code = 3;
            done = true;
            return kRoutingWaitingForFailureFeedback;
        }
    } else if (!mAnimator->EndFollow()) {
        done = true;
    }
    mAnimator->field_0x664 = 0;
    return kRoutingFailed;
}

#ifdef NON_MATCHING
// Not matched: 25 of 36 words line up. The game's inlined IsIdling clears its flag before the
// load and sign-extends the state again before comparing with 3; no form tried does both (an
// int or bool flag, ||, a switch, GetIdleState as int or s16, a person data getter).
// Standing up: back to the start once the Sim is idle, else failure feedback with result 6.
cXPerson::RoutingState cXPerson::Routing_WaitingForStand(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    SetPersonData(0x20, route->field_0x9c);
    if (GetLastTransition() && IsIdling())
        return kRoutingInitial;
    route->mResult = 6;
    return kRoutingNeedFailureFeedback;
}
#endif

// Waits for a routing turn, then plans the route: InitRoute's answer is the tree's code.
cXPerson::RoutingState cXPerson::Routing_FoundGoals(bool &done, TreeReturnCode &code)
{
    if (!CanRouteThisFrame()) {
        if (!mInRoutingQueue) {
            s_RoutingQueue.Enqueue(this);
            mInRoutingQueue = 1;
        }
        code = 2;
        done = true;
        return kRoutingFoundGoals;
    }
    s_nNumRoutedThisFrame++;
    code = InitRoute(GetCurrentRoute());
    PopFromRoutingQueue(this);
    switch (code) {
    case 2:
        return kRoutingFoundPath;
    case 3:
        SetPersonData(0x20, 0);
        done = true;
        return kRoutingWaitingForSit;
    case 0:
        return kRoutingTrapped;
    case 1:
        return kRoutingSucceeded;
    }
    return kRoutingFailed;
}

// Another Sim was asked to move. The Sim first plays "sg_route ask to move animation" at it, then
// "sg_route waiting animation" for up to 300 ticks, and is trapped if the other doesn't move.
cXPerson::RoutingState cXPerson::Routing_WaitingForPersonToMove(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    if (ShouldInterrupt() && mAnimator->IsInterruptable()) {
        route->mResult = 4;
        return kRoutingFailed;
    }
    mAnimator->field_0x664 = 0;
    if (route->mMovingPerson == 0) {
        route->mWaitStartTicks = 0;
        if (route->mMoveSuccess)
            route->mTrapCount = 0;
        return kRoutingInitial;
    }
    if (route->mWaitStartTicks == 0) {
        s16 tree = mSelector->mBehavior->GetTreeIDByNameFast("sg_route ask to move animation");
        if (tree && GosubObjectTree(this, 0, tree, false)) {
            GetCurElem()->mStackObject = route->mMovingPerson->mID;
            code = 3;
            done = true;
            route->mWaitStartTicks = GetSimulator()->mTicks;
            return kRoutingWaitingForPersonToMove;
        }
        return kRoutingTrapped;
    }
    if (GetSimulator()->mTicks - route->mWaitStartTicks < 300) {
        s16 tree = mSelector->mBehavior->GetTreeIDByNameFast("sg_route waiting animation");
        if (tree && GosubObjectTree(this, 0, tree, false)) {
            code = 3;
            done = true;
            return kRoutingWaitingForPersonToMove;
        }
    }
    route->mWaitStartTicks = 0;
    return kRoutingTrapped;
}

// Trapped: on the routing turn, the trap count steps 0 to 3. A Sim that has moved more than 8
// sixteenths of a tile since last time routes again; then it asks others to move; then it
// routes again; then failure feedback.
cXPerson::RoutingState cXPerson::Routing_Trapped(bool &done, TreeReturnCode &code)
{
    if (!CanRouteThisFrame()) {
        if (!mInRoutingQueue) {
            s_RoutingQueue.Enqueue(this);
            mInRoutingQueue = 1;
        }
        code = 2;
        done = true;
        return kRoutingTrapped;
    }
    s_nNumRoutedThisFrame++;
    PopFromRoutingQueue(this);
    code = 2;
    mAnimator->field_0x664 = 0;
    XRoute *route = GetCurrentRoute();
    if (route->mTrapCount == 0) {
        FTilePt last = route->mLastLocation;
        int dist = last.SquareDist(GetLocationRef());
        last = GetLocationRef();
        route->mLastLocation = last;
        if (dist > 0x40) {
            route->mTrapCount = 0;
            return kRoutingInitial;
        }
        route->mTrapCount = 1;
        return kRoutingTrapped;
    }
    if (route->mTrapCount == 1) {
        route->mTrapCount = 2;
        if (!route->mMovingPerson && AskOthersToMove(route))
            return kRoutingWaitingForPersonToMove;
        return kRoutingNeedFailureFeedback;
    }
    if (route->mTrapCount == 2) {
        route->mTrapCount = 3;
        return kRoutingInitial;
    }
    if (route->mTrapCount == 3)
        return kRoutingNeedFailureFeedback;
    return kRoutingFailed;
}

// A failed route keeps its result, or gets 1, and the tree goes on with code 0.
cXPerson::RoutingState cXPerson::Routing_Failed(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    if (route->mResult == 0)
        route->mResult = 1;
    CleanUpAfterFinishingRoute(kRoutingFailed);
    code = 0;
    done = true;
    return kRoutingFailed;
}

// Sitting down: succeeded, or failure feedback with person data 0x1C as the result (5 if 0).
cXPerson::RoutingState cXPerson::Routing_WaitingForSit(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    if (GetLastTransition())
        return kRoutingSucceeded;
    s16 result = mPersonData[0x1c];
    if (result == 0)
        route->mResult = 5;
    else
        route->mResult = result;
    return kRoutingNeedFailureFeedback;
}

// The next four wait for the animator to finish the walk, then move on.
cXPerson::RoutingState cXPerson::Routing_WalkingSucceeded(bool &done, TreeReturnCode &code)
{
    if (mAnimator->EndFollow())
        return kRoutingSucceeded;
    done = true;
    return kRoutingWalkingSucceeded;
}

cXPerson::RoutingState cXPerson::Routing_WalkingFailed(bool &done, TreeReturnCode &code)
{
    if (mAnimator->EndFollow())
        return kRoutingTrapped;
    done = true;
    return kRoutingWalkingFailed;
}

cXPerson::RoutingState cXPerson::Routing_WalkTerminated(bool &done, TreeReturnCode &code)
{
    if (mAnimator->EndFollow())
        return kRoutingFailed;
    done = true;
    return kRoutingWalkTerminated;
}

// A path was found: the animator starts following it, with the distance to the target.
cXPerson::RoutingState cXPerson::Routing_FoundPath(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    f32 distance = CalcShortDistance(route->mDest);
    mAnimator->BeginFollow(distance);
    done = true;
    return kRoutingWalking;
}

cXPerson::RoutingState cXPerson::Routing_RouteInvalidatedWhileWalking(bool &done, TreeReturnCode &code)
{
    if (mAnimator->EndFollow())
        return kRoutingInitial;
    done = true;
    return kRoutingRouteInvalidatedWhileWalking;
}

cXPerson::RoutingState cXPerson::Routing_WaitingForFailureFeedback(bool &done, TreeReturnCode &code)
{
    XRoute *route = GetCurrentRoute();
    SetPersonData(0x1c, route->mResult);
    mAnimator->field_0x664 = 0;
    return kRoutingFailed;
}

cXPerson::RoutingState cXPerson::Routing_ObjectDeleted(bool &done, TreeReturnCode &code)
{
    mAnimator->field_0x664 = 0;
    return kRoutingFailed;
}

cXPerson::RoutingState cXPerson::Routing_ObjectDeletedWhileWalking(bool &done, TreeReturnCode &code)
{
    if (mAnimator->EndFollow())
        return kRoutingObjectDeleted;
    done = true;
    return kRoutingObjectDeletedWhileWalking;
}
