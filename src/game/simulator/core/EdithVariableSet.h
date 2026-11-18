#ifndef GAME_SIMULATOR_CORE_EDITHVARIABLESET_H
#define GAME_SIMULATOR_CORE_EDITHVARIABLESET_H

#include "common/types.h"

// An object's named variables, scope 3 and 4 of an instruction. 0x30 bytes, from the offsets
// of the fields either side of it in cXObject; what it holds isn't worked out yet.
class cXObject;

class EdithVariableSet {
public:
    u32 pad_0x0[0xc];

    static int s_bTaggedGraphicField;

    const s16 &operator[](int index) const;
    int Contains(int index);
    const s16 *WriteVar(int index, s16 value);
    void Initialize(cXObject *object); // 0x214FB8
    // Reset keeps a copy across Reset() to restore nine variables from it. The default
    // constructor is only declared so that the object classes stay constructible.
    EdithVariableSet();
    EdithVariableSet(const EdithVariableSet &other); // 0x214EB0
    ~EdithVariableSet();                             // 0x254228
    void Reset();                                    // 0x2190E0
};

#endif
