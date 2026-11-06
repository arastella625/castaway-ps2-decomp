#include "game/cas/CasSimDescription.h"

#include "common/EResourceManager.h"
#include "common/ReconBuffer.h"
#include "game/cas/CasUtils.h"
#include "game/cas/LatticeParam.h"
#include "game/simulator/core/ClothingItem.h"
#include "game/simulator/core/Family.h"
#include "game/simulator/core/InventoryItems.h"
#include "game/simulator/core/Neighborhood.h"

Character::eSpecies CasSimDescription::GetSpecies() const
{
    return (Character::eSpecies)(flags[0] == false);
}

int CasSimDescription::GetSimType() const
{
    return 0;
}

// Resets the sim. `a` becomes flag 0. With `b`, only the clothes from part 0x1d on are
// reset and the rest of the sim is kept. `c` also gives the sim the default names.
void CasSimDescription::ClearSim(bool a, bool b, bool c)
{
    bool oldFlag = GetFlag(0);
    if (c) {
        if (!b) {
            SetFlag(1, false);
            SetFlag(0, a);
            field_0x80 = -1;
            ewcsncpy(firstName, _globals.GetNewUIString("default_player1_first_name", 0), 32);
            firstName[31] = 0;
            ewcsncpy(lastName, _globals.GetNewUIString("default_player1_last_name", 0), 32);
            lastName[31] = 0;
        }
    }
    if (textureInfo == 0 || oldFlag != GetFlag(0))
        InitQuickdataPointers();
    magic = -275;
    version = 7;
    for (u8 part = b ? 0x1d : 0x1a; part <= 0x28; part++)
        bodyParts[part - 0x1a]->SetToDefault(a);
    if (!b) {
        BodyType() = 0;
        SkinColor() = 0;
        skinTextureOffset = 0;
        field_0xfe = typeInfo->field_0x2c;
        int column = a == false;
        for (int i = 0; i < Sim::TATTOO_LOCATION_COUNT; i++)
            tattooIndices[i] = gDefaultTattooIndices[i][column];
        Character::eSpecies species = GetSpecies();
        LatticeParam *lattice = LatticeParam::Instance();
        for (int i = 0; i < Sim::LATTICE_MORPH_REGION_COUNT; i++)
            LatticeMorphValue((Sim::eLatticeMorphRegion)i)
                = lattice->GetMorphValNormal(species, (Sim::eLatticeMorphRegion)i);
        MorphTargetParam *morph = MorphTargetParam::Instance();
        for (int i = 0; i < Sim::MORPH_TARGET_REGION_COUNT; i++)
            MorphTargetValue((Sim::eMorphTargetRegion)i)
                = morph->GetMorphValNormal(species, (Sim::eMorphTargetRegion)i);
        for (u32 i = 0; i < PERSONALITY_COUNT; i++)
            personalityValues[i] = zodiacPersonalityValues[0][i];
        archetype = 0;
    }
}

// Reads or writes a saved sim. Streams older than version 4 have a different layout and
// go to DoStreamTSC5. The second parameter is not used; the version is the one in the stream.
void CasSimDescription::DoStream(ReconBuffer *buffer, int unused, bool a)
{
    if (a)
        buffer->Recon16(&magic, 1);
    else
        magic = -275;
    buffer->Recon8(&version, 1);
    if (version < 7) {
        if (version < 4) {
            DoStreamTSC5(buffer);
            return;
        }
    }
    buffer->Recon16((s16 *)firstName, 32);
    buffer->Recon16((s16 *)lastName, 32);
    buffer->Recon32((int *)&flags, 1);
    if (buffer->IsReading())
        InitQuickdataPointers();
    if (version >= 5)
        buffer->Recon32(&field_0x80, 1);
    for (int i = 0; i < Character::BODY_PART_COUNT; i++)
        bodyParts[i]->DoStream(buffer, version);
    buffer->ReconFloat(latticeMorphValues, 4);
    buffer->ReconFloat(morphTargetValues, 4);
    buffer->Recon8((s8 *)tattooIndices, 4);
    buffer->Recon8((s8 *)personalityValues, 5);
    buffer->Recon8((s8 *)&archetype, 1);
    buffer->Recon8((s8 *)&bodyType, 1);
    buffer->Recon8((s8 *)&skinColor, 1);
    buffer->Recon8((s8 *)&skinTextureOffset, 1);
    if (version >= 6)
        buffer->Recon16((s16 *)&field_0xfe, 1);
    if (buffer->IsReading())
        version = 7;
}

