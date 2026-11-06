#ifndef GAME_CAS_CASSIMDESCRIPTION_H
#define GAME_CAS_CASSIMDESCRIPTION_H

#include "common/BitFlags.h"
#include "common/types.h"
#include "game/cas/SimBodyPart.h"

class InventoryItems;
struct TextureBlendInfo;

// Enumerator values are filled in as code proves them.
enum PersonalityEnum { PERSONALITY_COUNT = 5 };

namespace Character {
enum eSpecies { SPECIES_0, SPECIES_1 };
enum eBodyPart { BODY_PART_FIRST = 0x1a, BODY_PART_COUNT = 15 };
}

namespace Sim {
enum eTattooLocation { TATTOO_LOCATION_COUNT = 4 };
enum eLatticeMorphRegion { LATTICE_MORPH_REGION_COUNT = 4 };
enum eMorphTargetRegion { MORPH_TARGET_REGION_COUNT = 4 };
enum eSimPartsMapLocation { SIM_PARTS_MAP_LOCATION_0 };
}

class CasEventChangeCharacter {
public:
    enum CharacterPart { CHARACTER_PART_0 };
};

namespace Sim {

// Wii names, from the VECTOR instantiations CasSimDescription's texture functions use.
struct BodyPartTextureTSC6 {
    u32 field_0x0; // 0x00
    u32 textureID; // 0x04
    u32 field_0x8; // 0x08
    u32 field_0xc; // 0x0c
};
STATIC_ASSERT(BodyPartTextureTSC6_size, sizeof(BodyPartTextureTSC6) == 0x10);

struct BodyPartSkinTSC6 {
    u32 field_0x0;          // 0x00
    VECTOR<u32> textureIDs; // 0x04
    u32 field_0x8;          // 0x08
};
STATIC_ASSERT(BodyPartSkinTSC6_size, sizeof(BodyPartSkinTSC6) == 0xc);

}  // namespace Sim

// No known Wii name; this name is ours.
struct CasSimTypeInfo {
    VECTOR<BodyPartTypeInfo> bodyPartTypes; // 0x00  one per body part index
    u32 *typeCodes;                         // 0x04  [0] when flag 0 is set, else [1]
    u8 field_0x8[0x2c];                     // 0x08
    u8 skinTexturesPerLocation;             // 0x34  multiplies the parts-map location in GetSkinTextureID
};

// No known Wii name; this name is ours.
struct CasSimTextureInfo {
    u8 field_0x0[0x34];                                                         // 0x00
    VECTOR<Sim::BodyPartTextureTSC6> tattooTextures[Sim::TATTOO_LOCATION_COUNT]; // 0x34
    VECTOR<Sim::BodyPartSkinTSC6> skins;                                        // 0x44  one per skin colour
};

// The virtual functions are in vtable order, which the Wii and PS2 builds share.
// Return types of functions that aren't decompiled yet are guesses.
class CasSimDescription {
public:
    u16 firstName[32];                                // 0x000
    u16 lastName[32];                                 // 0x040
    u32 field_0x80;                                   // 0x080
    u16 field_0x84;                                   // 0x084
    u8 field_0x86;                                    // 0x086
    BitFlags<u32> flags;                              // 0x088
    CasSimTextureInfo *textureInfo;                   // 0x08c
    CasSimTypeInfo *typeInfo;                         // 0x090
    SimBodyPart *bodyParts[Character::BODY_PART_COUNT]; // 0x094  point into bodyPartStorage
    f32 latticeMorphValues[Sim::LATTICE_MORPH_REGION_COUNT]; // 0x0d0
    f32 morphTargetValues[Sim::MORPH_TARGET_REGION_COUNT];   // 0x0e0
    u8 tattooIndices[Sim::TATTOO_LOCATION_COUNT];     // 0x0f0
    u8 personalityValues[PERSONALITY_COUNT];          // 0x0f4
    u8 archetype;                                     // 0x0f9
    u8 bodyType;                                      // 0x0fa
    u8 skinColor;                                     // 0x0fb
    u8 skinTextureOffset;                             // 0x0fc  added to the index in GetSkinTextureID
    u16 field_0xfe;                                   // 0x0fe
    // The 15 SimBodyPart subclass objects, head first. pkg/cas_layouts.csv lists them;
    // they become real members once the subclasses are declared.
    u8 bodyPartStorage[0x230];                        // 0x100
    // vtable pointer                                 // 0x330

