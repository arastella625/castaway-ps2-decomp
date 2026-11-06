#include "game/simulator/objectdata/TreeTableQuickData.h"

int TreeTableQuickData::LoadFromIndex(u32 a, int b)
{
    return 0;
}

int TreeTableQuickData::GetResourceName(StringBuffer &a) const
{
    return 0;
}

int TreeTableQuickData::SetResourceName(const StringBuffer &a)
{
    return 0;
}

int TreeTableQuickData::LoadOnlyNameAndIDFromIndex(u32 a, int b)
{
    return 0;
}

void TreeTableQuickData::SwapEntries(int a, int b)
{
}

int TreeTableQuickData::GetNthEntry(int a)
{
    return 0;
}

int TreeTableQuickData::GetEntryByIndex(int a)
{
    return 0;
}

int TreeTableQuickData::GetNthOrderedEntry(int a)
{
    return 0;
}

void TreeTableQuickData::SetName(const StringBuffer &a)
{
}

bool TreeTableQuickData::AreOrderIndicesValid() const
{
    return true;
}

int TreeTableQuickData::GetIsTuned() const
{
    return 0;
}

void TreeTableQuickData::SetIsTuned(bool a)
{
}

int TreeTableQuickData::AddEntry(s16 a, s16 b)
{
    return 0;
}

int TreeTableQuickData::RemoveEntry(const ITreeTableEntry *a)
{
    return 0;
}

void TreeTableQuickData::SetPrefixCheckTreeID(s16 a)
{
}

int TreeTableQuickData::SaveDataByID(const ObjectDataID &a)
{
    return 0;
}

int TreeTableQuickData::GetDataSourceType() const
{
    return 1;
}

void TreeTableQuickData::AddRef()
{
    field_0x20++;
}
