#ifndef GAME_ANIMATION_SANIMATOR2_H
#define GAME_ANIMATION_SANIMATOR2_H

// The vtable stays an external symbol.
#pragma interface

#include "common/types.h"

class AnimRef;
class StackElem;

// The operands of primitive 0x2c, Animate.
class AnimateNewParam {
public:
    s16 mAnim;    // 0x00  the animation; 0 turns it off
    u8 mEventLocal; // 0x02  where events go, if bit 5 of mFlags is set
    u8 mAnimType; // 0x03  bit 7 mirrored, the rest a behaviour above 3
    u8 field_0x4;
    u8 mFlags;    // 0x05  bit 0 background, 1 backwards, 2 anim in parameter 0, 4 carry,
                  //       5 events to a local, 7 preload only

    int GetBehavior() const
    {
        int behavior = 0;
        int group = mAnimType & 0x7f;
        if (group) {
            behavior = group + 3;
        } else {
            if (mFlags & 1)
                behavior |= 1;
            if (mFlags & 0x10)
                behavior |= 2;
        }
        return behavior;
    }
    int GetUsesStackVar() const { return (mFlags >> 2) & 1; }
    int GetMirrored() const { return mAnimType >> 7; }
};

// Wii: ESim, what draws a Sim. Only what TryAnimate reads.
class ESim {
public:
    u32 pad_0x0[0x1da];       // 0x000
    int mPendingAnim;         // 0x768
    u32 pad_0x76c[0xc];       // 0x76c
    int mModelLoaded;         // 0x79c

    int HasQueuedOperation() const;
    void DrawSim(bool draw);
};

// Wii: SAnimator2's virtual functions in vtable order, as far as TryAnimate reaches. The vptr is
// at offset 0, which needs a base holding only that.
class SAnimatorVirtuals {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual long TryAnimate(StackElem *elem, const AnimateNewParam *param);
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void Slot16();
    virtual int DequeueAnimEvent(int *event);
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
    virtual void Slot32();
    virtual void Slot33();
    virtual void Slot34();
    virtual void Slot35();
    virtual void Slot36();
    virtual void Slot37();
    virtual void Slot38();
    virtual void Slot39();
    virtual void Slot40();
    virtual void Slot41();
    virtual void Slot42();
    virtual void Slot43();
    virtual void Slot44();
    virtual void Slot45();
    virtual void Slot46();
    virtual void Slot47();
    virtual void Slot48();
    virtual void Slot49();
    virtual void Slot50();
    virtual void Slot51();
    virtual void Slot52();
    virtual void Slot53();
    virtual void Slot54();
    virtual void Slot55();
    virtual void Slot56();
    virtual void Slot57();
    virtual void Slot58();
    virtual void Slot59();
    virtual void Slot60();
    virtual void Slot61();
    virtual long CarryBehavior(const AnimateNewParam *param, s16 anim, const AnimRef *&ref, StackElem *elem);
};

// Runs a Sim's animations. The field offsets are PS2's.
class SAnimator2 : public SAnimatorVirtuals {
public:
    u32 pad_0x4[0x24];   // 0x004
    ESim *mESim;         // 0x094
    u32 pad_0x98[0x68];  // 0x098
    int mAnimating;      // 0x238
    u32 pad_0x23c[0x11c]; // 0x23c
    u32 mAnimFlags;      // 0x6ac

    virtual long TryAnimate(StackElem *elem, const AnimateNewParam *param);

    int PendingAnimationLoaded(long &result, const AnimateNewParam *param);
    long PreloadOnly(bool &preloaded, const AnimateNewParam *param, const AnimRef *&ref, StackElem *elem);
    void UpdateAnimationValidity();
    long BackgroundBehavior(const AnimateNewParam *param, s16 anim, const AnimRef *&ref);
    long FXAnimationBehavior(const AnimateNewParam *param, s16 anim, const AnimRef *&ref, StackElem *elem);
    long TurnOffAnim();
    int ProcessAnimatePrimitiveNotEntered(long &result, const AnimateNewParam *param, const AnimRef *&ref,
                                          StackElem *elem, bool a, bool mirrored);
    long ProcessDequeueEvent(const AnimateNewParam *param, int event, StackElem *elem);
    long ProcessNoDequeueEvent(const AnimateNewParam *param, int event, StackElem *elem);
};

#endif
