#include "game/simulator/core/cXMTObject.h"

u32 cXMTObject::ReconType()
{
    return 0x584d544f;
}

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

// Each part's offset from the first part, turned by the first part's direction (its data 1).
// Each part's own data 1 is read too, and not used.
void cXMTObject::AssignOffsets()
{
    cXMTObject *first = mFirstPart;
    if (!first)
        first = this;
    int x = first->mPartPos[0];
    int y = first->mPartPos[1];
    int z = first->mPartPos[2];
    int direction = first->mVars[1];
    for (cXMTObject *part = first; part; part = part->mNextPart) {
        part->mPartOffset[0] = part->mPartPos[0] - x;
        part->mPartOffset[1] = part->mPartPos[1] - y;
        part->mPartOffset[2] = part->mPartPos[2] - z;
        part->mVars[1];
        CardinalRotateCoords(direction, part->mPartOffset[0], part->mPartOffset[1]);
    }
}
