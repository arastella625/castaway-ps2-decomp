#ifndef GAME_CAS_CASSIMDESCRIPTION_H
#define GAME_CAS_CASSIMDESCRIPTION_H

#include "common/BitFlags.h"
#include "common/types.h"
#include "game/cas/SimBodyPart.h"

class InventoryItems;

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

// One texture to blend into a composite map. FillInTextureBlendInfo writes every field.
struct TextureBlendInfo {
    u32 textureID;            // 0x00
    u32 reflectionMaskID;     // 0x04
    TRect<u16> rect;          // 0x08  where the texture goes in the map
    Character::eBodyPart part; // 0x10
    int layoutMapNumber;      // 0x14
    int layered;              // 0x18  SimBodyPart::IsLayered, stored as a word
};
STATIC_ASSERT(TextureBlendInfo_size, sizeof(TextureBlendInfo) == 0x1c);

class CasEventChangeCharacter {
public:
    enum CharacterPart { CHARACTER_PART_0 };
};

namespace Sim {

// Wii names, from the VECTOR instantiations CasSimDescription's texture functions use.
struct BodyPartTextureTSC6 {
    const char *name; // 0x00  the tattoo's clothing set name
    u32 textureID;    // 0x04
    u32 field_0x8; // 0x08
    u32 field_0xc; // 0x0c
};
STATIC_ASSERT(BodyPartTextureTSC6_size, sizeof(BodyPartTextureTSC6) == 0x10);

struct BodyPartSkinTSC6 {
    const char *name;       // 0x00
    VECTOR<u32> textureIDs; // 0x04
    u32 field_0x8;          // 0x08
};
STATIC_ASSERT(BodyPartSkinTSC6_size, sizeof(BodyPartSkinTSC6) == 0xc);

}  // namespace Sim

// No known Wii name; this name is ours.
struct CasSimTypeInfo {
    VECTOR<BodyPartTypeInfo> bodyPartTypes; // 0x00  one per body part index
    const char **typeCodes;                 // 0x04  [0] when flag 0 is set, else [1]
    u8 field_0x8[0x24];                     // 0x08
    s16 skinHitPointsPerState;              // 0x2c  what the skin starts with after each degrade
    f32 field_0x30;                         // 0x30  clothing hit points above this shield the skin
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
    s32 field_0x80;                                   // 0x080  ClearSim sets -1
    s16 magic;                                        // 0x084  -275 (0xfeed), set by DoStream
    s8 version;                                       // 0x086  stream version; 7 is current
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
    s16 skinHitPoints;                                // 0x0fe
    // The 15 SimBodyPart subclass objects, head first. pkg/cas_layouts.csv lists them;
    // they become real members once the subclasses are declared.
    u8 bodyPartStorage[0x230];                        // 0x100
    // vtable pointer                                 // 0x330

    virtual ~CasSimDescription();
    virtual Character::eSpecies GetSpecies() const;
    virtual int GetSimType() const;
    virtual void DoStream(ReconBuffer *buffer, int unused, bool a);
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
    virtual const char *GetTypeCode() const;
    virtual SimBodyPart *GetBodyPart(Character::eBodyPart part) const;
    virtual SimBodyPart *GetBodyPartByIndex(u8 index) const;
    virtual u32 GetSkinTextureID(Sim::eSimPartsMapLocation location, bool a) const;
    virtual EString GetSkinTextureName(Sim::eSimPartsMapLocation location, bool a) const;
    virtual EString GetTattooClothingSetName(Sim::eTattooLocation location) const;
    virtual u32 GetTattooTextureID(Sim::eTattooLocation location, bool a) const;
    virtual void GetTattooTextureInfo(Sim::eSimPartsMapLocation location, TextureBlendInfo *info, bool a) const;
    virtual int GetClothingTextureInfo(Sim::eSimPartsMapLocation location, TextureBlendInfo *info, bool a) const;
    virtual EString GetCompositeTextureBaseName() const;
    virtual EString GetCompositeTextureName(const char *base, Sim::eSimPartsMapLocation location) const;
    virtual u32 GetCompositeTextureID(const char *base, Sim::eSimPartsMapLocation location) const;
    virtual bool GetValidClothingSetIndexes(
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
    virtual bool UseDyesForTextureVariation(Character::eBodyPart part);
    virtual bool ChangeToCostume(const char *name);
    virtual BitFlags<u32> ApplyDamage(s16 neighborID, const Weather::DamageEvent &event, s16 scale);
    virtual const char *GetBodyPartName(Character::eBodyPart part) const;
    virtual u8 GetNumBodyParts() const;
    virtual u8 GetBodyPartIndex(Character::eBodyPart part) const;
    virtual void CreateWornClothingItemsInInventory(InventoryItems *items, s16 neighborID, int count);
    virtual void SetAllClothingItemsFromInventory(s16 neighborID);
    virtual u8 GetClothingSetUIIndex(Character::eBodyPart part) const;

    // Not virtual. The PS2 build inlines GetFlag.
    bool GetFlag(int index) const { return flags[index]; }
    void SetFlag(int index, bool value) { flags.setFlag(index, value); }
    void CopyBodyPart(Character::eBodyPart part, const CasSimDescription &other);
    // Moves the skin to its next, more worn texture, if there is one, and reports that with
    // bit 1. The PS2 build inlines this; the Wii build keeps it as a function.
    BitFlags<u32> DegradeSkin()
    {
        BitFlags<u32> result(0);
        if (skinTextureOffset + 1 < typeInfo->skinTexturesPerLocation) {
            skinTextureOffset++;
            result.setFlag(1, true);
        }
        return result;
    }
    // The PS2 build inlines these three; the Wii build keeps them as functions.
    CasSimTextureInfo *GetBodyData() const { return textureInfo; }
    CasSimTypeInfo *GetBodyPartTypeInfoTable() const { return typeInfo; }
    void InitBodyPartData()
    {
        for (int i = 0; i < Character::BODY_PART_COUNT; i++)
            bodyParts[i]->Init(this);
    }
    void DoStreamTSC5(ReconBuffer *buffer);
    void FillInTextureBlendInfo(Character::eBodyPart part, bool a, TextureBlendInfo *info) const;
    void InitQuickdataPointers();

    // Builds "<info> <first name> <last name>" plus a line break for an assert. The release build throws
    // the text away, but the code that builds it is still inlined wherever a body part
    // reports a problem. The Wii build keeps this as a function.
    void AssertWithInfo(const char *info) const
    {
        char text[1024];
        strncpy(text, info, 1024);
        text[1023] = 0;
        char *p = text + strlen(info);
        *p++ = ' ';
        for (u32 i = 0; firstName[i] != 0 && i < 32; i++)
            *p++ = firstName[i];
        *p++ = ' ';
        for (u32 i = 0; lastName[i] != 0 && i < 32; i++)
            *p++ = lastName[i];
        *p++ = '\n';
        *p = 0;
    }
};
STATIC_ASSERT(CasSimDescription_size, sizeof(CasSimDescription) == 0x334);

// A body part asks its sim for a flag, and falls back to its own copy when it has no sim.
inline bool SimBodyPart::GetCharacterFlag(int index) const
{
    if (description != 0)
        return description->GetFlag(index);
    return characterFlags[index];
}

inline void SimBodyPart::AssertWithInfo(const char *info)
{
    if (description != 0)
        description->AssertWithInfo(info);
}

inline void SimBodyPart::SetCharacterFlag(int index, bool value)
{
    if (description != 0)
        description->SetFlag(index, value);
    else
        characterFlags.setFlag(index, value);
}

#endif
