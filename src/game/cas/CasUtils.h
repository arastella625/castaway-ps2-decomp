#ifndef GAME_CAS_CASUTILS_H
#define GAME_CAS_CASUTILS_H

#include "common/EGlobal.h"
#include "common/ERQuickdata.h"
#include "common/types.h"
#include "game/cas/CasSimDescription.h"

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

// Wii: UIDBGetInt__4UIDBFPCc. Reads a named integer from the UI database.
class UIDB {
public:
    static int UIDBGetInt(const char *name);
};

// Wii: cheatIgnoreRequirements__11PlanManager, a bool there. The PS2 code reads a word.
class PlanManager {
public:
    static int cheatIgnoreRequirements;
};

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

    // Inlined on PS2. The quickdata is loaded on first use (0x2A2AF469 is its resource id).
    static ERQuickdata *GetQuickdata()
    {
        ERQuickdata *data = _globals.casQuickdata;
        if (data == 0)
            data = _quickdataman.AddRef(0x2A2AF469, 0, 0);
        return data;
    }

    // Also inlined on PS2. The first row of the table, or null when it has none.
    static const CasSimTypeInfo *GetBodyPartTypeInfoTable()
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

    // The PS2 build inlines this into its callers; the Wii build keeps it as a function.
    static u8 GetBodyPartIndex(Character::eBodyPart part)
    {
        int index = part - 0x1a;
        if (index >= 0 && index < 15)
            return index;
        if (part >= 0 && part <= 0x19)
            return GetS2CEquivalentBodyPart(part) - 0x1a;
        return 14;
    }

    // Also inlined on PS2. 0x27 is the sleeves, which share the torso's database index;
    // 0x7f means the part has none.
    static u8 GetBodyPartDBIndex(Character::eBodyPart part)
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
};

#endif
