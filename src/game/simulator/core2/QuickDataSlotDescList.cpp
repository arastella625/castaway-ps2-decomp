#include "game/simulator/core2/QuickDataSlotDescList.h"

int QuickDataSlotDescList::AddSlot(SlotDescriptor &a)
{
    return 0;
}

int QuickDataSlotDescList::RemoveSlot(int a)
{
    return 0;
}

int QuickDataSlotDescList::GetStringSetID() const
{
    return 0;
}

void QuickDataSlotDescList::SetStringSetID(int a)
{
}

int QuickDataSlotDescList::SaveDataByID(const ObjectDataID &a)
{
    return 0;
}

int QuickDataSlotDescList::GetDataSourceType() const
{
    return 1;
}

int QuickDataSlotDescList::LoadOnlyNameAndIDFromIndex(u32 a, int b)
{
    return 0;
}

void QuickDataSlotDescList::AddRef()
{
    field_0x5c++;
}
