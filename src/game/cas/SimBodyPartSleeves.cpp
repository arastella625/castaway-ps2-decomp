#include "game/cas/SimBodyPartSleeves.h"

#include "game/cas/CasSimDescription.h"

int SimBodyPartSleeves::GetType() const
{
    return 0x27;
}

int SimBodyPartSleeves::GetBodyPartIndex() const
{
    return 0xd;
}

int SimBodyPartSleeves::GetBodyPartDBIndex() const
{
    return 4;
}

int SimBodyPartSleeves::GetTattooLocation() const
{
    return 0;
}

bool SimBodyPartSleeves::IsLayered() const
{
    return true;
}

bool SimBodyPartSleeves::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartSleeves::GetLayoutMapNumber() const
{
    return 0;
}

TRect<u16> SimBodyPartSleeves::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0x40, 0x80, 0x80);
}

int SimBodyPartSleeves::GetImageModelSetType() const
{
    return 6;
}

u8 SimBodyPartSleeves::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return field_0x24 + styleIndex;
}

u8 SimBodyPartSleeves::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return (modelIndex - field_0x24) % GetNumPossibleStyles(0);
}

void SimBodyPartSleeves::SwitchToFirstAvailableDegradationStage()
{
    SwitchToMatchTorsoSettings();
}

s32 SimBodyPartSleeves::GetIsDamaged()
{
    return 0;
}

BitFlags<u32> SimBodyPartSleeves::ApplyDamage(const Weather::DamageEvent &event, s16 a)
{
    return BitFlags<u32>(0);
}

BitFlags<u32> SimBodyPartSleeves::Degrade()
{
    return BitFlags<u32>(0);
}

BitFlags<u32> SimBodyPartSleeves::Repair()
{
    return BitFlags<u32>(0);
}

// Clothing set flag bit 6 marks a sleeveless torso. Sleeves otherwise copy the torso, and
// their model index has to fall in the block of models that starts at field_0x24.
void SimBodyPartSleeves::Validate()
{
    if (description != 0) {
        SimBodyPart *torso = description->GetBodyPart((Character::eBodyPart)0x1e);
        if (torso->flags.bits & 0x40) {
            if (clothingSetIndex != GetNudeClothingSetIndex(GetCharacterFlag(0))) {
                char buffer[512];
                Sprintf(buffer, "Torso is marked as sleeveless, but sleeves are not set to Nude Clothing "
                                "Index.  Changing Sleeves to Nude.");
                AssertWithInfo(buffer);
                SwitchToMatchTorsoSettings();
            }
        } else if (clothingSetIndex != torso->GetClothingSetIndex()
                   || modelCodeIndex != torso->ModelCodeIndex() || styleIndex != torso->StyleIndex()
                   || textureIndex != torso->TextureIndex()
                   || degradationState != torso->DegradationState()) {
            char buffer[512];
            Sprintf(buffer, "Sleeve settings do not match torso's settings.  Changing Sleeves to match.");
            AssertWithInfo(buffer);
            SwitchToMatchTorsoSettings();
        }
        u8 count = GetNumPossibleStyles(0);
        if (modelIndex < field_0x24 || modelIndex >= field_0x24 + count) {
            char buffer[512];
            Sprintf(buffer, "Sleeve Model Index %d is invalid.  Must be in the range [%d,%d].", modelIndex,
                    field_0x24, field_0x24 + count - 1);
            AssertWithInfo(buffer);
            SwitchToMatchTorsoSettings();
        }
        IsValid(true, false);
    }
}
