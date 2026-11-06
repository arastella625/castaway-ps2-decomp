#ifndef COMMON_BITFLAGS_H
#define COMMON_BITFLAGS_H

#include "common/types.h"

// Wii: BitFlags<unsigned int> and BitFlags<unsigned short>.
template <class T>
struct BitFlags {
    T bits;

    // Wii: __ct__12BitFlags<Ui>FUi. The default of 0 is a guess.
    BitFlags(T bits_ = 0) : bits(bits_) {}
    // The game returns BitFlags through a hidden pointer, which this compiler only does for
    // a class with its own copy constructor. A destructor alone doesn't (tested with EString).
    BitFlags(const BitFlags &other) : bits(other.bits) {}
    // Wii: __vc__12BitFlags<Ui>CFi. Defined in BitFlags.cpp; the game keeps one copy per T.
    bool operator[](int index) const;
    // Wii: __opUi__12BitFlags<Ui>CFv and setMask__12BitFlags<Ui>FUi.
    operator T() const { return bits; }
    void setMask(T mask) { bits |= mask; }
    // Wii: setFlag__12BitFlags<Ui>Fib. Inlined everywhere in the PS2 game.
    void setFlag(int index, bool value)
    {
        if (value)
            bits |= 1 << index;
        else
            bits &= ~(1 << index);
    }
};
STATIC_ASSERT(BitFlags_u32_size, sizeof(BitFlags<u32>) == 0x4);
STATIC_ASSERT(BitFlags_u16_size, sizeof(BitFlags<u16>) == 0x2);

#endif
