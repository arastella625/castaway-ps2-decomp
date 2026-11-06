#ifndef GAME_INTERACTOR_SOCIALMODEINTERACTOR_H
#define GAME_INTERACTOR_SOCIALMODEINTERACTOR_H

#include "common/types.h"

namespace InteractorModule { class InteractorInfo; }
namespace InteractorModule { class InteractorVisualizer; }

namespace InteractorModule {
// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class SocialModeInteractor {
public:
    u8 pad_0x0[0x6c];
    s16 field_0x6c; // 0x6c

    void OnCreate();
    void OnDestroy();
    void GetInteractorInfo(InteractorModule::InteractorInfo &a);
    void Draw(InteractorModule::InteractorVisualizer &a);
};
}

#endif
