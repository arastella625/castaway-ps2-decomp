#ifndef GAME_SIMULATOR_CORE_CXMTOBJECT_H
#define GAME_SIMULATOR_CORE_CXMTOBJECT_H

#include "game/simulator/core/cXObject.h"

// A multi-tile object: one of these for each tile, in a list from the first part.
class cXMTObject : public cXObject {
public:
    cXMTObject *mNextPart;  // 0x13c
    cXMTObject *mFirstPart; // 0x140
    int mPartPos[3];        // 0x144  x, y and the third coordinate of this part
    int mPartOffset[3];     // 0x150  AssignOffsets: from the first part, turned by its direction

    virtual u32 ReconType();
    virtual void AssignOffsets();
    virtual FTilePt GetAverageLocation() const;
    virtual cXObject *GetAnchorTile();

    cXMTObject *GetFirstMultiTileObject() { return mFirstPart ? mFirstPart : this; }
};

#endif
