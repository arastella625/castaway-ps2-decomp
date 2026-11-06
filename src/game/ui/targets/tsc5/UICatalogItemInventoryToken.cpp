#include "game/ui/targets/tsc5/UICatalogItemInventoryToken.h"

u32 UISimulatorSupport::UICatalogItemInventoryToken::GetInventoryToken()
{
    return field_0x8;
}

bool UISimulatorSupport::UICatalogItemInventoryToken::IsClothing() const
{
    return false;
}

int UISimulatorSupport::UICatalogItemInventoryToken::GetPrice() const
{
    return 0;
}

int UISimulatorSupport::UICatalogItemInventoryToken::GetUnlockLevel() const
{
    return 0;
}

bool UISimulatorSupport::UICatalogItemInventoryToken::CanAddToInventory(InventoryItems *a) const
{
    return true;
}

bool UISimulatorSupport::UICatalogItemInventoryToken::IsRecentlyUnlocked() const
{
    return false;
}

void UISimulatorSupport::UICatalogItemInventoryToken::SetRecentlyUnlockedStatus(bool a)
{
}
