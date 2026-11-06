#include "game/cas/SimBodyPartHat.h"

#include "game/cas/CasSimDescription.h"

int SimBodyPartHat::GetType() const
{
    return 0x21;
}

int SimBodyPartHat::GetBodyPartIndex() const
{
    return 7;
}

int SimBodyPartHat::GetBodyPartDBIndex() const
{
    return 7;
}

int SimBodyPartHat::GetTattooLocation() const
{
    return 4;
}

bool SimBodyPartHat::UseStyleCodeInTextureName() const
{
    return false;
}

int SimBodyPartHat::GetLayoutMapNumber() const
{
    return 2;
}

TRect<u16> SimBodyPartHat::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0x40, 0x40, 0x80);
}

int SimBodyPartHat::GetImageModelSetType() const
{
    return 9;
}

u8 SimBodyPartHat::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return styleIndex;
}

u8 SimBodyPartHat::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex;
}

void SimBodyPartHat::UpdateRelatedData()
{
    if (GetModelID() != 0)
        SetCharacterFlag(2, true);
    else
        SetCharacterFlag(2, false);
}

s32 SimBodyPartHat::GetIsDamaged()
{
    return 0;
}

BitFlags<u32> SimBodyPartHat::ApplyDamage(const Weather::DamageEvent &event, s16 a)
{
    return BitFlags<u32>(0);
}

// Character flag 2 is kHairBottomDisplayed, going by these messages. It has to agree with
// whether the hat has a model.
void SimBodyPartHat::Validate()
{
    if (IsValid(true, false) && description != 0) {
        if (GetModelID() != 0) {
            if (!GetCharacterFlag(2)) {
                char buffer[512];
                Sprintf(buffer, "Hat is being worn, but kHairBottomDisplayed is not set correctly");
                AssertWithInfo(buffer);
                SetCharacterFlag(2, true);
            }
        } else if (GetCharacterFlag(2)) {
            char buffer[512];
            Sprintf(buffer, "Hat is not being worn, but kHairBottomDisplayed is set");
            AssertWithInfo(buffer);
            SetCharacterFlag(2, false);
        }
    }
}
