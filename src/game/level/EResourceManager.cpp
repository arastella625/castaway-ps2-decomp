#include "game/level/EResourceManager.h"

int EResourceManager::GetAllocationFlags() const
{
    return 0;
}

bool EResourceManager::CanCache()
{
    return false;
}
