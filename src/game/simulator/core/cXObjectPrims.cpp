#include "game/simulator/core/cXObject.h"

#include "common/EGlobal.h"
#include "game/animation/SAnimator2.h"

#include "game/simulator/core/Lot.h"
#include "game/simulator/core2/cXPerson.h"
#include "game/simulator/objectdata/XObjLang.h"

#define OBJECT_ERROR_CHECK(condition, code) TREE_CHECK(this, condition, code, return -1)

namespace InteractorModule {
class DirectInteractor {
public:
    u32 pad_0x0[0x23];
    int mAutonomous; // 0x8c  Wii: IsAutonomous
};

class InteractorManager {
public:
    static InteractorManager *GetSingleton();
    DirectInteractor *GetPlayerInteractor(int a, int type);
};
}

// Primitive 0xf, TreeBreak.
struct TreeBreakParam {
    s16 mData;  // 0x00
    s16 mScope; // 0x02
    u16 mFlags; // 0x04  bit 0 log only, bit 1 stop
};

// A class in memory, not a plain s16: the game keeps it in the slot FindGoodLocation's tile uses.
class TreeBreakValue {
public:
    s16 mValue;

    TreeBreakValue() {}
    TreeBreakValue(const TreeBreakValue &other);
};

// Primitive 0: wait. The counter is a parameter of this tree, numbered by the first operand.
struct IdleParam {
    s16 mParam; // 0x00
};

long cXObject::TryIdle(StackElem *elem, XPrimParam *primParam)
{
    IdleParam *param = (IdleParam *)primParam;
    TREE_CHECK(this, param->mParam >= 0 && param->mParam < elem->mNumParams, 8, return -1);
    s16 *params = elem->GetParams();
    s16 *counter = &params[param->mParam];
    if (GetIdleStatus() == 0) {
        *counter = 0;
        SetIdleStatus(-1);
    }
    if (*counter < 0)
        *counter = 0;
    if (*counter == 0)
        return 1;
    if (!AllowIdleOptimization()) {
        --*counter;
        return 2;
    }
    SetIdleStatus(*counter - 1);
    return 2;
}

// Wii: GetNextRandomNumber, and SRand, which the PS2 build inlines.
u32 GetNextRandomNumber();

inline u32 SRand(u32 limit)
{
    if (limit != 0)
        return GetNextRandomNumber() % limit;
    return 0;
}

// Primitive 8: a random number below a limit, into an operand.
struct RandomParam {
    s16 mData;       // 0x00
    u16 mScope;      // 0x02
    u16 mLimitData;  // 0x04
    u16 mLimitScope; // 0x06
};

long cXObject::TryRandom(StackElem *elem, XPrimParam *primParam)
{
    RandomParam param = *(RandomParam *)primParam;
    const s16 *ref;
    f32 *floatRef;
    s16 limit;

    if (InterpValue((s16)param.mScope, param.mData, &ref, &floatRef, 0, 0) == -1)
        return -1;
    if (InterpValue((s16)param.mLimitScope, (s16)param.mLimitData, 0, 0, &limit, 0) == -1)
        return -1;
    TREE_CHECK(this, limit, 4, return -1);
    if (ref) {
        // Read as a u16 and narrowed, which is why the game loads the limit a second time here.
        InterpWriteValue((s16)param.mScope, param.mData, &ref, SRand((s16)*(u16 *)&limit));
        return 1;
    }
    if (floatRef) {
        *floatRef = (int)SRand(limit);
    } else {
        TREE_ERROR(0x13);
        return -1;
    }
    return 1;
}

#ifdef NON_MATCHING
// The logic is complete, and 131 of the 189 words line up: the gotos give the game's shared
// return blocks, but the hoisted locals change registers.
// Primitive 0x35: questions about the object in an operand.
struct CheckObjectParam {
    s16 mData;  // 0x00
    s16 mScope; // 0x02
    s16 mTest;  // 0x04
};

