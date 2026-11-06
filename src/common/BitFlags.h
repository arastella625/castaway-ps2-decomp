#ifndef COMMON_BITFLAGS_H
#define COMMON_BITFLAGS_H

#include "common/types.h"

// Wii: BitFlags<unsigned int>. Declared without the template until a second
// instantiation turns up on PS2.
struct BitFlags {
    u32 bits;

    bool Get(int index) const;
};
STATIC_ASSERT(BitFlags_size, sizeof(BitFlags) == 0x4);

#endif
