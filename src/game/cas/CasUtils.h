#ifndef GAME_CAS_CASUTILS_H
#define GAME_CAS_CASUTILS_H

#include "game/simulator/core/Plan.h"
#include "common/EGlobal.h"
#include "common/ERQuickdata.h"
#include "common/EResourceManager.h"
#include "common/types.h"
#include "game/cas/CasSimDescription.h"
#include "game/UIDB.h"

// The function at 0x3D9930. Ghidra's name list calls it EChecksum::ComputeSymbol; the
// parameter types are read from its callers.
class EChecksum {
public:
    static u32 ComputeSymbol(const char *suffix, u32 id);
};

// Wii: _gd, the game's global data. Only the two words UseArchetype tests are known.
struct GlobalData {
    u8 field_0x0[0x48]; // 0x00
    int field_0x48;     // 0x48
    int field_0x4c;     // 0x4c
};
extern GlobalData _gd;


namespace Sim {

// Wii: Sim::CostumesTSC6, one row of the costume table. Each list holds one clothing set
// name per body part database index; an empty name means the costume leaves that part alone.
struct CostumesTSC6 {
    u32 field_0x0;                 // 0x00
    VECTOR<char *> flagSetNames;   // 0x04  used when the caller's bool is true
    VECTOR<char *> flagClearNames; // 0x08
};

}  // namespace Sim

// The tattoo a new sim starts with at each location: [location][flag 0 clear]. The eight
// bytes at 0x4A9950, all zero. The name is ours.
extern const u8 gDefaultTattooIndices[Sim::TATTOO_LOCATION_COUNT][2];

// Wii: zodiacPersonalityValues, five values per star sign. A new sim gets the first row.
extern u8 zodiacPersonalityValues[][PERSONALITY_COUNT];

// Static helpers. Only the functions the decompiled code uses are declared so far.
class CasUtils {
public:
    static Character::eBodyPart GetS2CEquivalentBodyPart(Character::eBodyPart part);
    static u8 GetClothingSetIndexByName(bool a, Character::eBodyPart part, const char *name);
    static const char *GetBodyPartName(Character::eBodyPart part);
    static Character::eBodyPart GetBodyPartByName(const char *name);

    // Inlined on PS2. The quickdata is loaded on first use (0x2A2AF469 is its resource id).
    static ERQuickdata *GetQuickdata()
    {
        ERQuickdata *data = _globals.casQuickdata;
        if (data == 0)
            data = _quickdataman.AddRef(0x2A2AF469, 0, 0);
        return data;
    }

    // The first row of the table, or null when it has none. The game has this inlined and as
    // a function (0x143B20, called by ClothingItem::GetBodyPartTypeInfo), so the inline body
    // has its own name; the function is in CasUtils.cpp.
    static const CasSimTypeInfo *GetBodyPartTypeInfoTable();
    static const CasSimTypeInfo *GetBodyPartTypeInfoTableInline()
    {
        const ERQTable *table = GetQuickdata()->GetTable("Sim::BodyPartTypeInfoTableTSC6");
        if (table == 0 || table->count == 0)
            return 0;
        return (const CasSimTypeInfo *)table->rows;
    }

    // Also inlined on PS2.
    static const Sim::CostumesTSC6 *GetCostumeData(const char *name)
    {
        ERQuickdata *data = GetQuickdata();
        return (const Sim::CostumesTSC6 *)data->GetRow(data->GetTable("Sim::CostumesTSC6"), name);
    }

    // Also inlined on PS2. Turns a stored texture id into the one to use now: when the
    // game-state switch is set, or the caller asks, a non-zero id is rehashed with "_GAME".
    static u32 GetGameStateTextureID(u32 id, bool a)
    {
        if (_globals.useGameStateTextures) {
            if (id != 0)
                return EChecksum::ComputeSymbol("_GAME", id);
        } else if (id != 0 && a) {
            return EChecksum::ComputeSymbol("_GAME", id);
        }
        return id;
    }

    // Also inlined on PS2. True in the Create-a-Sim modes that pick a ready-made archetype.
    static bool UseArchetype()
    {
        bool use = false;
        if (_gd.field_0x4c != 0 || _gd.field_0x48 != 0) {
            switch (UIDB::UIDBGetInt("CAS_mode")) {
            case 0:
            case 6:
            case 7:
            case 8:
            case 9:
                use = true;
                break;
            }
        }
        return use;
    }

    // Also inlined on PS2.
    static s16 GetCASNeighborID()
    {
        int id = 0;
        if (_gd.field_0x4c != 0 || _gd.field_0x48 != 0)
            id = UIDB::UIDBGetInt("CAS_neighborID");
        return id;
    }

    // Inlined into its callers, and also a function (0x143B90) for ClothingItem's inlines.
    static u8 GetBodyPartIndex(Character::eBodyPart part);
    static u8 GetBodyPartIndexInline(Character::eBodyPart part)
    {
        int index = part - 0x1a;
        if (index >= 0 && index < 15)
            return index;
        if (part >= 0 && part <= 0x19)
            return GetS2CEquivalentBodyPart(part) - 0x1a;
        return 14;
    }

    // 0x27 is the sleeves, which share the torso's database index; 0x7f means the part has
    // none. Inlined into its callers, and also a function (0x143BD8) for GetClothingSetData.
    static u8 GetBodyPartDBIndex(Character::eBodyPart part);
    static u8 GetBodyPartDBIndexInline(Character::eBodyPart part)
    {
        int index = part - 0x1a;
        if (index >= 0 && index < 13)
            return index;
        if (part >= 0 && part <= 0x19) {
            Character::eBodyPart converted = GetS2CEquivalentBodyPart(part);
            index = converted - 0x1a;
            if (index >= 0 && index < 13)
                return index;
            part = converted;
        }
        if (part == 0x27)
            return 4;
        return 0x7f;
    }

    // Wii: GetBodyData. The "Sim::SimPartsTSC6" row for one sex. The PS2 build inlines it
    // into GetClothingSetData, and takes the flag as a word.
    static const CasSimTextureInfo *GetBodyData(int male)
    {
        ERQuickdata *data = GetQuickdata();
        const ERQTable *table = data->GetTable("Sim::SimPartsTSC6");
        return (const CasSimTextureInfo *)data->GetRow(table, male ? "male" : "female");
    }
    // One clothing set of one body part, or null. 0x129D30.
    static Sim::BodyPartGeometryTSC6 *GetClothingSetData(int male, Character::eBodyPart part,
                                                         u8 setIndex);

    // Also inlined on PS2.
    static u32 GetNoneIconTextureID()
    {
        char buffer[128];
        Snprintf(buffer, 128, "%s%s", "none", "_ICON");
        return EResourceManager::CalcId(buffer, true);
    }

    // Also inlined on PS2.
    static u8 CalculateClothingSetIndexInfoFlags(Character::eBodyPart part, bool male,
                                                 u8 setIndex, ClothingItem *item)
    {
        u8 flags = 0;
        if (part >= 0x1a) {
            if (part < 0x1d) {
                flags |= 1;
            } else if (part < 0x28 && item != 0) {
                const Sim::BodyPartGeometryTSC6 *data = GetClothingSetData(male, part, setIndex);
                if (data != 0 && (data->flags & 0x8000) != 0)
                    flags |= 1;
            }
        }
        return flags;
    }
};

#endif
