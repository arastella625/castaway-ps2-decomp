#include "game/animation/SAnimator2.h"

#include "game/simulator/core2/StackElem.h"

// Primitive 0x2c on a Sim. It returns 2 and waits while an animation plays. An animation event
// is a return of 0, and the end of the animation a return of 1.
long SAnimator2::TryAnimate(StackElem *elem, const AnimateNewParam *param)
{
    long result = -1;
    int event = 0;
    const AnimRef *ref = 0;
    bool second = false;
    int behavior = param->GetBehavior();

    if (!mESim->mModelLoaded)
        return 2;
    if (!PendingAnimationLoaded(result, param))
        return result;
    bool preloaded;
    result = PreloadOnly(preloaded, param, ref, elem);
    if (preloaded)
        return result;
    s16 anim = param->mAnim;
    if (mESim->HasQueuedOperation() || mESim->mPendingAnim != -1) {
        result = 2;
        UpdateAnimationValidity();
        return result;
    }
    mAnimFlags &= ~0x2000;
    if (param->GetUsesStackVar())
        anim = elem->GetParams()[0];
    mESim->DrawSim(true);
    switch (behavior) {
    case 1:
        return BackgroundBehavior(param, anim, ref);
    case 4:
        return FXAnimationBehavior(param, anim, ref, elem);
    case 2:
    case 5:
        return CarryBehavior(param, anim, ref, elem);
    case 3:
        second = true;
        break;
    }
    if (anim == 0)
        return TurnOffAnim();
    mAnimating = 1;
    if (ProcessAnimatePrimitiveNotEntered(result, param, ref, elem, second, param->GetMirrored()))
        return result;
    if (DequeueAnimEvent(&event))
        return ProcessDequeueEvent(param, event, elem);
    return ProcessNoDequeueEvent(param, event, elem);
}
