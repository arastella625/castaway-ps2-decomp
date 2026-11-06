#include "game/simulator/objectdata/IFFBehaviorTree.h"

int IFFBehaviorTree::GetDataSourceType() const
{
    return 2;
}

void IFFBehaviorTree::AddRef()
{
    field_0x60++;
}
