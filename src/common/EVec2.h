#ifndef COMMON_EVEC2_H
#define COMMON_EVEC2_H

#include "common/types.h"

struct EVec2 {
    f32 x, y;

    EVec2() {}
    EVec2(f32 x_, f32 y_) : x(x_), y(y_) {}
};
STATIC_ASSERT(EVec2_size, sizeof(EVec2) == 8);

// Wii: CollisionMap, the free-movement grid. Only the check TestPlace makes is declared.
class CollisionMap {
public:
    // Wii: IsLocationGood(const EVec2 &, bool ghost, bool).
    static bool IsLocationGood(const EVec2 &location, bool ghost, bool a); // 0x3157A8
};

#endif
