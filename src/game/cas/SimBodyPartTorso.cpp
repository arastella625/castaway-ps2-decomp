#include "game/cas/SimBodyPartTorso.h"

#include "game/cas/CasSimDescription.h"
#include "game/cas/SimBodyPartSleeves.h"

int SimBodyPartTorso::GetType() const
{
    return 0x1e;
}

int SimBodyPartTorso::GetBodyPartIndex() const
{
    return 4;
}

int SimBodyPartTorso::GetBodyPartDBIndex() const
{
    return 4;
}

int SimBodyPartTorso::GetTattooLocation() const
{
    return 1;
}

bool SimBodyPartTorso::IsLayered() const
{
    return true;
}

bool SimBodyPartTorso::IsRequiredBodyPart() const
{
    return true;
}

int SimBodyPartTorso::GetLayoutMapNumber() const
{
    return 1;
}

TRect<u16> SimBodyPartTorso::GetLayoutMapRect() const
{
    return TRect<u16>(0, 0, 0x80, 0x80);
}

int SimBodyPartTorso::GetImageModelSetType() const
{
    return 6;
}

u8 SimBodyPartTorso::CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const
{
    return styleIndex;
}

u8 SimBodyPartTorso::CalculateStyleIndexFromModelIndex(u8 modelIndex) const
{
    return modelIndex;
}

void SimBodyPartTorso::UpdateRelatedData()
{
    if (description != 0) {
        // 0x27 is the sleeves.
        SimBodyPart *sleeves = description->GetBodyPart((Character::eBodyPart)0x27);
        ((SimBodyPartSleeves *)sleeves)->SwitchToMatchTorsoSettings();
    }
}

BitFlags<u32> SimBodyPartTorso::Degrade()
{
    BitFlags<u32> result = SimBodyPart::Degrade();
    UpdateRelatedData();
    return result;
}

BitFlags<u32> SimBodyPartTorso::Repair()
{
    BitFlags<u32> result = SimBodyPart::Repair();
    UpdateRelatedData();
    return result;
}

void SimBodyPartTorso::Validate()
{
    if (IsValid(true, false)) {
        u8 count = GetNumPossibleStyles(0);
        if (modelIndex >= count) {
            char buffer[512];
            Sprintf(buffer, "Torso Model Index %d is invalid.  Must be in the range [0,%d]", modelIndex, count);
            AssertWithInfo(buffer);
            SwitchToFirstAvailableDegradationStage();
        }
    }
}
