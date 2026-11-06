#ifndef GAME_SIMULATOR_CORE2_SLOT_H
#define GAME_SIMULATOR_CORE2_SLOT_H

#include "common/types.h"

class SlotDescriptor;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class Slot {
public:
    void WriteSlot(SlotDescriptor *a);
};

#endif
