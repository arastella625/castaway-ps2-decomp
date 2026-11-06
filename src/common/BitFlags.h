#ifndef COMMON_BITFLAGS_H
#define COMMON_BITFLAGS_H

#include "common/types.h"

// Wii: BitFlags<unsigned int>. Declared without the template until a second
// instantiation turns up on PS2.
struct BitFlags {
    u32 bits;

    // Wii: __ct__12BitFlags<Ui>FUi. The default of 0 is a guess.
    BitFlags(u32 bits_ = 0) : bits(bits_) {}
    // The game returns BitFlags through a hidden pointer, which this compiler only does for
    // a 4-byte class that has its own copy constructor or destructor. Which one is a guess.
    BitFlags(const BitFlags &other) : bits(other.bits) {}
    bool Get(int index) const;
};
STATIC_ASSERT(BitFlags_size, sizeof(BitFlags) == 0x4);

#endif
