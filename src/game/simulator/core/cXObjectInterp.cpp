#include "game/simulator/core/cXObject.h"

#include "common/ERQuickdata.h"
#include "game/simulator/core/Inventory.h"
#include "game/simulator/core/Lot.h"
#include "game/simulator/core/ObjSelector.h"
#include "game/simulator/core/Plan.h"
#include "game/simulator/core2/ObjTestSim.h"
#include "game/simulator/core2/cXPerson.h"
#include "game/simulator/objectdata/XObjLang.h"

// Wii: tempDataPrim. What a check-tree ad scope points at when ads aren't allowed.
extern s16 tempDataPrim;

// Wii: OnEdithSetFlagField1 sets this. What reads it isn't worked out yet.
extern int sEdithFlagFieldChanged;

#define OBJECT_CHECK(condition, code) TREE_CHECK(this, condition, code, return -1)

// The operand is the s16 at this address: hand back both the address and the value.
#define REF_VALUE(address)         \
    {                              \
        const s16 *p = (address);  \
        *ref = p;                  \
        value = *p;                \
    }

// Rounds half away from zero.
inline int fround(f32 value)
{
    return (int)(value < 0.0f ? value - 0.5f : value + 0.5f);
}

// Person data: most fields live in the Sim, some in its Neighbor, and an old index is
// translated to the new one first. A field the two tables don't have was stopped earlier.
// Only the first of the four person data scopes spells this out; the others jump into it.
#define PERSON_DATA_GOTO(isPerson, isNeighbor)                       \
    if (isPerson)                                                    \
        goto personField;                                            \
    if (isNeighbor)                                                  \
        goto neighborField;                                          \
    REF_VALUE(person->GetPersonDataArray() + data);

