#ifndef COMMON_ERESOURCEMANAGER_H
#define COMMON_ERESOURCEMANAGER_H

#include "common/types.h"

// Wii: EResourceManager. Only what the decompiled code uses is declared so far.
class EResourceManager {
public:
    // Turns a resource name into its id. The meaning of the flag is unknown.
    static u32 CalcId(const char *name, bool a);
};

#endif
