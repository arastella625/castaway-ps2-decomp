#ifndef GAME_SIMULATOR_CORE2_CXPERSON_H
#define GAME_SIMULATOR_CORE2_CXPERSON_H

#include "common/types.h"
#include "game/simulator/core/Interaction.h"
#include "game/simulator/core/ObjSelector.h"
#include "game/simulator/core/cXObject.h"

class Neighbor;

// Wii: TreeReturnCode. 64 bits on PS2: it's stored with sd.
typedef long TreeReturnCode;

class cXPerson;

// Wii: Queue<cXPerson *, 24>, the Sims waiting for a routing turn.
class RoutingQueue {
public:
    cXPerson *mItems[24]; // 0x00
    u32 mHead;            // 0x60
    u32 mTail;            // 0x64

    bool Enqueue(cXPerson *const &person)
    {
        if (mTail - mHead < 24) {
            mItems[mTail % 24] = person;
            mTail++;
            return true;
        }
        return false;
    }
    cXPerson *&Peek(u32 index) { return mItems[(mHead + index) % 24]; }
};

// Wii: RouteGoal, one place a route may end, 16 bytes. Its fields aren't named yet.
struct RouteGoal {
    u32 field_0x0[4];
};

// Wii: XRoute, 0xA8 bytes on PS2. Made in GotoRoutingSlot and kept in the Sim's route list.
class XRoute {
public:
    eastl::vector<RouteGoal, eastl::allocator> mGoals; // 0x00
    RoutingSlot mSlot;   // 0x10  a copy of the slot it routes to; Wii: GetRoutingSlot
    cXObject *mDest;     // 0x4c  Wii: GetDest
    cXObject *mStart;    // 0x50  the Sim, Wii: GetStart
    int mCurrentGoal;    // 0x54  -1 none, Wii: HasCurrentGoal
    int field_0x58;      // 0x58
    int mTrapCount;      // 0x5c  Wii: SetTrapCount
    int mWaitStartTicks; // 0x60  Wii: GetWaitStartTicks
    FTilePt mLastLocation; // 0x64  Wii: GetLastLocation, where the Sim was when last trapped
    cXPortal *mCurPortal; // 0x6c  Wii: GetCurPortal
    FTilePt mStartPt;    // 0x70  Wii: GetStartPt
    int mExitDirFlag;    // 0x78  Wii: GetExitDirFlag
    int mValid;          // 0x7c  Wii: GetValid
    int field_0x80;      // 0x80
    cXObject *mMovingPerson; // 0x84  the Sim asked to move, Wii: GetMovingPerson
    int mMoveSuccess;    // 0x88  Wii: GetMoveSuccess
    int mResult;         // 0x8c  Wii: GetResult and SetResult
    s16 mBlockingObjectID; // 0x90  Wii: GetBlockingObjectID
    int field_0x94;      // 0x94
    int field_0x98;      // 0x98
    s16 field_0x9c;      // 0x9c  person data 0x20 takes it on arrival
    int field_0xa0;      // 0xa0  0x30 when made
    int field_0xa4;      // 0xa4

    XRoute(cXObject *start, cXObject *dest, const RoutingSlot *slot);
    XRoute(const XRoute &other);
    void Construct(cXObject *start, cXObject *dest, const RoutingSlot *slot);
    RoutingSlot *GetRoutingSlot();
    bool HasCurrentGoal();
    RouteGoal *GetCurrentGoal();
    void BuildGoalList();
    int IsPersonSittingOnChairGoal(cXPerson *person);
    int GetResult() const { return mResult; }
    int GetExitDirFlag() const { return mExitDirFlag; }
};

// Wii: ActionQueue, a Queue<Interaction, 12>: a ring of twelve actions.
class ActionQueue {
public:
    Interaction mActions[12]; // 0x000
    u32 mHead;                // 0x300
    u32 mTail;                // 0x304

    u32 Count() const { return mTail - mHead; }
    Interaction *At(u32 index) { return &mActions[(mHead + index) % 12]; }
    bool IsVisibleAction(const Interaction &action) const;
    int CountVisible()
    {
        s16 lastGroup = 0;
        int visible = 0;
        for (int index = 0; index < (int)Count(); index++) {
            Interaction *queued = At(index);
            if (IsVisibleAction(*queued)) {
                int group = queued->field_0x30;
                if (lastGroup != group) {
                    visible++;
                    lastGroup = group;
                }
            }
        }
        return visible;
    }
    // The body of IsVisibleAction, which AddAction's second loop has inlined.
    int IsVisibleInline(Interaction &action)
    {
        int shown = 0;
        if (action.GetIconObject())
            shown = !action.GetHidden() ? 1 : 0;
        if (shown && action.GetContinuation()) {
            if (Count() == 0)
                return 0;
            if (&action != At(0))
                shown = 0;
        }
        return shown;
    }
    u8 mHudOff;               // 0x308  cleared whenever the HUD exists
    u8 pad_0x309[3];