void CasSimDescription::CopyBodyPart(Character::eBodyPart part, const CasSimDescription &other)
{
    u8 index = GetBodyPartIndex(part);
    SimBodyPart *source = other.GetBodyPartByIndex(index);
    if (source != 0) {
        bodyParts[index]->CopyFrom(source);
    } else {
        bodyParts[index]->SetClothingSetIndex(other.GetClothingSetIndex(part));
        bodyParts[index]->SwitchToFirstAvailableDegradationStage();
    }
}

u8 CasSimDescription::GetClothingSetIndex(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)]->clothingSetIndex;
}

void CasSimDescription::SetClothingSetIndex(Character::eBodyPart part, u8 index)
{
    bodyParts[GetBodyPartIndex(part)]->SetClothingSetIndex(index);
}

u8 CasSimDescription::GetTextureIndex(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)]->textureIndex;
}

void CasSimDescription::SetTextureIndex(Character::eBodyPart part, u8 index)
{
    bodyParts[GetBodyPartIndex(part)]->textureIndex = index;
}

u8 &CasSimDescription::TextureIndex(Character::eBodyPart part)
{
    return bodyParts[GetBodyPartIndex(part)]->textureIndex;
}

u8 CasSimDescription::GetStyleIndex(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)]->styleIndex;
}

void CasSimDescription::SetStyleIndex(Character::eBodyPart part, u8 index)
{
    bodyParts[GetBodyPartIndex(part)]->styleIndex = index;
}

u8 &CasSimDescription::StyleIndex(Character::eBodyPart part)
{
    return bodyParts[GetBodyPartIndex(part)]->styleIndex;
}

u8 CasSimDescription::GetModelCodeIndex(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)]->modelCodeIndex;
}

void CasSimDescription::SetModelCodeIndex(Character::eBodyPart part, u8 index)
{
    bodyParts[GetBodyPartIndex(part)]->modelCodeIndex = index;
}

u8 &CasSimDescription::ModelCodeIndex(Character::eBodyPart part)
{
    return bodyParts[GetBodyPartIndex(part)]->modelCodeIndex;
}

u8 CasSimDescription::GetTattooIndex(Sim::eTattooLocation location) const
{
    return tattooIndices[location];
}

u8 &CasSimDescription::TattooIndex(Sim::eTattooLocation location)
{
    return tattooIndices[location];
}

f32 CasSimDescription::GetLatticeMorphValue(Sim::eLatticeMorphRegion region) const
{
    return latticeMorphValues[region];
}

f32 &CasSimDescription::LatticeMorphValue(Sim::eLatticeMorphRegion region)
{
    return latticeMorphValues[region];
}

f32 CasSimDescription::GetMorphTargetValue(Sim::eMorphTargetRegion region) const
{
    return morphTargetValues[region];
}

void CasSimDescription::SetMorphTargetValue(Sim::eMorphTargetRegion region, f32 value)
{
    morphTargetValues[region] = value;
}

f32 &CasSimDescription::MorphTargetValue(Sim::eMorphTargetRegion region)
{
    return morphTargetValues[region];
}

f32 CasSimDescription::GetMorphTargetWeightScaleFactor()
{
    return 1.0f;
}

u8 CasSimDescription::GetPersonalityValue(PersonalityEnum personality) const
{
    return personalityValues[personality];
}

u8 &CasSimDescription::PersonalityValue(PersonalityEnum personality)
{
    return personalityValues[personality];
}

u8 *CasSimDescription::GetPersonalityArray()
{
    return personalityValues;
}

u8 CasSimDescription::GetArchetype() const
{
    return archetype;
}

u8 &CasSimDescription::Archetype()
{
    return archetype;
}

u8 CasSimDescription::GetBodyType() const
{
    return bodyType;
}

u8 &CasSimDescription::BodyType()
{
    return bodyType;
}

u8 CasSimDescription::GetSkinColor() const
{
    return skinColor;
}

u8 &CasSimDescription::SkinColor()
{
    return skinColor;
}

