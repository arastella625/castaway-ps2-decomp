#ifndef COMMON_RECONBUFFER_H
#define COMMON_RECONBUFFER_H

#include "common/types.h"

// Wii: ReconBuffer. Streams values in or out, depending on its mode. Only what the
// decompiled code uses is declared so far.
class ReconBuffer {
public:
    u8 field_0x0[0xc]; // 0x00
    s32 field_0xc;     // 0x0c  zero while reading

    void Recon16(s16 *values, int count);
    void Recon32(int *values, int count);
    void ReconFloat(f32 *values, int count);
    void Recon8(s8 *values, int count);
    void ReconBool(bool *value);
    // The PS2 build inlines this.
    bool IsReading() { return field_0xc == 0; }
};

#endif
