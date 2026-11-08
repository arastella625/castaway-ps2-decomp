#include "game/animation/SAnimator2.h"

// Wii: AdultSimAnimator, a grown-up Sim's animator (PS2 vtable 0x4A7B18). Only what's decompiled.
class AdultSimAnimator : public SAnimator2 {
public:
    f32 GetMovementVelocityFromStyle(SAnimator2::eWalkRunStyle style); // vtable slot 53
};

// Always 1: the factor every route speed and walk clip rate is multiplied by.
f32 SAnimator2::GetTrackScaleVelocityFactor()
{
    return 1.0f;
}

// A route speed for each walk/run style, in sixteenths of a tile per second. Walk is 28.8 (1.8
// tiles a second) and run 64 (4 tiles); Wii's tuning has 60.8 for run. Styles 3 and 4 use the
// speed measured from their clips.
f32 AdultSimAnimator::GetMovementVelocityFromStyle(SAnimator2::eWalkRunStyle style)
{
    f32 velocity = 28.8f;
    switch (style) {
    case 0:
    case 8:
    case 9:
        velocity = 28.8f;
        break;
    case 3:
        velocity = mClipVelocity[5];
        break;
    case 4:
        velocity = mClipVelocity[6];
        break;
    case 1:
    case 2:
        velocity = 64.0f;
        break;
    case 5:
    case 7:
        velocity = 0.0f;
        break;
    case 6:
        velocity = 24.0f;
        break;
    }
    return velocity;
}
