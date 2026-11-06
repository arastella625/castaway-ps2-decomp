#include "game/ui/targets/tsc5/NumericalInputSpinner.h"

void UISimulatorSupport::NumericalInputSpinner::NotifyInputHappened()
{
    field_0x0++;
}

s32 UISimulatorSupport::NumericalInputSpinner::GetFramesPressed()
{
    return field_0x0;
}
