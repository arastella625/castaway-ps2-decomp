#ifndef GAME_SIMULATOR_CORE_RELMATRIX_H
#define GAME_SIMULATOR_CORE_RELMATRIX_H

#include "common/types.h"

class ReconBuffer;

// Wii: RelMatrix, the relationships an object holds. The Wii vtable is pure virtual apart
// from the destructor, so only the call cXObject::ReconStream makes is placed: the tenth
// entry, +0x50 on PS2. Its name is ours, from the arguments.
class RelMatrix {
public:
    virtual ~RelMatrix();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void ReconStream(ReconBuffer *buffer, int version);
};

#endif
