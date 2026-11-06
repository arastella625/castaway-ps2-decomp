#include "game/simulator/core2/QuickDataSoundInfo.h"

int QuickDataSoundInfo::LoadFromIndex(u32 a, int b)
{
    return 0;
}

int QuickDataSoundInfo::SaveDataByID(const ObjectDataID &a)
{
    return 0;
}

int QuickDataSoundInfo::GetDataSourceType() const
{
    return 1;
}

int QuickDataSoundInfo::LoadOnlyNameAndIDFromIndex(u32 a, int b)
{
    return 0;
}

void QuickDataSoundInfo::AddRef()
{
    field_0x58++;
}
