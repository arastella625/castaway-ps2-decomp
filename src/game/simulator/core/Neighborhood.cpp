#include "game/simulator/core/Neighborhood.h"

int Neighborhood::GetNumFamilies()
{
    return mFamilies.size();
}

Family *Neighborhood::GetFamilyByIndex(int index)
{
    return mFamilies[index];
}

// Not matched: 6 of 15 words, all register names. The game keeps the list's start in v1 and
// runs the index through v0 after the size test, and leaves the branch's delay slot empty
// where this loads the start into it. Eight spellings tried (two tests or one, a named
// result, a local set on both paths, begin()[id - 1], a signed size, the index computed
// first, a pointer to the slot); the local set on both paths gives the game's registers but
// adds a copy at the end. The same lookup is inlined in InterpValue, TryLotTransition and
// TryFamilyInventory, where the game copies the result through a temporary.
#ifdef NON_MATCHING
// The neighbour with this ID, which is its place plus one, or null.
Neighbor *Neighborhood::FindNeighborByID(int id)
{
    if (id <= 0 || mNeighbors.size() < (unsigned int)id)
        return 0;
    return mNeighbors[id - 1];
}
#endif