#ifdef NON_MATCHING
// About 2,900 of the 3,060 words line up; the register allocation, stack frame and block order
// all agree with the game, so what follows reads the same as the game's code. What still differs:
// - Scope 0x22: the game has a compare with 7 and a branch to the end, then v0 = 1 in a delay
//   slot, though neither does anything. Wii has the same compare. Nothing tried here keeps it.
// - The neighbour lookup: the game loads into v0 and copies to the variable (lw, move) in scopes
//   0x20, 0x26 and 0x2e, and sets zero straight into the variable. This loads straight into it.
// - Scopes 0x34 to 0x37: the same tail as this but with v0 and v1 swapped, which stops them
//   merging with the last block the way the game's do, and shifts the alignment nops after them.
// - Scope 0x1a: the game retests the constants pointer in the else branch; here that's folded.
// - Scope 0x2a: the game keeps a null test on the table that does nothing, with the row loop's
//   exit block placed inside it.
// Reads one operand. scope picks where it lives and data says which one (bhav.md has the list).
// value gets the number. ref gets its address when it can be written through, and floatRef the
// address of a motive, which is a float underneath.
long cXObject::InterpValue(s16 scope, s16 data, const s16 **ref, f32 **floatRef, s16 *valueOut, s16 **unused)
{
    const s16 *nothing = 0;
    s16 value = 0;
    cXObject *object;
    cXPerson *person;
    bool isPerson;
    bool isNeighbor;

    if (ref == 0)
        ref = &nothing;
    *ref = 0;
    if (floatRef)
        *floatRef = 0;
    if (unused)
        *unused = 0;

    switch (scope) {
    case 0x23: {
        // Whether the stack object's type has this function.
        object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 10);
        value = object->mSelector->GetFnTable()->HasFunction(data) != 0;
        break;
    }
    case 0x22:
        // A neighbourhood variable.
        OBJECT_CHECK(data > 0 && data < 0x24, 0x3c);
        REF_VALUE(&GetNeighborhood()->mVars[data]);
        if (data != 7)
            do {
            } while (0);
        break;
    case 0x21:
        TREE_ERROR(0x3a);
        return -1;
    case 0x1b: {
        // The stack object's indexed data, with the index in one of my temps.
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            s16 index = mTemps[data];
            OBJECT_CHECK(index >= 0 && index < object->mNumIndexedData, 0x32);
            REF_VALUE(&object->mIndexedData[index]);
            break;
        }
        OBJECT_CHECK(0, 10);
        break;
    }
    case 0x1a:
    case 0x2d: {
        // A tree constant. Scope 0x1a packs the table and index in data; 0x2d has the table in
        // temp 0, and index 0x7f means temp 1 holds it.
        s16 tableID = 0;
        int index = 0;
        if (scope == 0x1a) {
            if (!XObjLang::GetConstantsID(data, &tableID, &index)) {
                TREE_ERROR(0x2f);
                return -1;
            }
            if (index == 0x7f)
                index = mTemps[0];
        } else {
            tableID = mTemps[0];
            index = data;
            if (data == 0x7f)
                index = mTemps[1];
        }
        Behavior *behavior = GetCurElem()->mBehavior;
        ConsoleAutoRefCount<ObjectDataBehaviorConstants> constants(0);
        OBJECT_CHECK(behavior->GetResFileID(tableID) != (u32)-1, 0x2f);
        if (behavior->GetConstants(tableID, constants.AsPPTypeParam())) {
            OBJECT_CHECK(index >= 0 && index < constants->CountValues(), 0x2f);
            value = constants->GetValue(index);
        } else {
            OBJECT_CHECK(constants.mObject, 0x2f);
        }
        break;
    }
    case 0x18:
        // Data the neighbourhood keeps for the stack object.
        value = GetNeighborhood()->GetStackObjectValue(GetCurElem()->mStackObject, data, ref);
        break;
    case 0x17: {
        // The room whose number is in temp 0. 0xfffb is the room outside.
        u16 roomID = mTemps[0];
        switch (data) {
        case 0: {
            if (roomID == 0xfffb) {
                value = 0;
                break;
            }
            Room *room = RoomManager::sRoomMgr->GetRoom(roomID);
            OBJECT_CHECK(room, 0x28);
            value = (int)(room->GetAmbientLight() * 100.0f);
            break;
        }
        case 1: {
            if (roomID == 0xfffb) {
                value = 1;
                break;
            }
            Room *room = RoomManager::sRoomMgr->GetRoom(roomID);
            OBJECT_CHECK(room, 0x28);
            value = room->IsOutside() != 0;
            break;
        }
        case 2: {
            if (roomID == 0xfffb) {
                value = 0;
                break;
            }
            Room *room = RoomManager::sRoomMgr->GetRoom(roomID);
            OBJECT_CHECK(room, 0x28);
            value = 1;
            break;
        }
        case 3: {
            if (roomID == 0xfffb) {
                value = 0;
                break;
            }
            Room *room = RoomManager::sRoomMgr->GetRoom(roomID);
            OBJECT_CHECK(room, 0x28);
            value = room->GetArea();
            break;
        }
        case 4: {
            Room *room = RoomManager::sRoomMgr->GetRoom(roomID);
            OBJECT_CHECK(room, 0x28);
            value = (int)RoomManager::sRoomMgr->GetRoomEnvironmentScore(roomID);
            break;
        }
        default:
            OBJECT_CHECK(0, 0x29);
        }
        break;
    }
    case 0x16: {
        // The stack object's attribute, with the index in a parameter.
        OBJECT_CHECK(data >= 0 && data < GetCurElem()->mNumParams, 8);
        data = GetCurElem()->GetParams()[data];
        object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(data < object->mNumAttrs, 6);
        OBJECT_CHECK(object, 10);
        REF_VALUE(&object->mAttrs[data]);
        break;
    }
    case 0x15: {
        // A word of the stack object's definition.
        OBJECT_CHECK(data < 0x62, 0x23);
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            value = object->mDef[data];
            break;
        }
        OBJECT_CHECK(0, 10);
        break;
    }
    case 0x12: {
        // My person data.
        isPerson = cXPerson::isValidPersonDataField(data);
        isNeighbor = Neighbor::IsValidField(data);
        OBJECT_CHECK(GetType() == 2, 0x1c);
        OBJECT_CHECK(data >= 0 && (isPerson || isNeighbor), 0x21);
        person = GetPerson();
        if (isPerson) {
        personField:
            data = cXPerson::TranslasteToNewPersonDataIndex(data, true);
            REF_VALUE(person->GetPersonDataArray() + data);
        } else if (isNeighbor) {
        neighborField:
            data = Neighbor::sIndexTable[data];
            REF_VALUE(person->GetNeighbor()->GetPersonDataArray() + data);
        } else {
            REF_VALUE(person->GetPersonDataArray() + data);
        }
        break;
    }
    case 0x1e: {
        // My person data, with the field in one of my temps.
        OBJECT_CHECK(GetType() == 2, 0x1c);
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        data = mTemps[data];
        isPerson = cXPerson::isValidPersonDataField(data);
        isNeighbor = Neighbor::IsValidField(data);
        OBJECT_CHECK(data >= 0 && (isPerson || isNeighbor), 0x21);
        person = GetPerson();
        PERSON_DATA_GOTO(isPerson, isNeighbor);
        break;
    }
    case 0x1f: {
        // The stack object's person data, with the field in one of my temps.
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        data = mTemps[data];
        isPerson = cXPerson::isValidPersonDataField(data);
        isNeighbor = Neighbor::IsValidField(data);
        OBJECT_CHECK(data >= 0 && (isPerson || isNeighbor), 0x21);
        person = object->GetPerson();
        PERSON_DATA_GOTO(isPerson, isNeighbor);
        break;
    }
    case 0x20: {
        // The person data of the neighbour whose ID is the stack object.
        Neighbor *neighbor = GetNeighborhood()->FindNeighborByIDInline(GetCurElem()->mStackObject);
        OBJECT_CHECK(neighbor, 0x38);
        OBJECT_CHECK(data >= 0 && Neighbor::IsValidField(data), 0x21);
        value = neighbor->mPersonData[Neighbor::sIndexTable[data]];
        break;
    }
    case 0x13: {
        // The stack object's person data.
        isPerson = cXPerson::isValidPersonDataField(data);
        isNeighbor = Neighbor::IsValidField(data);
        OBJECT_CHECK(data >= 0 && (isPerson || isNeighbor), 0x21);
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        person = object->GetPerson();
        PERSON_DATA_GOTO(isPerson, isNeighbor);
        break;
    }
    case 0x2e: {
        // A variable of the family of the neighbour whose ID is the stack object.
        Neighbor *neighbor = GetNeighborhood()->FindNeighborByIDInline(GetCurElem()->mStackObject);
        OBJECT_CHECK(neighbor, 0x38);
        Family *family = neighbor->GetFamilyInline();
        if (family && family->GetVarRef(data, ref) && *ref)
            value = **ref;
        break;
    }
    case 0x0c:
        // The range of ad number data of the check tree being run.
        if (cXPerson::sbAllowingCheckTreeAds) {
            CheckTreeAds *ads = ObjTestSim::sCheckTreeModEntry;
            OBJECT_CHECK(data >= 0 && data < ads->CountAds(), 0x1a);
            REF_VALUE(&ads->mAds[data].mRange);
            break;
        }
        goto noAds;
    case 0x1c:
        if (cXPerson::sbAllowingCheckTreeAds) {
            CheckTreeAds *ads = ObjTestSim::sCheckTreeModEntry;
            OBJECT_CHECK(data >= 0 && data < ads->CountAds(), 0x1a);
            REF_VALUE(&ads->mAds[data].mPersonalityVar);
            break;
        }
        goto noAds;
    case 0x1d:
        if (cXPerson::sbAllowingCheckTreeAds) {
            CheckTreeAds *ads = ObjTestSim::sCheckTreeModEntry;
            OBJECT_CHECK(data >= 0 && data < ads->CountAds(), 0x1a);
            REF_VALUE(&ads->mAds[data].mMin);
            break;
        }
    noAds:
        tempDataPrim = 0;
        value = 0;
        *ref = &tempDataPrim;
        break;
    case 0x14: {
        // The object in one of my slots.
        ObjectSlot *slot = GetObjectSlot(data);
        if (slot) {
            value = slot->mObjectID;
            break;
        }
        OBJECT_CHECK(0, 0x1b);
        break;
    }
    case 0x10: {
        // The object in one of the stack object's slots.
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            ObjectSlot *slot = object->GetObjectSlot(data);
            if (slot) {
                value = slot->mObjectID;
                break;
            }
            OBJECT_CHECK(0, 0x1b);
        }
        OBJECT_CHECK(0, 0x15);
        break;
    }
    case 0x0f: {
        // The stack object's motive.
        OBJECT_CHECK(data >= 0 && data < 16, 0x1a);
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        cXPerson *person = object->GetPerson();
        value = fround(person->GetMotive(data));
        if (floatRef)
            *floatRef = person->GetMotiveRef(data);
        break;
    }
    case 0x11: {
        // The stack object's motive, with the motive in one of my temps.
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        OBJECT_CHECK(mTemps[data] >= 0 && mTemps[data] < 16, 0x1a);
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        cXPerson *person = object->GetPerson();
        value = fround(person->GetMotive(mTemps[data]));
        if (floatRef)
            *floatRef = person->GetMotiveRef(mTemps[data]);
        break;
    }
    case 0x0e: {
        // My motive.
        OBJECT_CHECK(data >= 0 && data < 16, 0x1a);
        OBJECT_CHECK(GetType() == 2, 0x1c);
        cXPerson *person = GetPerson();
        value = fround(person->GetMotive(data));
        if (floatRef)
            *floatRef = person->GetMotiveRef(data);
        break;
    }
    case 0x00:
        OBJECT_CHECK(data >= 0 && data < mNumAttrs, 6);
        REF_VALUE(&mAttrs[data]);
        break;
    case 0x02:
        OBJECT_CHECK(data >= 0 && data < mNumSemiAttrs, 6);
        REF_VALUE(&mSemiAttrs[data]);
        break;
    case 0x40:
        OBJECT_CHECK(data >= 0 && data < mNumStaticAttrs, 6);
        REF_VALUE(&mStaticAttrs[data]);
        break;
    case 0x24:
        OBJECT_CHECK(data >= 0 && data < mSelector->CountTypeAttributes(), 0x40);
        REF_VALUE(&mSelector->GetTypeAttributes()[data]);
        break;
    case 0x0a: {
        // The stack object's ID, as a frame holds it. data must be 0, for the top frame.
        OBJECT_CHECK(data == 0, 15);
        StackElem *elem = GetNthElem(GetStackSize() - 1 - data);
        REF_VALUE(&elem->mStackObject);
        break;
    }
    case 0x01: {
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            OBJECT_CHECK(data >= 0 && data < object->mNumAttrs, 6);
            REF_VALUE(&object->mAttrs[data]);
            break;
        }
        OBJECT_CHECK(0, 10);
        break;
    }
    case 0x05: {
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            OBJECT_CHECK(data >= 0 && data < object->mNumSemiAttrs, 6);
            REF_VALUE(&object->mSemiAttrs[data]);
            break;
        }
        OBJECT_CHECK(0, 10);
        break;
    }
    case 0x41: {
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            OBJECT_CHECK(data >= 0 && data < object->mNumStaticAttrs, 6);
            REF_VALUE(&object->mStaticAttrs[data]);
            break;
        }
        OBJECT_CHECK(0, 10);
        break;
    }
    case 0x25: {
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            OBJECT_CHECK(data >= 0 && data < object->mSelector->CountTypeAttributes(), 0x40);
            REF_VALUE(&object->mSelector->GetTypeAttributes()[data]);
            break;
        }
        OBJECT_CHECK(0, 10);
        break;
    }
    case 0x03: {
        // One of my variables. Reading variable 0x19 counts as activity.
        int found = ContainsData(data);
        OBJECT_CHECK(found, 7);
        REF_VALUE(&mVars[data]);
        if (data == 0x19)
            mModule->ClearIdleStatus(mID);
        break;
    }
    case 0x04: {
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (!object) {
            TREE_ERROR(10);
            return -1;
        }
        REF_VALUE(&object->mVars[data]);
        if (data == 0x19)
            object->mModule->ClearIdleStatus(object->mID);
        break;
    }
    case 0x06:
        // A global. Four of them aren't plain memory.
        OBJECT_CHECK(data >= 0 && data < 0x30, 0x3f);
        if (GetSimulator()->GetGlobalRef(data, ref)) {
            value = **ref;
        } else if (data == 0x2e) {
            value = GetSimulator()->GetPlayerCheatsValue();
        } else if (data == 0x20) {
            value = GetSimulator()->GetPlayerCheatsValue() >> 16;
        } else if (data == 0xb) {
            value = LotManager::Instance()->WhereIsHomeTiki();
        } else {
            value = GetSimulator()->GetGlobal(data);
        }
        break;
    case 0x07:
        // A literal.
        value = data;
        break;
    case 0x08:
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        REF_VALUE(&mTemps[data]);
        break;
    case 0x0d: {
        object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            OBJECT_CHECK(data >= 0 && data < 8, 2);
            REF_VALUE(&object->mTemps[data]);
            break;
        }
        OBJECT_CHECK(0, 10);
        break;
    }
    case 0x0b: {
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        s16 index = mTemps[data];
        OBJECT_CHECK(index >= 0 && index < 8, 2);
        REF_VALUE(&mTemps[index]);
        break;
    }
    case 0x09:
        OBJECT_CHECK(data >= 0 && data < GetCurElem()->mNumParams, 8);
        REF_VALUE(&GetCurElem()->GetParams()[data]);
        break;
    case 0x19:
        OBJECT_CHECK(data >= 0 && data < GetCurElem()->mNumLocals, 8);
        REF_VALUE(&GetCurElem()->GetLocals()[data]);
        break;
    case 0x28:
        value = LotManager::Instance()->HasLotBeenVisited(data) != 0;
        break;
    case 0x29: {
        GoalAndUnlockData *goals = &GetNeighborhood()->mGoals;
        OBJECT_CHECK(data >= 0 && data < goals->Count(4), 0x3c);
        value = goals->Get(4, data) != 0;
        break;
    }
    case 0x26: {
        // A word of the definition of the neighbour whose ID is the stack object.
        Neighbor *neighbor = GetNeighborhood()->FindNeighborByIDInline(GetCurElem()->mStackObject);
        OBJECT_CHECK(neighbor, 0x38);
        OBJECT_CHECK(data >= 0 && data < 0x62, 0x23);
        ObjSelector *selector = neighbor->mSelector;
        OBJECT_CHECK(selector || neighbor, 0x38);
        value = selector->mDefinition[data];
        break;
    }
    case 0x2b:
    case 0x2c: {
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        cXPerson *simPerson = object->GetPerson();
        OBJECT_CHECK(simPerson, 0x1c);
        void *description = simPerson->GetSimDescription();
        OBJECT_CHECK(description, 0x1c);
        value = scope == 0x2b ? 9 : 0;
        break;
    }
    case 0x27:
        value = 0;
        break;
    case 0x2a: {
        // The ingredient whose ID is in temp 0.
        s16 id = mTemps[0];
        const IngredientToken *token = GetIngredientManager()->FindIngredientToken(id);
        OBJECT_CHECK(token, 0x5d);
        switch (data) {
        case 0:
            value = token->field_0x2;
            break;
        case 1:
            value = token->field_0x38;
            break;
        case 2:
            value = token->field_0xc;
            break;
        case 3:
            value = token->field_0x10;
            break;
        case 4:
            value = token->field_0x14;
            break;
        case 5:
            value = token->field_0x18;
            break;
        case 6:
            value = token->field_0x1c;
            break;
        }
        break;
    }
    case 0x42: {
        // The inventory token whose ID is in temp 0.
        const InventoryTokens *token = InventoryItems::GetTokenFromID(mTemps[0]);
        OBJECT_CHECK(token, 0x5d);
        switch (data) {
        case 0:
            value = token->field_0x1c;
            break;
        case 1:
            value = token->field_0x20;
            break;
        case 2:
            value = token->field_0x10;
            break;
        case 3:
            value = token->field_0x1e;
            break;
        case 4:
            value = token->field_0x22;
            break;
        case 5:
            value = token->field_0x14;
            break;
        case 6:
            value = 0;
            break;
        case 7:
            value = token->field_0xe;
            break;
        case 8:
            value = token->field_0x4;
            break;
        case 9:
            value = token->field_0x6;
            break;
        case 10:
            value = token->field_0x0;
            break;
        case 11:
            value = token->field_0x2;
            break;
        case 12: {
            InventoryItems *tribe = InventoryItems::GetTribeInventory();
            if (tribe)
                value = tribe->GetTokenItemIndex(mTemps[0]);
            else
                value = -1;
            break;
        }
        case 13:
            value = token->field_0x18;
            break;
        case 14:
            value = token->field_0x1a;
            break;
        case 15:
            value = token->GetHarvestTool();
            break;
        case 16: {
            value = 0;
            InventoryItems *tribe = InventoryItems::GetTribeInventory();
            if (tribe)
                value = tribe->HasTokenEverBeenCollected(token->mID);
            break;
        }
        }
        break;
    }
    case 0x43: {
        // The plan whose ID is in temp 0.
        s16 planID = mTemps[0];
        Plan *plan = PlanManager::GetInstance()->GetPlanByID(planID);
        OBJECT_CHECK(plan, 0x5d);
        switch (data) {
        case 0:
            value = PlanManager::GetInstance()->IsPlanUnlocked(planID) != 0;
            break;
        case 1:
            value = plan->field_0x2;
            break;
        case 2:
            value = plan->field_0x8;
            break;
        case 3:
            value = plan->field_0x4;
            break;
        case 4:
            value = plan->field_0x6;
            break;
        case 5:
            value = plan->PutItemInHand() != 0;
            break;
        case 6:
            value = plan->PutItemOnGround() != 0;
            break;
        case 7:
            value = plan->PutItemInInventory() != 0;
            break;
        case 8:
            value = plan->AddTokenToInventory() != 0;
            break;
        case 9:
            value = plan->GetConstructionMotion();
            break;
        case 10:
            value = plan->ResolveToolEnumToExistingToken(plan->GetToolUsed());
            break;
        case 11:
            value = plan->GetResourcePile();
            break;
        case 12:
            value = plan->GetToolUsed();
            break;
        case 13:
            value = plan->GetTimeToMake();
            break;
        case 14:
            if (plan->GetNumberOfBuildMotives() > 0)
                value = plan->mBuildMotives[0];
            else
                value = 0;
            break;
        case 15:
            if (plan->GetNumberOfBuildMotives() > 0)
                value = plan->mMotiveMin[0];
            else
                value = 0;
            break;
        case 16:
            if (plan->GetNumberOfBuildMotives() > 0)
                value = plan->mMotiveMax[0];
            else
                value = 0;
            break;
        case 17:
            if (plan->GetNumberOfBuildMotives() > 1)
                value = plan->mBuildMotives[1];
            else
                value = 0;
            break;
        case 18:
            if (plan->GetNumberOfBuildMotives() > 1)
                value = plan->mMotiveMin[1];
            else
                value = 0;
            break;
        case 19:
            if (plan->GetNumberOfBuildMotives() > 1)
                value = plan->mMotiveMax[1];
            else
                value = 0;
            break;
        case 20:
            if (plan->GetNumberOfBuildMotives() > 2)
                value = plan->mBuildMotives[2];
            else
                value = 0;
            break;
        case 21:
            if (plan->GetNumberOfBuildMotives() > 2)
                value = plan->mMotiveMin[2];
            else
                value = 0;
            break;
        case 22:
            if (plan->GetNumberOfBuildMotives() > 2)
                value = plan->mMotiveMax[2];
            else
                value = 0;
            break;
        case 23:
            value = plan->GetSkillToImprove();
            break;
        case 24:
            value = plan->field_0x5e;
            break;
        }
        break;
    }
    case 0x44: {
        // The lot transition whose ID is in temp 0.
        s16 id = mTemps[0];
        switch (data) {
        case 0:
            value = LotManager::Instance()->IsTransitionOpen(id, false) != 0;
            break;
        case 1:
            value = LotManager::Instance()->IsTransitionOpen(id, true) != 0;
            break;
        case 2:
            value = LotManager::Instance()->HasTransitionBeenUsed(id);
            break;
        }
        break;
    }
    case 0x2f: {
        // An action in the stack object's queue: data 0 and 1 are the first action's stack
        // object and tree tab index, 2 and 3 the second's.
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x1c);
        cXPerson *person = object->GetPerson();
        OBJECT_CHECK(person, 0x1c);
        *ref = 0;
        Interaction action;
        if (data >= 0 && data <= 3) {
            s16 slot = data / 2;
            if (!person->mActionQueue.Peek(slot, action)) {
                value = -1;
                break;
            }
        }
        switch (data) {
        case 0:
        case 2:
            if (action.GetStackObject())
                value = action.GetStackObject()->mID;
            else
                value = 0;
            break;
        case 1:
        case 3:
            value = action.GetTreeTabEntryIndex();
            break;
        default:
            TREE_ERROR(7);
            return -1;
        }
        break;
    }
    case 0x30:
        OBJECT_CHECK(GetType() == 2, 0x1c);
        OBJECT_CHECK(data >= 0 && data < 0x24, 0x21);
        REF_VALUE(GetPerson()->GetPersonDataArray() + data);
        break;
    case 0x31: {
        OBJECT_CHECK(GetType() == 2, 0x1c);
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        data = mTemps[data];
        OBJECT_CHECK(data >= 0 && data < 0x24, 0x21);
        REF_VALUE(GetPerson()->GetPersonDataArray() + data);
        break;
    }
    case 0x32: {
        OBJECT_CHECK(data >= 0 && data < 0x24, 0x21);
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        REF_VALUE(object->GetPerson()->GetPersonDataArray() + data);
        break;
    }
    case 0x33: {
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        data = mTemps[data];
        OBJECT_CHECK(data >= 0 && data < 0x24, 0x21);
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        REF_VALUE(object->GetPerson()->GetPersonDataArray() + data);
        break;
    }
    case 0x34:
    case 0x3a:
        OBJECT_CHECK(GetType() == 2, 0x1c);
        OBJECT_CHECK(data >= 0 && data < 0x31, 0x21);
        {
            Neighbor *neighbor = GetPerson()->GetNeighbor();
            REF_VALUE(neighbor->GetPersonDataArray() + data);
        }
        break;
    case 0x35:
    case 0x3b: {
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        data = mTemps[data];
        OBJECT_CHECK(GetType() == 2, 0x1c);
        OBJECT_CHECK(data >= 0 && data < 0x31, 0x21);
        {
            Neighbor *neighbor = GetPerson()->GetNeighbor();
            REF_VALUE(neighbor->GetPersonDataArray() + data);
        }
        break;
    }
    case 0x36:
    case 0x3c: {
        OBJECT_CHECK(data >= 0 && data < 0x31, 0x21);
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        {
            Neighbor *neighbor = object->GetPerson()->GetNeighbor();
            REF_VALUE(neighbor->GetPersonDataArray() + data);
        }
        break;
    }
    case 0x37:
    case 0x3d: {
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        data = mTemps[data];
        OBJECT_CHECK(data >= 0 && data < 0x31, 0x21);
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        OBJECT_CHECK(object, 0x15);
        OBJECT_CHECK(object->GetType() == 2, 0x1c);
        {
            Neighbor *neighbor = object->GetPerson()->GetNeighbor();
            REF_VALUE(neighbor->GetPersonDataArray() + data);
        }
        break;
    }
    case 0x38:
    case 0x3e: {
        OBJECT_CHECK(data >= 0 && data < 0x31, 0x21);
        Neighbor *neighbor = GetNeighborhood()->FindNeighborByIDInline(GetCurElem()->mStackObject);
        OBJECT_CHECK(neighbor, 0x38);
        REF_VALUE(neighbor->GetPersonDataArray() + data);
        break;
    }
    case 0x39:
    case 0x3f: {
        OBJECT_CHECK(data >= 0 && data < 8, 2);
        data = mTemps[data];
        OBJECT_CHECK(data >= 0 && data < 0x31, 0x21);
        Neighbor *neighbor = GetNeighborhood()->FindNeighborByIDInline(GetCurElem()->mStackObject);
        OBJECT_CHECK(neighbor, 0x38);
        REF_VALUE(neighbor->GetPersonDataArray() + data);
        break;
    }
    default:
        TREE_ERROR(9);
        return -1;
    }

    if (valueOut)
        *valueOut = value;
    return 1;
}

