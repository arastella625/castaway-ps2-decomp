#ifndef COMMON_EVEC3_H
#define COMMON_EVEC3_H

#include "common/types.h"

struct EVec3 {
    f32 x, y, z;

    EVec3() {}
    EVec3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}
    EVec3 &operator+=(const EVec3 &other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
};
STATIC_ASSERT(EVec3_size, sizeof(EVec3) == 0xc);

// Rows of four. Only what routing and Snap use.
struct EMat4 {
    f32 m[4][4];

    EVec3 operator*(const EVec3 &v) const
    {
        return EVec3(v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0],
                     v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1],
                     v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2]);
    }
};

inline EVec3 operator*(const EVec3 &v, const EMat4 &mat)
{
    return EVec3(v.x * mat.m[0][0] + v.y * mat.m[1][0] + v.z * mat.m[2][0] + mat.m[3][0],
                 v.x * mat.m[0][1] + v.y * mat.m[1][1] + v.z * mat.m[2][1] + mat.m[3][1],
                 v.x * mat.m[0][2] + v.y * mat.m[1][2] + v.z * mat.m[2][2] + mat.m[3][2]);
}

#endif
