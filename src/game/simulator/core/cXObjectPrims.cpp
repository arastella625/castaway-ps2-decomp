#include "game/simulator/core/cXObject.h"

#include "game/simulator/core2/cXPerson.h"
#include "game/simulator/objectdata/XObjLang.h"

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

// Primitive 0x3d. Wii keeps this as a function; PS2 inlines it.
inline void cXObject::TryParticleEffect(ParticleEffectParam effect, long &result)
{
    cXPerson *person = GetPerson();
    TREE_CHECK(this, person, 0x1c, result = -1; return);
    int played = person->mSimInstance->PlayEffect(effect.field_0x0, effect.field_0x4);
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
