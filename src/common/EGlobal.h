#ifndef COMMON_EGLOBAL_H
#define COMMON_EGLOBAL_H

#include "common/types.h"

class ERQuickdata;

// Wii: ELocString, a localised string, with __opPCUs__10ELocStringCFv to read it. On PS2
// it is one pointer, returned in a register, and reading it checks that pointer for null.
// The entry it points at starts with the characters; the entry's name is ours.
struct ELocEntry {
    const u16 *characters; // 0x00
};

class ELocString {
public:
    const ELocEntry *entry; // 0x00

    operator const u16 *() const { return entry != 0 ? entry->characters : 0; }
};

// Wii: EGlobal and its instance _globals. Only what the decompiled code uses is declared.
class EGlobal {
public:
    u8 field_0x0[0x1a8];       // 0x000
    ERQuickdata *casQuickdata; // 0x1a8  the Create-a-Sim quickdata once it is loaded
    u8 field_0x1ac[0x29c];     // 0x1ac
    int useGameStateTextures;  // 0x448  when set, texture ids are rehashed with "_GAME"

    ELocString GetNewUIString(const char *name, const char *a);
};
extern EGlobal _globals;

// Wii: ewcsncpy__FPUsPCUsUl, strncpy for 16-bit characters.
u16 *ewcsncpy(u16 *to, const u16 *from, unsigned long count);

#endif
