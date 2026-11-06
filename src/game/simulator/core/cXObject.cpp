#include "game/simulator/core/cXObject.h"

void cXObject::PreSave()
{
}

void cXObject::TagGraphicsField()
{
    mGraphicsTagged = 1;
}

u32 cXObject::GetSpriteSlot()
{
    return (u32)mSpriteSlots.mpBegin;
}

void cXObject::ForceLocation()
{
}

u32 cXObject::ReconType()
{
    return 0x584f424a;
}