    virtual ~CasSimDescription();
    virtual Character::eSpecies GetSpecies() const;
    virtual int GetSimType() const;
    virtual void DoStream(ReconBuffer *buffer, int version, bool a);
    virtual void DoStreamTSC5Pet(ReconBuffer *buffer, int version, bool a);
    virtual void Verify();
    virtual void Randomize(Character::eSpecies species, bool a, bool b);
    virtual void ClearSim(bool a, bool b, bool c);
    virtual void CopyFrom(const CasSimDescription &other);
    virtual u8 GetClothingSetIndex(Character::eBodyPart part) const;
    virtual void SetClothingSetIndex(Character::eBodyPart part, u8 index);
    virtual u8 GetTextureIndex(Character::eBodyPart part) const;
    virtual void SetTextureIndex(Character::eBodyPart part, u8 index);
    virtual u8 &TextureIndex(Character::eBodyPart part);
    virtual u8 GetStyleIndex(Character::eBodyPart part) const;
    virtual void SetStyleIndex(Character::eBodyPart part, u8 index);
    virtual u8 &StyleIndex(Character::eBodyPart part);
    virtual u8 GetModelCodeIndex(Character::eBodyPart part) const;
    virtual void SetModelCodeIndex(Character::eBodyPart part, u8 index);
    virtual u8 &ModelCodeIndex(Character::eBodyPart part);
    virtual u8 GetTattooIndex(Sim::eTattooLocation location) const;
    virtual u8 &TattooIndex(Sim::eTattooLocation location);
    virtual f32 GetLatticeMorphValue(Sim::eLatticeMorphRegion region) const;
    virtual f32 &LatticeMorphValue(Sim::eLatticeMorphRegion region);
    virtual f32 GetMorphTargetValue(Sim::eMorphTargetRegion region) const;
    virtual void SetMorphTargetValue(Sim::eMorphTargetRegion region, f32 value);
    virtual f32 &MorphTargetValue(Sim::eMorphTargetRegion region);
    virtual f32 GetMorphTargetWeightScaleFactor();
    virtual u8 GetPersonalityValue(PersonalityEnum personality) const;
    virtual u8 &PersonalityValue(PersonalityEnum personality);
    virtual u8 *GetPersonalityArray();
    virtual u8 GetArchetype() const;
    virtual u8 &Archetype();
    virtual u8 GetBodyType() const;
    virtual u8 &BodyType();
    virtual u8 GetSkinColor() const;
    virtual u8 &SkinColor();
    virtual u32 GetTypeCode() const;
    virtual SimBodyPart *GetBodyPart(Character::eBodyPart part) const;
    virtual SimBodyPart *GetBodyPartByIndex(u8 index) const;
    virtual u32 GetSkinTextureID(Sim::eSimPartsMapLocation location, bool a) const;
    virtual void GetSkinTextureName(Sim::eSimPartsMapLocation location, bool a) const;
    virtual void GetTattooClothingSetName(Sim::eTattooLocation location) const;
    virtual u32 GetTattooTextureID(Sim::eTattooLocation location, bool a) const;
    virtual void GetTattooTextureInfo(Sim::eSimPartsMapLocation location, TextureBlendInfo *info, bool a) const;
    virtual void GetClothingTextureInfo(Sim::eSimPartsMapLocation location, TextureBlendInfo *info, bool a) const;
    virtual EString GetCompositeTextureBaseName() const;
    virtual EString GetCompositeTextureName(const char *base, Sim::eSimPartsMapLocation location) const;
    virtual u32 GetCompositeTextureID(const char *base, Sim::eSimPartsMapLocation location) const;
    virtual void GetValidClothingSetIndexes(
        Character::eBodyPart part, eastl::vector<Sim::ClothingSetIndexInfo, eastl::allocator> &indexes);
    virtual void GetValidTattooIndexes(
        Sim::eTattooLocation location, eastl::vector<Sim::ClothingSetIndexInfo, eastl::allocator> &indexes);
    virtual void SwitchToFirstAvailableStyle(Character::eBodyPart part);
    virtual void AdjustToLegalStyle(Character::eBodyPart part);
    virtual void SetBodyTypeMorphValues();
    virtual void InitializeToDefaultCostume(bool a);
    virtual void RemoveClothesForTattoo();
    virtual void HandleEventChangeCharacter(const CasEventChangeCharacter &event, u8 &a, u8 &b);
    virtual bool ChangeEventTypeHasMoreThanOneOption(CasEventChangeCharacter::CharacterPart part);
    virtual void UseDyesForTextureVariation(Character::eBodyPart part);
    virtual void ChangeToCostume(const char *name);
    virtual void ApplyDamage(s16 a, const Weather::DamageEvents &events);
    virtual const char *GetBodyPartName(Character::eBodyPart part) const;
    virtual u8 GetNumBodyParts() const;
    virtual u8 GetBodyPartIndex(Character::eBodyPart part) const;
    virtual void CreateWornClothingItemsInInventory(InventoryItems *items, s16 a, int b);
    virtual void SetAllClothingItemsFromInventory(s16 a);
    virtual u8 GetClothingSetUIIndex(Character::eBodyPart part) const;

    // Not virtual. The PS2 build inlines GetFlag.
    bool GetFlag(int index) const { return flags[index]; }
    void SetFlag(int index, bool value) { flags.Set(index, value); }
    void CopyBodyPart(Character::eBodyPart part, const CasSimDescription &other);
};
STATIC_ASSERT(CasSimDescription_size, sizeof(CasSimDescription) == 0x334);

// A body part asks its sim for a flag, and falls back to its own copy when it has no sim.
inline bool SimBodyPart::GetCharacterFlag(int index) const
{
    if (description != 0)
        return description->GetFlag(index);
    return characterFlags[index];
}

inline void SimBodyPart::SetCharacterFlag(int index, bool value)
{
    if (description != 0)
        description->SetFlag(index, value);
    else
        characterFlags.Set(index, value);
}

#endif