    // Wii: Queue<Interaction, 12>::Insert. Later actions move back one place.
    bool QueueInsert(const Interaction &action, u32 index)
    {
        if (Count() < 12) {
            u32 i = mTail;
            while (mHead + index < i) {
                mActions[i % 12] = mActions[(i - 1) % 12];
                i--;
            }
            mActions[i % 12] = action;
            mTail++;
            return true;
        }
        return false;
    }
    int Insert(const Interaction &action, u32 index);
    u32 FindVisualInsertGUID(u32 index) const;
    bool Peek(u32 index, Interaction &action) const
    {
        if (index >= mTail - mHead)
            return false;
        action = mActions[(mHead + index) % 12];
        return true;
    }
};

// Wii: MotiveInc, one change to a motive over time. It has a destructor that does nothing.
class MotiveInc {
public:
    int mMotive; // 0x00
    f32 mDelta;  // 0x04
    f32 mLimit;  // 0x08

    ~MotiveInc() {}
};

// The Sim's animator, SAnimator2 in game/animation: only the slot ParticleEffect calls.
class PersonAnimatorVirtuals {
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
    virtual void Slot11Offset(f32 x, f32 y, f32 direction);
    virtual void BeginFollow(f32 distance); // Wii SAnimator2 slot 12
    virtual int FollowOneStep(); // Wii SAnimator2 slot 13
    virtual int EndFollow(); // Wii SAnimator2 slot 14
    virtual int IsFollowing(); // Wii SAnimator2 slot 15: GotoRoutingSlot drops the result
    virtual int IsInterruptable(); // Wii SAnimator2 slot 16
    virtual void Slot17();
    virtual void Slot18();
    virtual void Slot19();
    virtual void Slot20();
    virtual void Slot21();
    virtual void Slot22();
    virtual void Slot23();
    virtual void Slot24();
    virtual void Slot25();
    virtual void Slot26();
    virtual void Slot27();
    virtual void Slot28();
    virtual void Slot29();
    virtual void Slot30();
    virtual void Slot31();
    virtual int PlayEffect(u32 a, u32 b);
};

// GCC 2.95 puts the vtable pointer after a class's own data, so the data goes in a derived class
// to keep the pointer at 0, where the game has it.
class PersonAnimator : public PersonAnimatorVirtuals {
public:
    u8 pad_0x4[0x660];
    u8 field_0x664; // 0x664  Routing_Waiting clears it (Wii +0x656)
};

// A Sim. The fields are the ones InterpValue reads.
class cXPerson : public cXObject {
public:
    u32 pad_0x13c[0x11];      // 0x13c
    f32 mMotives[16];         // 0x180  scope 0xe, read through GetMotive
    u32 pad_0x1c0[0x10];      // 0x1c0
    eastl::vector<MotiveInc, eastl::allocator> mMotiveIncs; // 0x200
    u32 pad_0x210[3];         // 0x210
    ActionQueue mActionQueue; // 0x21c
    u32 pad_0x528[4];         // 0x528
    u32 field_0x538;          // 0x538  IsIdle clears bit 0 of it
    u32 pad_0x53c[3];         // 0x53c
    s16 mPersonData[0x24];    // 0x548  scopes 0x12, 0x13 and 0x30 to 0x33
    Neighbor *mNeighbor;      // 0x590
    u32 pad_0x594[2];         // 0x594
    eastl::vector<FTilePt, eastl::allocator> mPath; // 0x59c  8-byte tile points
    u32 pad_0x5ac[9];         // 0x5ac
    eastl::vector<XRoute, eastl::allocator> mRoutes; // 0x5d0  the last is current
    int field_0x5e0;          // 0x5e0  XRoute::Construct clears it
    u32 pad_0x5e4;            // 0x5e4
    int mInRoutingQueue;      // 0x5e8  Wii: a byte at +0x5DA
    u32 pad_0x5ec;            // 0x5ec
    PersonAnimator *mAnimator; // 0x5f0
    s16 mIdleLoopCount;       // 0x5f4

    static int sbAllowingCheckTreeAds;

    long TryGotoRoutingSlot(StackElem *elem, XPrimParam *primParam);
    long TrySetMotiveDelta(StackElem *elem, XPrimParam *primParam);
    long TryGotoRelative(StackElem *elem, XPrimParam *primParam);
    long TryIdleForInput(StackElem *elem, XPrimParam *primParam);
    int ShouldInterrupt();
    long executeInterruptOnIdlePrimitve(StackElem *elem, s16 *counter, bool allow);
    long GotoRoutingSlot(cXObject *object, const RoutingSlot *slot, int &state);
    int IsModelLoaded();
    int IsIdle();
    void SetCurrentActionText(const BString2 &text);
    int AddAction(Interaction *action);

