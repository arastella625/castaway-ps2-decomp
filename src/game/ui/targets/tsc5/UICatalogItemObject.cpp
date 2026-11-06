#include "game/ui/targets/tsc5/UICatalogItemObject.h"

u32 UISimulatorSupport::UICatalogItemObject::GetSelector()
{
    return field_0x8;
}

bool UISimulatorSupport::UICatalogItemObject::IsEatFromInventory() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemObject::IsCultivatable() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemObject::IsClothing() const
{
    return false;
}

void UISimulatorSupport::UICatalogItemObject::AddToInventory(InventoryItems *a, int b) const
{
}

bool UISimulatorSupport::UICatalogItemObject::IsRecentlyUnlocked() const
{
    return false;
}

void UISimulatorSupport::UICatalogItemObject::SetRecentlyUnlockedStatus(bool a)
{
}