#endif

// Writes one operand back. ref is what InterpValue returned for it, so the scopes that are
// plain memory only have to store through it.
int cXObject::InterpWriteValue(s16 scope, s16 data, const s16 **ref, s16 value)
{
    switch (scope) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x05:
    case 0x06:
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x12:
    case 0x13:
    case 0x16:
    case 0x18:
    case 0x19:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x1f:
    case 0x22:
    case 0x24:
    case 0x25:
    case 0x2e:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39:
    case 0x3a:
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f:
    case 0x40:
    case 0x41:
        *(s16 *)*ref = value;
        break;
    case 0x03: {
        EdithVariableSet *vars = &mVars;
        int found = vars->Contains(data);
        TREE_CHECK(this, found, 7, return -1);
        *ref = vars->WriteVar(data, value);
        break;
    }
    case 0x04: {
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            EdithVariableSet *vars = &object->mVars;
            int found = vars->Contains(data);
            TREE_CHECK(this, found, 7, return -1);
            *ref = vars->WriteVar(data, value);
            break;
        }
        TREE_CHECK(this, 0, 10, return -1);
    }
    default:
        TREE_ERROR(9);
        return -1;
    }
    return 1;
}

// Wii: OnEdithSetFlagField1, inlined. Bit 0x2000 of an object's flag word changing is noticed.
inline void OnEdithSetFlagField1(int mask, bool wasSet, bool isSet)
{
    if (mask == 0x2000 && wasSet != isSet)
        sEdithFlagFieldChanged = 1;
}

