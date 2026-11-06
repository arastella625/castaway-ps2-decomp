#ifndef GAME_SIMULATOR_CORE_EDITHVARIABLESET_H
#define GAME_SIMULATOR_CORE_EDITHVARIABLESET_H

#include "common/types.h"

// An object's named variables, scope 3 and 4 of an instruction. 0x30 bytes, from the offsets
// of the fields either side of it in cXObject; what it holds isn't worked out yet.
class EdithVariableSet {
public:
    u32 pad_0x0[0xc];

    const s16 *operator[](int index) const;
    int Contains(int index);
    const s16 *WriteVar(int index, s16 value);
};

#endif
