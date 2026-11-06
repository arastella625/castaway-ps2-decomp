#ifndef COMMON_ERQUICKDATA_H
#define COMMON_ERQUICKDATA_H

#include "common/types.h"

class EFile;

// One table of a quickdata resource. Only what the decompiled code reads is known.
struct ERQTable {
    u32 field_0x0; // 0x00
    void *rows;    // 0x04  the first row
    u32 field_0x8; // 0x08
    u32 count;     // 0x0c
};

// Wii: ERQuickdata, a resource of named tables with named rows. The Wii build looks tables
// and rows up through templates with an out-parameter (GetTable<T>, GetRow<T>). The PS2
// build calls two plain functions that return the result, at 0x455030 and 0x455140; their
// names here are ours.
class ERQuickdata {
public:
    const ERQTable *GetTable(const char *typeName);
    const void *GetRow(const ERQTable *table, const char *rowName);
};

// Wii: EQuickdataManager and its global _quickdataman.
class EQuickdataManager {
public:
    ERQuickdata *AddRef(u32 id, EFile *file, int a);
};
extern EQuickdataManager _quickdataman;

#endif
