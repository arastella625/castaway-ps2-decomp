#include "common/BitFlags.h"

// The game has one copy of operator[] for each of its two instantiations. They are written
// as explicit specialisations because an ordinary template instantiation goes in a linkonce
// section, which tools/verify.py does not check.
template <>
bool BitFlags<u32>::operator[](int index) const
{
    return (bits >> index) & 1;
}

template <>
bool BitFlags<u16>::operator[](int index) const
{
    return (bits >> index) & 1;
}
