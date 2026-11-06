#include "game/level/EResource.h"

bool EResource::IsSafeToDelete()
{
    return true;
}

void EResource::Init()
{
}

void EResource::Refresh(EFile *a)
{
}

void EResource::DelRefSubResources()
{
}

void EResource::AddRefSubResources()
{
}

int EResource::TryIncrementSubResources()
{
    return 1;
}

void EResource::DetachClone()
{
}
