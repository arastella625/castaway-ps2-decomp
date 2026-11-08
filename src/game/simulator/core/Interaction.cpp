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

// A name nobody set is built from the tree table entry, with "*" in front of a debug-only one.
// GetEntry is written out here: the game inlines it.
const BString2 &Interaction::GetName() const
{
    static const u16 debugMark[] = {'*', 0};
    Interaction *self = (Interaction *)this;
    if (mName.length() == 0) {
        TreeTableEntry *entry = 0;
        if (mStackObject)
            entry = (TreeTableEntry *)mStackObject->mSelector->GetTreeTab()->GetEntry(mTreeTabIndex);
        if (entry) {
            if (entry->GetDebugOnly())
                self->mName = debugMark;
            self->mName += entry->GetName();
        }
    }
    self->mFlags &= ~0x80;
    return mName;
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
