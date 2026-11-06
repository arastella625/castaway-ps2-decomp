#ifndef COMMON_TRECT_H
#define COMMON_TRECT_H

#include "common/types.h"

// Wii: TRect<unsigned short>, built by __ct__9TRect<Us>Fiiii. The field names are ours:
// the values passed by SimBodyPart::GetLayoutMapRect read as two corners in a texture.
template <class T>
struct TRect {
    T left, top, right, bottom;

    TRect(int left_, int top_, int right_, int bottom_)
        : left(left_), top(top_), right(right_), bottom(bottom_) {}
};
STATIC_ASSERT(TRect_u16_size, sizeof(TRect<u16>) == 0x8);

#endif
