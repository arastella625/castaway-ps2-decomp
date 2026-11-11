#include "game/ui/core/CURTarget.h"

// ActionMenu::ShowMenu opens the menu here when the player isn't in direct control.
void CURTarget::GetPosition(int &x, int &y)
{
    x = mX;
    y = mY;
}

u32 CURTarget::GetCursorHighlight()
{
    return field_0xb0;
}
