#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

// Fixed-width names for the EE's data model: int and pointers are 32 bits, long is 64.
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef short s16;
typedef int s32;
typedef float f32;

// The compiler predates static_assert. A false condition gives an array of size -1,
// which fails the build, so a wrong struct layout can't slip through.
#define STATIC_ASSERT(name, cond) typedef char static_assert_##name[(cond) ? 1 : -1]

#endif
