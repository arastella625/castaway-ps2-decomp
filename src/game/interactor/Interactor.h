#ifndef GAME_INTERACTOR_INTERACTOR_H
#define GAME_INTERACTOR_INTERACTOR_H

#include "common/types.h"

class ESimsCam;
class Interaction;
class cXObject;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
namespace InteractorModule {
class Interactor {
public:
    u8 pad_0x0[0x4];
    ESimsCam *field_0x4; // 0x04

    void ExecuteObjectActionMenu(cXObject *a);
    void StopInObjectMenu();
    int ChooseAction(Interaction *a);
    void AttachCamera(ESimsCam *value);
    ESimsCam *GetAttachedCamera();
};
}

#endif