const char *CasSimDescription::GetTypeCode() const
{
    if (flags[0])
        return typeInfo->typeCodes[0];
    return typeInfo->typeCodes[1];
}

SimBodyPart *CasSimDescription::GetBodyPart(Character::eBodyPart part) const
{
    return bodyParts[GetBodyPartIndex(part)];
}

SimBodyPart *CasSimDescription::GetBodyPartByIndex(u8 index) const
{
    if (index < Character::BODY_PART_COUNT)
        return bodyParts[index];
    return 0;
}

u32 CasSimDescription::GetSkinTextureID(Sim::eSimPartsMapLocation location, bool a) const
{
    if (skinColor >= textureInfo->skins.size())
        return 0;
    u8 index = skinTextureOffset + typeInfo->skinTexturesPerLocation * location;
    Sim::BodyPartSkinTSC6 *skin = &textureInfo->skins[skinColor];
    if (skin == 0 || index >= skin->textureIDs.size())
        return 0;
    return CasUtils::GetGameStateTextureID(skin->textureIDs[index], a);
}

EString CasSimDescription::GetSkinTextureName(Sim::eSimPartsMapLocation location, bool a) const
{
    EString name;
    if (skinColor >= textureInfo->skins.size())
        return 0;
    u8 index = skinTextureOffset + typeInfo->skinTexturesPerLocation * location;
    Sim::BodyPartSkinTSC6 *skin = &textureInfo->skins[skinColor];
    if (skin == 0 || index >= skin->textureIDs.size())
        return 0;
    name.Format("%s_%u%s", skin->name, skinTextureOffset, a ? "_GAME" : "");
    return name;
}

u32 CasSimDescription::GetTattooTextureID(Sim::eTattooLocation location, bool a) const
{
    if (tattooIndices[location] >= (u8)textureInfo->tattooTextures[(u8)location].size())
        return 0;
    return CasUtils::GetGameStateTextureID(
        textureInfo->tattooTextures[(u8)location][tattooIndices[location]].textureID, a);
}

EString CasSimDescription::GetTattooClothingSetName(Sim::eTattooLocation location) const
{
    EString name;
    if (tattooIndices[location] < (u8)textureInfo->tattooTextures[(u8)location].size())
        name.Format("%s", textureInfo->tattooTextures[(u8)location][tattooIndices[location]].name);
    return name;
}

EString CasSimDescription::GetCompositeTextureBaseName() const
{
    EString name;
    name.Format("%S_%S", firstName, lastName);
    name.MakeLegalSymbolName();
    return name;
}

EString CasSimDescription::GetCompositeTextureName(const char *base, Sim::eSimPartsMapLocation location) const
{
    EString name;
    switch (location) {
    case 1:
        name.Format("%s_cur_%s", GetTypeCode(), base);
        break;
    case 2:
        name.Format("%s_cll_%s", GetTypeCode(), base);
        break;
    case 3:
        name.Format("%s_clr_%s", GetTypeCode(), base);
        break;
    case 0:
    default:
        name.Format("%s_cul_%s", GetTypeCode(), base);
        break;
    }
    return name;
}

u32 CasSimDescription::GetCompositeTextureID(const char *base, Sim::eSimPartsMapLocation location) const
{
    return EResourceManager::CalcId(GetCompositeTextureName(base, location).c_str(), true);
}

void CasSimDescription::FillInTextureBlendInfo(Character::eBodyPart part, bool a, TextureBlendInfo *info) const
{
    u8 index = GetBodyPartIndex(part);
    info->part = part;
    info->rect = bodyParts[index]->GetLayoutMapRect();
    info->layoutMapNumber = bodyParts[index]->GetLayoutMapNumber();
    info->textureID = bodyParts[index]->GetTextureID(a);
    info->reflectionMaskID = bodyParts[index]->GetReflectionMaskID(a);
    info->layered = bodyParts[index]->IsLayered();
}

