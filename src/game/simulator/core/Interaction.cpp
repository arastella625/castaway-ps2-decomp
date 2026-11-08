#include "game/simulator/core/Interaction.h"

#include "game/simulator/core/ObjSelector.h"
#include "game/simulator/core/cXObject.h"

u32 Interaction::GetPerson() const
{
    return field_0x8;
}

// Wii: GetEntry, through cXObject::GetTreeTab.
TreeTableEntry *Interaction::GetEntry() const
{
    if (mStackObject == 0)
        return 0;
    return (TreeTableEntry *)mStackObject->mSelector->GetTreeTab()->GetEntry(mTreeTabIndex);
}

void Interaction::SetUniqueID()
{
    field_0x30 = ++sLastUniqueID;
}

// Bit 7 of the flags: the name was set, not built from the tree table.
void Interaction::SetName(const BString2 &name)
{
    mName = name;
    mFlags |= 0x80;
}

cXObject *Interaction::GetIconObject() const
{
    return mIconObject;
}
