#ifndef GAME_SIMULATOR_CORE_CXMTOBJECT_H
#define GAME_SIMULATOR_CORE_CXMTOBJECT_H

#include "game/simulator/core/cXObject.h"

// Wii: CardinalRotateCoords<T>. Turns (x, y) by a direction in eighths: 0 leaves it, 2 gives
// (-y, x), 4 (-x, -y) and 6 (y, -x). Odd directions give (0, 0).
template <class T>
inline void CardinalRotateCoords(int direction, T &x, T &y)
{
    T x0 = x;
    T y0 = y;
    y = 0;
    x = 0;
    switch (direction & 7) {
    case 0:
        x = x0;
        y = y0;
        break;
    case 2:
        y = x0;
        x = -y0;
        break;
    case 4:
        x = -x0;
        y = -y0;
        break;
    case 6:
        x = y0;
        y = -x0;
        break;
    }
}

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