long cXObject::TryCheckObject(StackElem *elem, XPrimParam *primParam)
{
    CheckObjectParam *param = (CheckObjectParam *)primParam;
    s16 id = -1;
    int value;
    void *found;
    cXObject *object;
    cXPerson *person;
    InteractorModule::InteractorManager *manager;
    InteractorModule::DirectInteractor *interactor;

    if (InterpValue(param->mScope, param->mData, 0, 0, &id, 0) == -1)
        return -1;
    switch (param->mTest) {
    case 0: {
        // The player's Sim, under direct control.
        object = GetObjectFromID(id);
        if (!object)
            return 0;
        person = object->GetPerson();
        if (!person)
            return 0;
        manager = InteractorModule::InteractorManager::GetSingleton();
        if (_globals.mPlayerPerson != person)
            return 0;
        interactor = manager->GetPlayerInteractor(0, 0);
        if (!interactor)
            return 0;
        value = interactor->mAutonomous;
        goto isZero;
    }
    case 1:
        found = GetObjectFromID(id);
        goto isNonZero;
    case 2: {
        object = GetObjectFromID(id);
        if (!object)
            return 0;
        value = object->GetType() ^ 2;
    isZero:
        if (value)
            return 0;
        return 1;
    }
    case 3:
        found = GetNeighborhood()->FindNeighborByIDInline(id);
    isNonZero:
        if (found)
            return 1;
        break;
    case 4: {
        // How many relationships of the type in temp 0 the Sim has, back into temp 0.
        s16 count = 0;
        OBJECT_ERROR_CHECK(mTemps[0] > 0 && mTemps[0] < 10, 0x14);
        s16 type = mTemps[0];
        object = GetObjectFromID(id);
        if (object) {
            person = object->GetPerson();
            if (person)
                count = GetNeighborhood()->FindNeighborByIDInline(person->GetNeighborID())->CountRelationshipsOfType(type, false);
        }
        mTemps[0] = count;
        return 1;
    }
    case 5: {
        // The player's Sim with no interactor.
        object = GetObjectFromID(id);
        if (!object)
            return 0;
        person = object->GetPerson();
        if (!person)
            return 0;
        manager = InteractorModule::InteractorManager::GetSingleton();
        if (_globals.mPlayerPerson != person)
            return 0;
        if (manager->GetPlayerInteractor(0, 0) == 0)
            return 1;
        return 0;
    }
    }
    return 0;
}
#endif

#ifdef NON_MATCHING
// The logic is all here, and 69 of the 121 words line up. Still different: the registers of the
// selector and the hash lookup's variables are one along from the game's, and the compare in
// the bit 2 case branches the other way round.
// Primitive 0x20: is the object in an operand of a given type?
struct TestObjectTypeParam {
    int mGUID;  // 0x00
    s16 mData;  // 0x04
    u8 mScope;  // 0x06
    u8 mFlags;  // 0x07  bit 0 same semi-global group, bit 1 store the GUID, bit 2 GUID from temps
};

long cXObject::TryTestObjectType(StackElem *elem, XPrimParam *primParam)
{
    TestObjectTypeParam *param = (TestObjectTypeParam *)primParam;
    s16 id;
    int guid = param->mGUID;

    if (InterpValue(param->mScope, param->mData, 0, 0, &id, 0) == -1)
        return -1;
    cXObject *object = GetObjectFromID(id);
    TREE_CHECK(this, object, 0x17, return -1);
    ObjSelector *selector = object->mSelector;
    if (!selector)
        return 0;
    u8 flags = param->mFlags;
    if (flags & 4) {
        int wanted = (mTemps[1] << 16) | (u16)mTemps[2];
        if (selector->GetGUID() != wanted)
            return 0;
        goto matched;
    }
    if (flags & 1) {
        ObjSelector *other = GetObjectFolder()->GetSelectorByGUID(guid);
        if (other && selector->mSemiNamespaceID == other->mSemiNamespaceID)
            return 1;
        return 0;
    }
    if (flags & 2) {
        mTemps[2] = guid;
        mTemps[1] = guid >> 16;
    matched:
        return 1;
    }
    if (selector->GetGUID() == guid)
        return 1;
    if (selector->GetMasterSelector()->GetGUID() == guid)
        return 1;
    return 0;
}
#endif

