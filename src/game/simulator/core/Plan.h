#ifndef GAME_SIMULATOR_CORE_PLAN_H
#define GAME_SIMULATOR_CORE_PLAN_H

#include "common/Vector.h"
#include "common/types.h"

// Wii: Plan, one thing a Sim can build. The offsets are from InterpValue's scope 0x43.
class Plan {
public:
    s16 field_0x0;            // 0x00
    s16 field_0x2;            // 0x02
    s16 field_0x4;            // 0x04
    s16 field_0x6;            // 0x06
    s16 field_0x8;            // 0x08
    u8 pad_0xa[0x36];         // 0x0a
    VECTOR<u8> mBuildMotives; // 0x40
    VECTOR<s8> mMotiveMin;    // 0x44
    VECTOR<s8> mMotiveMax;    // 0x48
    u8 pad_0x4c[0x12];        // 0x4c
    s16 field_0x5e;           // 0x5e

    int GetNumberOfBuildMotives() const { return mBuildMotives.size(); }
    int PutItemInHand() const;
    int PutItemOnGround() const;
    int PutItemInInventory() const;
    int AddTokenToInventory() const;
    s16 GetConstructionMotion() const;
    s16 GetToolUsed() const;
    s16 ResolveToolEnumToExistingToken(s16 tool) const;
    s16 GetResourcePile() const;
    s16 GetTimeToMake() const;
    s16 GetSkillToImprove() const;
};

// Wii: cheatIgnoreRequirements__11PlanManager, a bool there. The PS2 code reads a word.
class PlanManager {
public:
    static int cheatIgnoreRequirements;

    static PlanManager *GetInstance();
    Plan *GetPlanByID(s16 id);
    int IsPlanUnlocked(s16 id);
    void ToggleAllPlansInUI();
    Plan *GetPlanByTokenID(s16 token);
    void SendNotification(s8 notification, s16 value, cXPerson *person);
    void ToggleIgnoreRequirements();
    void ForceAllPlansToUnlock();
};

#endif
