#include "game/cas/SimBodyPartBelt.h"

#include "game/cas/CasSimDescription.h"

int SimBodyPartBelt::GetType() const
{
    return 0x24;
}

int SimBodyPartBelt::GetBodyPartIndex() const
{
    return 0xa;
}

int SimBodyPartBelt::GetBodyPartDBIndex() const
{
    return 0xa;
}

int SimBodyPartBelt::GetTattooLocation() const
{
    return 4;
}

int SimBodyPartBelt::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartBelt::GetLayoutMapRect() const
{
    return TRect<u16>(0x20, 0, 0x40, 0x20);
}

int SimBodyPartBelt::GetImageModelSetType() const
{
    return 0xc;
}

// A belt has no model while it is the nude set, or while the torso's style hides it.
u32 SimBodyPartBelt::GetModelID() const
{
    if (clothingSetIndex >= geometryVector->size()
        || clothingSetIndex == GetNudeClothingSetIndex(description->GetFlag(0)))
        return 0;
    if (modelIndex >= clothingSetData->modelIDs.size())
        return 0;
    SimBodyPart *torso = description->GetBodyPart((Character::eBodyPart)0x1e);
    if (torso->StyleIndex() == 0 && (torso->flags.bits & 0x1000))
        return 0;
    if (torso->StyleIndex() == 1 && (torso->flags.bits & 0x2000))
        return 0;
    return clothingSetData->modelIDs[modelIndex];
}

s32 SimBodyPartBelt::GetIsDamaged()
{
    return 0;
}

BitFlags<u32> SimBodyPartBelt::ApplyDamage(const Weather::DamageEvents &events)
{
    return BitFlags<u32>(0);
}
