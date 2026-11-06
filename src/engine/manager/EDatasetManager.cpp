#include "engine/manager/EDatasetManager.h"

bool EDatasetManager::CanCache(EResource *a)
{
    return true;
}

void EDatasetManager::SetLoadProgress(f32 value)
{
    field_0xd44 = value;
}
