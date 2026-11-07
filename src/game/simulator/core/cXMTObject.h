#ifndef GAME_SIMULATOR_CORE_CXMTOBJECT_H
#define GAME_SIMULATOR_CORE_CXMTOBJECT_H

#include "game/simulator/core/cXObject.h"

// A multi-tile object: one of these for each tile, linked into a loop.
class cXMTObject : public cXObject {
public:
    cXMTObject *mChild;   // 0x13c
    cXMTObject *mSibling; // 0x140

    virtual u32 ReconType();

    cXMTObject *GetNextPart() { return mChild ? mChild : mSibling; }
};

#endif
