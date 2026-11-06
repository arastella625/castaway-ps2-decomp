#ifndef GAME_SIMULATOR_CORE_NEIGHBORHOOD_H
#define GAME_SIMULATOR_CORE_NEIGHBORHOOD_H

#include "common/types.h"

class Family;

// Wii: Neighbor. Only what the decompiled code calls is declared so far.
class Neighbor {
public:
    Family *GetFamily();
};

// Wii: Neighborhood.
class Neighborhood {
public:
    Neighbor *FindNeighborByID(int id);
};

// Wii: pNeighborhood__5Globs, read through GetNeighborhood__Fv, which the PS2 build inlines.
class Globs {
public:
    static Neighborhood *pNeighborhood;
};

inline Neighborhood *GetNeighborhood()
{
    return Globs::pNeighborhood;
}

#endif