#ifdef NON_MATCHING
// 69 of the 70 words line up. The game has one more: a nop that aligns the label after the
// behaviour test to eight bytes. A do/while around the lookup or the range test doesn't give it,
// and one around the whole test changes the registers.
class AnimRef;
class PassiveInfluenceSystem {
public:
    void SetSkillNameID(cXObject *object, const AnimRef *anim);
};

// Wii: resolveSkillForPassiveInfluence.
long resolveSkillForPassiveInfluence(cXObject *object, StackElem *elem, const AnimateNewParam *param,
                                     const AnimRef *&anim, bool &a);

// Primitive 0x2c on an object that isn't a Sim. A Sim's animation is SAnimator2::TryAnimate;
// this only picks me or the stack object and records the skill for passive influence.
long cXObject::TryAnimate(StackElem *elem, const AnimateNewParam *param)
{
    const AnimRef *anim = 0;
    int behavior = param->GetBehavior();
    cXObject *object = 0;
    if (behavior == 7) {
        object = this;
    } else if (behavior == 8) {
        object = GetObjectModule()->GetObjectFromIDInline(elem->mStackObject);
    } else {
        TREE_ERROR(5);
        return -1;
    }
    bool skill = 0;
    long result = resolveSkillForPassiveInfluence(object, elem, param, anim, skill);
    if (object)
        GetSimulator()->mPassiveInfluence->SetSkillNameID(object, anim);
    return result;
}
#endif

// Primitive 0x3d. Wii keeps this as a function; PS2 inlines it.
inline void cXObject::TryParticleEffect(ParticleEffectParam effect, long &result)
{
    cXPerson *person = GetPerson();
    TREE_CHECK(this, person, 0x1c, result = -1; return);
    int played = person->mAnimator->PlayEffect(effect.field_0x0, effect.field_0x4);
    result = 1;
    if (played != 1)
        result = 0;
}

