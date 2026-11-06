#include "game/simulator/core2/TreeSim.h"

u32 TreeSim::GetPortal() const
{
    return field_0xc;
}

u32 TreeSim::GetPerson() const
{
    return field_0x4;
}

u32 TreeSim::GetMTObject() const
{
    return field_0x8;
}

u32 TreeSim::GetObject() const
{
    return field_0x0;
}

void TreeSim::StackJustPopped()
{
}

u32 TreeSim::GetIterations()
{
    return field_0x18;
}

u32 TreeSim::GetLastResult()
{
    return field_0x40;
}