    // The routing state machine: each handler returns the next state. bhav.md has the table.
    enum RoutingState {
        kRoutingInitial = 1,
        kRoutingWaitingForPortal,
        kRoutingWalking,
        kRoutingWaitingForSit,
        kRoutingWaitingForStand,
        kRoutingTrapped,
        kRoutingFailed,
        kRoutingSucceeded,
        kRoutingWalkingSucceeded,
        kRoutingWalkingFailed,
        kRoutingWalkTerminated,
        kRoutingFoundPath,
        kRoutingWaiting,
        kRoutingWaitingForPersonToMove,
        kRoutingRouteInvalidatedWhileWalking,
        kRoutingNeedFailureFeedback,
        kRoutingWaitingForFailureFeedback,
        kRoutingFoundGoals,
        kRoutingObjectDeleted,
        kRoutingObjectDeletedWhileWalking
    };
    XRoute *GetCurrentRoute();
    void CleanUpAfterFinishingRoute(RoutingState state);
    int TryRoomRouting(XRoute *route);
    void SetPersonData(int index, s16 value);
    RoutingState Routing_WaitingForPortal(bool &done, TreeReturnCode &code);
    RoutingState Routing_Walking(bool &done, TreeReturnCode &code);
    RoutingState Routing_Waiting(bool &done, TreeReturnCode &code);
    RoutingState Routing_NeedFailureFeedback(bool &done, TreeReturnCode &code);
    RoutingState Routing_Failed(bool &done, TreeReturnCode &code);
    RoutingState Routing_FoundGoals(bool &done, TreeReturnCode &code);
    RoutingState Routing_Initial(bool &done, TreeReturnCode &code);
    // Wii: IsGhost, inlined on PS2: person data 0xC, and bit 2 of the neighbour's word 0x2A.
    bool IsGhost() { return mPersonData[0xc] ? ((u16)mNeighbor->mPersonData[0x2a] >> 2) & 1 : 0; }
    RoutingState Routing_Succeeded(bool &done, TreeReturnCode &code);
    RoutingState Routing_Trapped(bool &done, TreeReturnCode &code);
    int AskOthersToMove(XRoute *route);
    RoutingState Routing_WaitingForPersonToMove(bool &done, TreeReturnCode &code);
    TreeReturnCode InitRoute(XRoute *route);
    static void PopFromRoutingQueue(cXPerson *person);
    static RoutingQueue s_RoutingQueue;
    static int s_nNumRoutedThisFrame;
    static bool IsRoutingQueueEmpty() { return s_RoutingQueue.mTail == s_RoutingQueue.mHead; }
    // Wii: CanRouteThisFrame, inlined on PS2. One Sim routes a frame, in queue order.
    bool CanRouteThisFrame()
    {
        if (s_nNumRoutedThisFrame < 1 && (IsRoutingQueueEmpty() || this == s_RoutingQueue.Peek(0)))
            return true;
        return false;
    }
    RoutingState Routing_WaitingForStand(bool &done, TreeReturnCode &code);
    RoutingState Routing_WaitingForSit(bool &done, TreeReturnCode &code);
    RoutingState Routing_WalkingSucceeded(bool &done, TreeReturnCode &code);
    RoutingState Routing_WalkingFailed(bool &done, TreeReturnCode &code);
    RoutingState Routing_WalkTerminated(bool &done, TreeReturnCode &code);
    RoutingState Routing_FoundPath(bool &done, TreeReturnCode &code);
    RoutingState Routing_RouteInvalidatedWhileWalking(bool &done, TreeReturnCode &code);
    RoutingState Routing_WaitingForFailureFeedback(bool &done, TreeReturnCode &code);
    RoutingState Routing_ObjectDeleted(bool &done, TreeReturnCode &code);
    RoutingState Routing_ObjectDeletedWhileWalking(bool &done, TreeReturnCode &code);
    // Wii: IsIdling, inlined on PS2.
    int GetIdleState() const { return mPersonData[0]; }
    bool IsIdling() const
    {
        int idle = 0;
        if (GetIdleState() == 0 || GetIdleState() == 3)
            idle = 1;
        return idle;
    }
    int RunImmediateAction(Interaction *action);
    int MoveOutOfWay(int priority);
    int GetSimType();
    Interaction *GetCurrentAction();
    void ActionSkipped(const Interaction &action);
    u32 GetPendingAction() const { return field_0x538 ? 1 : 0; }


    u32 ReconType();

    s16 *GetPersonDataArray() { return mPersonData; }
    Neighbor *GetNeighbor() const { return mNeighbor; }
    s16 GetNeighborID() const { return mPersonData[12]; }
    void *GetSimDescription() { return mSelector->mSimDescription; }
    f32 GetMotive(int index);
    f32 *GetMotiveRef(int index);
    static bool isValidPersonDataField(s16 index);
    static int TranslasteToNewPersonDataIndex(int index, bool toNew);
};

#endif
