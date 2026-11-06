#include "game/interactor/Interactor.h"

void InteractorModule::Interactor::ResetIdleTime()
{
    field_0x58 = 0;
}

void InteractorModule::Interactor::ExecuteObjectActionMenu(cXObject *a)
{
}

void InteractorModule::Interactor::StopInObjectMenu()
{
}

int InteractorModule::Interactor::ChooseAction(Interaction *a)
{
    return 0;
}

void InteractorModule::Interactor::AttachCamera(ESimsCam *value)
{
    field_0x4 = value;
}

ESimsCam *InteractorModule::Interactor::GetAttachedCamera()
{
    return field_0x4;
}
