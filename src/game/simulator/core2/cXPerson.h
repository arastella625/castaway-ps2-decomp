#ifndef GAME_SIMULATOR_CORE2_CXPERSON_H
#define GAME_SIMULATOR_CORE2_CXPERSON_H

#include "common/types.h"
#include "game/simulator/core/Interaction.h"
#include "game/simulator/core/ObjSelector.h"
#include "game/simulator/core/cXObject.h"

class Neighbor;

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
    int Insert(const Interaction &action, u32 index);
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
class PersonAnimator {
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
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void Slot16();
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

// A Sim. The fields are the ones InterpValue reads.
class cXPerson : public cXObject {
public:
    u32 pad_0x13c[0x11];      // 0x13c
    f32 mMotives[16];         // 0x180  scope 0xe, read through GetMotive
    u32 pad_0x1c0[0x10];      // 0x1c0
    eastl::vector<MotiveInc, eastl::allocator> mMotiveIncs; // 0x200
    u32 pad_0x210[3];         // 0x210
    ActionQueue mActionQueue; // 0x21c
    u32 pad_0x524[5];         // 0x524
    u32 field_0x538;          // 0x538  IsIdle clears bit 0 of it
    u32 pad_0x53c[3];         // 0x53c
    s16 mPersonData[0x24];    // 0x548  scopes 0x12, 0x13 and 0x30 to 0x33
    Neighbor *mNeighbor;      // 0x590
    u32 pad_0x594[0x17];      // 0x594
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