// The switch over primitive numbers. Cases are in the order of the game's code; the source order
// sets the order of the blocks, and the numbers show which are together. cXPerson::TryElement
// handles the Sim-only primitives and passes the others here.
long cXObject::TryElement(StackElem *elem, s16 opcode, XPrimParam *param)
{
    long result;

    switch (opcode) {
    case 0x20:
        result = TryTestObjectType(elem, param);
        break;
    case 0x13:
        result = TryIsPersonInArea(elem, param);
        break;
    case 0x10: {
        FTilePt tile;
        s16 slot;
        TFGL_ACTIONS action;
        int found;
        result = TryFindGoodLocation(elem, param, &action, &tile, &found, &slot);
        break;
    }
    case 0x1a:
        result = TryRelationship2(elem, param);
        break;
    case 0x29:
        result = TrySetBalloon(elem, param);
        break;
    case 0x1c:
        result = TryCallNamedTree(elem, param);
        break;
    case 0x32:
        result = TryMakeActionString(elem, param);
        break;
    case 0x01:
        result = TryGenericSimCall(elem, param);
        break;
    case 0x24:
        result = TryDialog(elem, param, false);
        break;
    case 0x2e:
        result = TrySnap(elem, param);
        break;
    case 0x31:
        result = TryNotifyStackObject(elem, param);
        break;
    case 0x0e:
        result = TryFindFunctionalObject(elem, param);
        break;
    case 0x14:
        result = TryCallFunctionalTree(elem, param);
        break;
    case 0x0d:
        result = TryPushAction(elem, param);
        break;
    case 0x2b:
        result = TryDropOnto(elem, param);
        break;
    case 0x48:
        result = TrySnapObject(elem, param);
        break;
    case 0x19:
        result = TryBudget(elem, param);
        break;
    case 0x18:
        result = TryRelationship(elem, param);
        break;
    case 0x2a:
        result = TryCreateObject(elem, param);
        break;
    case 0x33:
        result = TryPreloadObject(elem, param);
        break;
    case 0x12: {
        s16 killed = -1;
        result = TryKillObject(elem, param, &killed);
        break;
    }
    case 0x21:
        result = TryFind5WorstMotives(elem, param);
        break;
    case 0x02:
        result = TryExpression((ExpressionParam *)param);
        break;
    case 0x00:
        result = TryIdle(elem, param);
        break;
    case 0x07:
        result = TryUpdate(elem, param);
        break;
    case 0x05:
        result = TryDrop();
        break;
    case 0x04:
        result = TryGrab(elem, param);
        break;
    case 0x0f: {
        // Stops the tree if the value is non-zero, unless bit 0 asks for logging only.
        TreeBreakValue value;
        TreeBreakParam *breakParam = (TreeBreakParam *)param;
        if (InterpValue(breakParam->mScope, breakParam->mData, 0, 0, &value.mValue, 0) == -1) {
            result = -1;
            break;
        }
        if (value.mValue == 0) {
            result = 1;
            break;
        }
        if (breakParam->mFlags & 1) {
            result = 1;
            break;
        }
        if ((breakParam->mFlags >> 1) & 1) {
            g_bTreeAssert = 1;
            SetError(0x3e);
            Error(0x3e);
            result = -1;
            break;
        }
        result = 1;
        break;
    }
    case 0x08:
        result = TryRandom(elem, param);
        break;
    case 0x0b:
        result = TryDistanceTo(elem, param);
        break;
    case 0x0c:
        result = TryDirectionTo(elem, param);
        break;
    case 0x17:
        result = TryPlaySound(elem, (PlaySoundParam *)param);
        break;
    case 0x30:
        result = TryKillSounds(elem, (KillSoundsParam *)param);
        break;
    case 0x1f:
        result = TrySetToNext(elem, param);
        break;
    case 0x09:
        result = TryBurn(elem, param);
        break;
    case 0x0a:
    {
        // Claims or releases the tutorial for this object.
        bool ok;
        switch (((TutorialParam *)param)->mMode) {
        case 0:
            ok = mModule->SetTutorialObject(this);
            break;
        case 1:
            if (this == mModule->GetTutorialObject())
                ok = mModule->ClearTutorialObject();
            else
                ok = false;
            break;
        default:
            TREE_ERROR_LOOP(0x36);
            result = -1;
            goto finished;
        }
        result = ok;
        break;
    }
    case 0x23:
        result = TryUserEvent(elem, param);
        break;
    case 0x34:
        result = TryRoomEffect(elem, param);
        break;
    case 0x35:
        result = TryCheckObject(elem, param);
        break;
    case 0x37:
        result = TryAnimateObject(elem, param);
        break;
    case 0x38:
        result = TryCompleteGoal(elem, *(CompleteGoalParam *)param);
        break;
    case 0x27:
        result = TryListenForInput(elem, param);
        break;
    case 0x28:
        result = TryCameraControl(elem, param);
        break;
    case 0x39:
        result = TryHUDCreate(elem, param);
        break;
    case 0x3a:
        result = TryHUDAddItem(elem, param);
        break;
    case 0x3b:
        result = TryHUDManage(elem, param);
        break;
    case 0x3c:
        result = TryHUDData(elem, param);
        break;
    case 0x3d:
        TryParticleEffect(*(ParticleEffectParam *)param, result);
        break;
    case 0x3f:
        result = TryMemory(elem, param);
        break;
    case 0x41:
        result = TryPassiveInfluence(elem, param);
        break;
    case 0x1d:
        result = TrySetMotiveDelta(elem, param);
        break;
    case 0x2c:
        result = TryAnimate(elem, (const AnimateNewParam *)param);
        break;
    case 0x42:
    case 0x43:
        result = 1;
        break;
    case 0x44:
        result = TryCustomUI(elem, param);
        break;
    case 0x45:
        result = TryLotTransition(elem, param);
        break;
    case 0x46:
        result = TryFamilyInventory(elem, param);
        break;
    case 0x15:
        result = TrySyncAudio(elem, param);
        break;
    case 0x2f:
        result = TryNotifyPlans(elem, param);
        break;
    case 0x36:
        result = TryManageClothing(elem, param);
        break;
    default:
        TREE_ERROR(5);
        return -1;
    }
finished:
    return result;
}
