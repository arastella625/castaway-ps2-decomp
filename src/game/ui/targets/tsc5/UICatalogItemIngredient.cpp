#include "game/ui/targets/tsc5/UICatalogItemIngredient.h"

u32 UISimulatorSupport::UICatalogItemIngredient::GetIngredient()
{
    return field_0x8;
}

bool UISimulatorSupport::UICatalogItemIngredient::IsEatFromInventory() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemIngredient::IsPutInHand() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemIngredient::IsCultivatable() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemIngredient::IsClothing() const
{
    return false;
}

bool UISimulatorSupport::UICatalogItemIngredient::CanAddToInventory(InventoryItems *a) const
{
    return true;
}

bool UISimulatorSupport::UICatalogItemIngredient::IsRecentlyUnlocked() const
{
    return false;
}

void UISimulatorSupport::UICatalogItemIngredient::SetRecentlyUnlockedStatus(bool a)
{
}