// Lists the clothing textures that go into one of the four composite maps, and returns
// how many there are.
int CasSimDescription::GetClothingTextureInfo(Sim::eSimPartsMapLocation location, TextureBlendInfo *info,
                                              bool a) const
{
    int count = 0;
    switch (location) {
    case 0:
        FillInTextureBlendInfo((Character::eBodyPart)0x27, a, &info[count++]);
        FillInTextureBlendInfo((Character::eBodyPart)0x20, a, &info[count++]);
        break;
    case 1:
        FillInTextureBlendInfo((Character::eBodyPart)0x1e, a, &info[count++]);
        break;
    case 2:
        FillInTextureBlendInfo((Character::eBodyPart)0x1c, a, &info[count++]);
        FillInTextureBlendInfo((Character::eBodyPart)0x21, a, &info[count++]);
        FillInTextureBlendInfo((Character::eBodyPart)0x25, a, &info[count++]);
        FillInTextureBlendInfo((Character::eBodyPart)0x1b, a, &info[count++]);
        FillInTextureBlendInfo((Character::eBodyPart)0x1d, a, &info[count++]);
        FillInTextureBlendInfo((Character::eBodyPart)0x22, a, &info[count++]);
        FillInTextureBlendInfo((Character::eBodyPart)0x23, a, &info[count++]);
        FillInTextureBlendInfo((Character::eBodyPart)0x24, a, &info[count++]);
        break;
    case 3:
        FillInTextureBlendInfo((Character::eBodyPart)0x1f, a, &info[count++]);
        break;
    }
    return count;
}

#ifdef NON_MATCHING
// The clear() is a short loop, and short loops don't match yet for two reasons.
// 1. The assembler pads a short loop with nops, a workaround for a fault in the PS2's
//    processor. The game's assembler pads until the branch is 4 instructions after the top
//    of the loop; every assembler build we have pads to 5. So this is one nop too long.
// 2. The game closes the loop with a "branch likely" (bnel) and keeps the index in a1;
//    this compiles to a plain bne and a2. The source shape of clear() is still a guess.
bool CasSimDescription::GetValidClothingSetIndexes(
    Character::eBodyPart part, eastl::vector<Sim::ClothingSetIndexInfo, eastl::allocator> &indexes)
{
    indexes.clear();
    u8 index = CasUtils::GetBodyPartDBIndex(part);
    if (index == 0x7f)
        return false;
    return bodyParts[index]->GetValidClothingSetIndexes(indexes);
}
#endif

void CasSimDescription::SwitchToFirstAvailableStyle(Character::eBodyPart part)
{
    bodyParts[GetBodyPartIndex(part)]->SwitchToFirstAvailableStyle();
}

void CasSimDescription::AdjustToLegalStyle(Character::eBodyPart part)
{
    bodyParts[GetBodyPartIndex(part)]->AdjustToLegalStyle();
}

// Sets lattice regions 1 to 3 from the body type. Region 0 is left alone.
void CasSimDescription::SetBodyTypeMorphValues()
{
    if (GetBodyType() >= Sim::BODY_TYPE_COUNT) {
        AssertWithInfo("Invalid body type");
        return;
    }
    LatticeMorphValue((Sim::eLatticeMorphRegion)1) = LatticeParam::Instance()->GetMorphValForBodyType(
        GetSpecies(), (Sim::eLatticeMorphRegion)1, (Sim::eBodyType)bodyType);
    LatticeMorphValue((Sim::eLatticeMorphRegion)2) = LatticeParam::Instance()->GetMorphValForBodyType(
        GetSpecies(), (Sim::eLatticeMorphRegion)2, (Sim::eBodyType)bodyType);
    LatticeMorphValue((Sim::eLatticeMorphRegion)3) = LatticeParam::Instance()->GetMorphValForBodyType(
        GetSpecies(), (Sim::eLatticeMorphRegion)3, (Sim::eBodyType)bodyType);
}

void CasSimDescription::InitializeToDefaultCostume(bool a)
{
    ClearSim(a, false, true);
    for (u8 part = 0x1a; part <= 0x28; part++)
        bodyParts[(u8)(part - 0x1a)]->SetToDefault(a);
}

void CasSimDescription::RemoveClothesForTattoo()
{
    for (u8 part = 0x1d; part < 0x28; part++)
        bodyParts[(u8)(part - 0x1a)]->SetToNude(GetFlag(0));
}

// A clothing set with flag bit 7 takes its texture variations from dyes, except when the
// requirements cheat is on or the sim is a ready-made archetype.
bool CasSimDescription::UseDyesForTextureVariation(Character::eBodyPart part)
{
    if (PlanManager::cheatIgnoreRequirements)
        return false;
    if (!(bodyParts[GetBodyPartIndex(part)]->flags.bits & 0x80))
        return false;
    return !CasUtils::UseArchetype();
}

