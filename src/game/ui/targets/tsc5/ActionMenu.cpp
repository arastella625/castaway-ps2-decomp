#include "game/GameData.h"

// GenericSimCall 0x11. Only a count of 1 to 9,998 is kept; Update shows it with imuShowCount
// when it changes.
void ActionMenu::SetupHarvestCount(int count)
{
    if ((u32)(count - 1) < 9998)
        mHarvestCount = count;
}
