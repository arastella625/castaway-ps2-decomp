#include "game/simulator/core2/iResFile.h"

s32 iResFile::GetError()
{
    return field_0x4;
}

void iResFile::SetError(int value)
{
    field_0x4 = value;
}