// Puts on every clothing set the named costume lists. False when there is no such costume.
bool CasSimDescription::ChangeToCostume(const char *name)
{
    const Sim::CostumesTSC6 *costume = CasUtils::GetCostumeData(name);
    if (costume == 0)
        return false;
    const VECTOR<char *> *names = GetFlag(0) ? &costume->flagSetNames : &costume->flagClearNames;
    for (u8 part = 0x1a; part <= 0x26; part++) {
        u8 index = CasUtils::GetBodyPartIndex((Character::eBodyPart)part);
        if (strlen((*names)[index]) != 0) {
            u8 setIndex = CasUtils::GetClothingSetIndexByName(GetFlag(0), (Character::eBodyPart)part,
                                                              (*names)[index]);
            bodyParts[index]->SetClothingSetIndex(setIndex);
            bodyParts[index]->SwitchToFirstAvailableDegradationStage();
        }
    }
    Verify();
    return true;
}

const char *CasSimDescription::GetBodyPartName(Character::eBodyPart part) const
{
    return typeInfo->bodyPartTypes[GetBodyPartIndex(part)].name;
}

u8 CasSimDescription::GetNumBodyParts() const
{
    return Character::BODY_PART_COUNT;
}

u8 CasSimDescription::GetBodyPartIndex(Character::eBodyPart part) const
{
    return CasUtils::GetBodyPartIndex(part);
}

u8 CasSimDescription::GetClothingSetUIIndex(Character::eBodyPart part) const
{
    u8 index = CasUtils::GetBodyPartDBIndex(part);
    if (index == 0x7f)
        return 0;
    return bodyParts[index]->GetClothingSetUIIndex();
}

// Makes an inventory item for each piece of clothing the sim wears, worn down by `count`
// steps first. ClothingItem derives from InventoryItem, which isn't declared yet, hence
// the cast.
void CasSimDescription::CreateWornClothingItemsInInventory(InventoryItems *items, s16 neighborID, int count)
{
    if (items != 0) {
        bool male = GetFlag(0);
        for (u8 part = 0x1d; part <= 0x26; part++) {
            SimBodyPart *bodyPart = bodyParts[GetBodyPartIndex((Character::eBodyPart)part)];
            if (bodyPart->GetClothingSetIndex() != bodyPart->GetNudeClothingSetIndex(male)) {
                s16 id = ClothingItem::FindClothingItemID(
                    CasUtils::GetBodyPartName((Character::eBodyPart)part), bodyPart->GetClothingSetData()->name);
                if (id > 0) {
                    for (int i = 0; i < count; i++)
                        bodyPart->Degrade();
                    ClothingItem *item = new ClothingItem(id, male);
                    items->AddItem((InventoryItem *)item);
                    bodyPart->SetClothingItem(item);
                    item->SetNeighborID(neighborID);
                    item->CopyFromSimBodyPart(bodyPart);
                }
            }
        }
    }
}

// Ties each piece of clothing the sim wears to the matching item in the family's
// inventory, so that damage to one shows on the other.
void CasSimDescription::SetAllClothingItemsFromInventory(s16 neighborID)
{
    Neighbor *neighbor = GetNeighborhood()->FindNeighborByID(neighborID);
    if (neighbor != 0) {
        Family *family = neighbor->GetFamily();
        if (family != 0) {
            InventoryItems *items = family->GetInventory();
            if (items != 0) {
                bool male = GetFlag(0);
                for (u8 part = 0x1d; part <= 0x26; part++) {
                    SimBodyPart *bodyPart = bodyParts[GetBodyPartIndex((Character::eBodyPart)part)];
                    if (bodyPart->GetClothingSetIndex() != bodyPart->GetNudeClothingSetIndex(male)) {
                        ClothingItem *item = items->GetClothingItem(neighborID, (Character::eBodyPart)part);
                        if (item != 0 && item->IsMale() == male && part == (u8)item->GetBodyPart()
                            && item->GetClothingSetIndex() == bodyPart->GetClothingSetIndex()) {
                            item->CopyFromSimBodyPart(bodyPart);
                            bodyPart->SetClothingItem(item);
                        }
                    }
                }
            }
        }
    }
}
