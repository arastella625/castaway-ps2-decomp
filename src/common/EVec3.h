#ifndef COMMON_EVEC3_H
#define COMMON_EVEC3_H

#include "common/types.h"

struct EVec3 {
    f32 x, y, z;

    EVec3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}
};
STATIC_ASSERT(EVec3_size, sizeof(EVec3) == 0xc);

#endif
