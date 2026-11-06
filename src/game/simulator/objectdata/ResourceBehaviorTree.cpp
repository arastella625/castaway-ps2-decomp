#include "game/simulator/objectdata/ResourceBehaviorTree.h"

int ResourceBehaviorTree::GetDataSourceType() const
{
    return 8;
}

int ResourceBehaviorTree::SaveDataByID(const ObjectDataID &a)
{
    return 0;
}

void ResourceBehaviorTree::AddRef()
{
    field_0x64++;
}
