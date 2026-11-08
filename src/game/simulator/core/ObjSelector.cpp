#include "game/simulator/core/ObjSelector.h"

// Wii: ObjectFolder::GetTreeTable, which loads a selector's tree table.
class ObjectFolder {
public:
    void GetTreeTable(ObjSelector *selector);
};

// Wii: ObjSelector::GetTreeTable. The table is loaded the first time it's asked for.
TreeTab *ObjSelector::GetTreeTab()
{
    if (!mTreeTab)
        mFolder->GetTreeTable(this);
    return mTreeTab;
}