#ifdef NON_MATCHING
// The size is exact and about 575 of 628 words line up; the rest is alignment that follows
// from these. What differs:
// - Where the left operand might be a motive, the game tests the float pointer in a0 and takes
//   a branch-likely with its delay slot from the target. OBJECT_CHECK gets a0 but not the
//   branch-likely; a plain if gets the branch-likely but puts the pointer in v0.
// - The order of result = 1 and wrote = true among the arguments of the write calls.
// Primitive 2. Reads two operands and compares or changes the left one. Motives are floats
// underneath, so where the left operand has no s16 to write through it uses the float.
long cXObject::TryExpression(ExpressionParam *param)
{
    s16 rightValue;
    u16 leftValue;
    const s16 *leftRef;
    f32 *leftFloat;
    s16 *unused = 0;
    long result = -1;
    bool wrote = false;

    if (param->mRightScope != 7) {
        if (InterpValue(param->mRightScope, param->mRight, 0, 0, &rightValue, 0) == -1)
            return -1;
    } else {
        int right = param->mRight;
        rightValue = right;
    }

    if (param->mLeftScope != 7) {
        if (InterpValue(param->mLeftScope, param->mLeft, &leftRef, &leftFloat, (s16 *)&leftValue, &unused) == -1)
            return -1;
    } else {
        int left = param->mLeft;
        leftValue = left;
        leftRef = 0;
        leftFloat = 0;
    }

    switch (param->mOperator) {
    case 0x11:
        // --left, then left > right.
        if (leftRef) {
            InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef - 1);
            leftValue = *leftRef;
        } else {
            OBJECT_CHECK(leftFloat, 0x13);
            *leftFloat -= 1.0f;
            leftValue = *leftRef;
        }
        wrote = true;
        result = (s16)leftValue > rightValue;
        break;
    case 0x0e:
        result = (s16)leftValue >= rightValue;
        break;
    case 0x0f:
        result = (s16)leftValue <= rightValue;
        break;
    case 0x10:
        result = 1;
        if ((s16)leftValue == rightValue)
            result = 0;
        break;
    case 0x0d:
        OBJECT_CHECK(leftRef, 0x13);
        result = 1;
        wrote = true;
        InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef & rightValue);
        break;
    case 0x12:
        OBJECT_CHECK(leftRef, 0x13);
        result = 1;
        wrote = true;
        InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef | rightValue);
        break;
    case 0x0c:
        // left %= right, with both made positive first.
        OBJECT_CHECK(leftRef, 0x13);
        OBJECT_CHECK(rightValue != 0, 0x26);
        if (*leftRef < 0)
            InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, -*leftRef);
        if (rightValue < 0)
            rightValue = -rightValue;
        result = 1;
        wrote = true;
        InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef % rightValue);
        break;
    case 0x0b:
        // ++left, then left < right.
        if (leftRef) {
            InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef + 1);
            leftValue = *leftRef;
        } else {
            OBJECT_CHECK(leftFloat, 0x13);
            *leftFloat += 1.0f;
            leftValue = *leftRef;
        }
        wrote = true;
        result = (s16)leftValue < rightValue;
        break;
    case 0x01:
        result = (s16)leftValue < rightValue;
        break;
    case 0x00:
        result = (s16)leftValue > rightValue;
        break;
    case 0x02:
        result = 1;
        if ((s16)leftValue != rightValue)
            result = 0;
        break;
    case 0x05:
        if (leftRef) {
            InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, rightValue);
        } else {
            OBJECT_CHECK(leftFloat, 0x13);
            *leftFloat = rightValue;
        }
        result = 1;
        wrote = true;
        break;
    case 0x03:
        if (leftRef) {
            InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef + rightValue);
        } else {
            OBJECT_CHECK(leftFloat, 0x13);
            *leftFloat += rightValue;
        }
        result = 1;
        wrote = true;
        break;
    case 0x04:
        if (leftRef) {
            InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef - rightValue);
        } else {
            OBJECT_CHECK(leftFloat, 0x13);
            *leftFloat -= rightValue;
        }
        result = 1;
        wrote = true;
        break;
    case 0x06:
        if (leftRef) {
            InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef * rightValue);
        } else {
            OBJECT_CHECK(leftFloat, 0x13);
            *leftFloat *= rightValue;
        }
        result = 1;
        wrote = true;
        break;
    case 0x07:
        OBJECT_CHECK(rightValue != 0, 0x26);
        if (leftRef) {
            InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef / rightValue);
        } else {
            OBJECT_CHECK(leftFloat, 0x13);
            *leftFloat /= rightValue;
        }
        result = 1;
        wrote = true;
        break;
    case 0x09: {
        // Set bit right of left. Bits count from 1.
        OBJECT_CHECK(rightValue < 0x11, 13);
        OBJECT_CHECK(leftRef, 0x13);
        wrote = true;
        s16 before = *leftRef;
        int mask = 1 << (rightValue - 1);
        InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef | mask);
        if (param->mLeft == 8)
            OnEdithSetFlagField1(mask, (before & mask) != 0, (*leftRef & mask) != 0);
        result = 1;
        wrote = true;
        break;
    }
    case 0x0a: {
        // Clear bit right of left. This one doesn't count as a write below.
        OBJECT_CHECK(rightValue < 0x11, 13);
        OBJECT_CHECK(leftRef, 0x13);
        s16 before = *leftRef;
        int mask = 1 << (rightValue - 1);
        InterpWriteValue(param->mLeftScope, param->mLeft, &leftRef, *leftRef & ~mask);
        if (param->mLeft == 8)
            OnEdithSetFlagField1(mask, (before & mask) != 0, (*leftRef & mask) != 0);
        result = 1;
        break;
    }
    case 0x08:
        OBJECT_CHECK(rightValue < 0x11, 13);
        result = ((s16)leftValue >> (rightValue - 1)) & 1;
        break;
    default:
        TREE_ERROR(1);
        return -1;
    }

    if (wrote) {
        // Motives stay between -100 and 100.
        switch (param->mLeftScope) {
        case 0x0e:
        case 0x0f:
        case 0x11:
            if (leftFloat) {
                if (*leftFloat < -100.0f)
                    *leftFloat = -100.0f;
                else if (*leftFloat > 100.0f)
                    *leftFloat = 100.0f;
            }
            break;
        }
        if (EdithVariableSet::s_bTaggedGraphicField == 1) {
            if (param->mLeftScope == 4)
                GetObjectFromID(GetCurElem()->mStackObject)->TagGraphicsField();
            else if (param->mLeftScope == 3)
                TagGraphicsField();
            EdithVariableSet::s_bTaggedGraphicField = 0;
        }
    }
    return result;
}
#endif
