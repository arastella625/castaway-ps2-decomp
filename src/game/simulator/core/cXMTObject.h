#ifndef GAME_SIMULATOR_CORE_CXMTOBJECT_H
#define GAME_SIMULATOR_CORE_CXMTOBJECT_H

#include "game/simulator/core/cXObject.h"

// A multi-tile object: one of these for each tile, in a list from the first part.
class cXMTObject : public cXObject {
public:
    cXMTObject *mNextPart;  // 0x13c
    cXMTObject *mFirstPart; // 0x140

    virtual u32 ReconType();

    cXMTObject *GetFirstMultiTileObject() { return mFirstPart ? mFirstPart : this; }
};

#endif
