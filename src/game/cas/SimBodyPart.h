#ifndef GAME_CAS_SIMBODYPART_H
#define GAME_CAS_SIMBODYPART_H

#include "common/BitFlags.h"
#include "common/EString.h"
#include "common/EVec3.h"
#include "common/TRect.h"
#include "common/Vector.h"
#include "common/types.h"

class CasSimDescription;
class ReconBuffer;
class SimModelTSC6;

namespace eastl {
class allocator;
template <class T, class Allocator> class vector;
}

namespace Weather {
enum eDamageType { DAMAGE_TYPE_COUNT = 3 };
struct DamageEvents;
}

namespace Sim {

struct ClothingSetIndexInfo;

// One clothing set of a body part. SimBodyPart::geometryVector holds these.
struct BodyPartGeometryTSC6 {
    const char *name;                       // 0x00
    VECTOR<u32> modelIDs;                   // 0x04  one per model index; zero means no such model
    VECTOR<const char *> textureVariations; // 0x08
    u32 flags;                              // 0x0c  bits 1-5: generic for style 1-5
    s32 hitPointsPerDegradationState;       // 0x10
    VECTOR<u8> armorValues;                 // 0x14  one per Weather::eDamageType
};
STATIC_ASSERT(BodyPartGeometryTSC6_size, sizeof(BodyPartGeometryTSC6) == 0x18);

}  // namespace Sim

// The three structs below have no known Wii names; these names are ours.
struct BodyPartStyleInfo {
    const char *code;        // 0x00  returned by SimBodyPart::GetStyleCode
    u8 numDegradationStates; // 0x04
};
STATIC_ASSERT(BodyPartStyleInfo_size, sizeof(BodyPartStyleInfo) == 0x8);

struct BodyPartModelCodeInfo {
    const char *code;                 // 0x00
    VECTOR<BodyPartStyleInfo> styles; // 0x04
};
STATIC_ASSERT(BodyPartModelCodeInfo_size, sizeof(BodyPartModelCodeInfo) == 0x8);

struct BodyPartTypeInfo {
    const char *name;                          // 0x00
    u32 field_0x4;                             // 0x04
    VECTOR<BodyPartModelCodeInfo> modelCodes;  // 0x08
    u32 field_0xc;                             // 0x0c
    u32 field_0x10;                            // 0x10
    u8 *nudeClothingSetIndexes;                // 0x14  [0] and [1], picked by a bool
};
STATIC_ASSERT(BodyPartTypeInfo_size, sizeof(BodyPartTypeInfo) == 0x18);

struct BodyPartDamageState {
    u32 field_0x0;         // 0x00
    u32 field_0x4;         // 0x04
    s32 currentHitPoints;  // 0x08
    u32 field_0xc[4];      // 0x0c
    s32 isDamaged;         // 0x1c
};

// The virtual functions are in vtable order, which the Wii and PS2 builds share.
// Return types of functions that aren't decompiled yet are guesses.
class SimBodyPart {
public:
    CasSimDescription *description;                   // 0x00  the sim this part belongs to, or null
    BitFlags<u32> flags;                                 // 0x04  copy of the clothing set's flags
    VECTOR<Sim::BodyPartGeometryTSC6> *geometryVector; // 0x08
    BodyPartTypeInfo *bodyPartTypeInfo;               // 0x0c
    Sim::BodyPartGeometryTSC6 *clothingSetData;       // 0x10  &(*geometryVector)[clothingSetIndex]
    BodyPartDamageState *damageState;                 // 0x14
    u8 clothingSetIndex;                              // 0x18
    u8 modelCodeIndex;                                // 0x19
    u8 modelIndex;                                    // 0x1a
    u8 styleIndex;                                    // 0x1b
    u8 textureIndex;                                  // 0x1c
    u8 degradationState;                              // 0x1d
    BitFlags<u16> characterFlags;                     // 0x1e  used when description is null
    // vtable pointer                                 // 0x20  GCC 2.95 puts it after the data

