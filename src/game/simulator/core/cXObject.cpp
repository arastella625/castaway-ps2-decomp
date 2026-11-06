#include "game/simulator/core/cXObject.h"

void cXObject::PreSave()
{
}

void cXObject::TagGraphicsField()
{
    field_0x134 = 1;
}

u32 cXObject::GetSpriteSlot()
{
    return field_0x110;
}

void cXObject::ForceLocation()
{
}

u32 cXObject::ReconType()
{
    return 0x584f424a;
}
