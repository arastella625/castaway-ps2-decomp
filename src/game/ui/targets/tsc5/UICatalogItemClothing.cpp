#include "game/ui/targets/tsc5/UICatalogItemClothing.h"

bool UISimulatorSupport::UICatalogItemClothing::IsEatFromInventory() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemClothing::IsPutInHand() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemClothing::IsCultivatable() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemClothing::IsClothing() const
{
    return true;
}

bool UISimulatorSupport::UICatalogItemClothing::IsPlaceable() const
{
    return false;
}

u32 UISimulatorSupport::UICatalogItemClothing::GetClothingItem()
{
    return field_0x8;
}

int UISimulatorSupport::UICatalogItemClothing::GetPrice() const
{
    return 0;
}

int UISimulatorSupport::UICatalogItemClothing::GetUnlockLevel() const
{
    return 0;
}

int UISimulatorSupport::UICatalogItemClothing::GetMotives(int *a, int *b) const
{
    return 0;
}

bool UISimulatorSupport::UICatalogItemClothing::CanAddToInventory(InventoryItems *a) const
{
    return true;
}

bool UISimulatorSupport::UICatalogItemClothing::IsRecentlyUnlocked() const
{
    return false;
}

void UISimulatorSupport::UICatalogItemClothing::SetRecentlyUnlockedStatus(bool a)
{
}