    virtual ~SimBodyPart();
    virtual void DoStream(ReconBuffer *buffer, int version);
    virtual void CopyFrom(const SimBodyPart *other);
    virtual bool IsValid(bool a, bool b);
    virtual bool IsValidClothingSetIndex(bool a, bool b);
    virtual bool IsValidModelIndex(bool a, bool b);
    virtual bool IsValidModelCodeIndex(bool a, bool b);
    virtual bool IsValidStyleIndex(bool a, bool b);
    virtual bool IsValidTextureIndex(bool a, bool b);
    virtual bool IsValidDegradationState(bool a, bool b);
    virtual void Validate();
    virtual void SetClothingSetIndex(u8 index);
    virtual int GetType() const = 0;
    virtual int GetBodyPartIndex() const = 0;
    virtual int GetBodyPartDBIndex() const = 0;
    virtual int GetTattooLocation() const = 0;
    virtual bool IsLayered() const;
    virtual bool UseStyleCodeInTextureName() const;
    virtual bool IsRequiredBodyPart() const;
    virtual int GetLayoutMapNumber() const = 0;
    virtual TRect<u16> GetLayoutMapRect() const = 0;
    virtual int GetImageModelSetType() const = 0;
    virtual EVec3 GetDefaultCameraPos() const;
    virtual EVec3 GetDefaultCameraTarget() const;
    virtual int GetIconPoseAnimID() const;
    virtual u8 GetNumPossibleClothingSets() const;
    virtual u8 GetNumPossibleModelCodes() const;
    virtual u8 GetNumPossibleStyles(u8 modelCodeIndex) const;
    virtual u8 GetNumValidStyles(u8 modelCodeIndex) const;
    virtual u8 GetNumPossibleModels() const;
    virtual u8 GetNumPossibleTextureVariations() const;
    virtual u8 GetNumPossibleDegradationStates() const;
    virtual bool IsGeneric() const;
    virtual const char *GetModelCode() const;
    virtual const char *GetStyleCode() const;
    virtual const char *GetClothingSetName() const;
    virtual const char *GetTextureVariationName() const;
    virtual EString GetModelName() const;
    virtual EString GetTextureName(bool a) const;
    virtual EString GetReflectionMaskName(bool a) const;
    virtual EString GetIconTextureName() const;
    virtual u32 GetModelID() const;
    virtual u32 GetTextureID(bool a) const;
    virtual u32 GetReflectionMaskID(bool a) const;
    virtual u32 GetIconTextureID() const;
    virtual u8 GetDefaultClothingSetIndex(bool a) const;
    virtual void SetToDefault(bool a);
    virtual u8 GetNudeClothingSetIndex(bool a) const;
    virtual void SetToNude(bool a);
    virtual void SetToRandom(int seed);
    virtual u8 CalculateModelCodeIndexFromStyleIndex(u8 styleIndex) const;
    virtual u8 CalculateModelIndexFromModelCodeIndexAndStyleIndex(u8 modelCodeIndex, u8 styleIndex) const;
    virtual u8 CalculateModelCodeIndexFromModelIndex(u8 modelIndex) const;
    virtual u8 CalculateStyleIndexFromModelIndex(u8 modelIndex) const;
    virtual void SwitchToFirstAvailableDegradationStage();
    virtual void SwitchToFirstAvailableStyle();
    virtual void AdjustToLegalStyle();
    virtual void UpdateRelatedData();
    virtual u8 GetArmorValue(Weather::eDamageType type);
    virtual s32 GetHitPointsPerDegradationState();
    virtual s32 GetIsDamaged();
    virtual void SetIsDamaged(bool damaged);
    virtual s32 GetCurrentHitPoints();
    virtual void SetCurrentHitPoints(int hitPoints);
    virtual BitFlags<u32> ApplyDamage(const Weather::DamageEvents &events);
    virtual BitFlags<u32> Degrade();
    virtual BitFlags<u32> Repair();
    virtual void CreateIcon(SimModelTSC6 *model, EVec3 *cameraPos, EVec3 *cameraTarget, f32 a, u32 b,
                            u32 c, int d, int e, int f, bool g);
    virtual void GetRequiredDyesForTextureVariation(u8 textureIndex, s16 *dyes);
    virtual void GetValidClothingSetIndexes(
        eastl::vector<Sim::ClothingSetIndexInfo, eastl::allocator> &indexes);
    virtual u8 GetClothingSetUIIndex() const;
    virtual u8 GetDegradationModelCodeIndex() const;

    // Not virtual. The PS2 build inlines these three; the Wii build keeps them as functions.
    u8 GetClothingSetIndex() { return clothingSetIndex; }
    u8 &StyleIndex() { return styleIndex; }
    u8 &TextureIndex() { return textureIndex; }
    Sim::BodyPartGeometryTSC6 *GetClothingSetData() const;
    BodyPartTypeInfo *GetBodyPartTypeInfo() const;
    // Defined in CasSimDescription.h, because it needs that class. The PS2 build inlines it.
    inline bool GetCharacterFlag(int index) const;
};
STATIC_ASSERT(SimBodyPart_size, sizeof(SimBodyPart) == 0x24);

#endif
