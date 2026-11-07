#ifndef GAME_SIMULATOR_CORE_CXOBJECT_H
#define GAME_SIMULATOR_CORE_CXOBJECT_H

// The vtable stays an external symbol, so the rebuild links to the game's copy at 0x4BC630.
#pragma interface

#include "common/eastl.h"
#include "common/types.h"
#include "game/simulator/core/EdithVariableSet.h"
#include "game/simulator/core/Neighborhood.h"
#include "game/simulator/core/ObjectModule.h"
#include "game/simulator/core2/TreeSim.h"

class FTileRect;
class AnimateNewParam;
class CompleteGoalParam;
class KillSoundsParam;
class PlaySoundParam;
class ObjSelector;
class ReconBuffer;
class RelMatrix;

namespace Weather {
class DamageEvent;
class Armor;
}

// Wii: FTilePt, a tile position, two FInts. The real class is probably 8 bytes: it is declared
// as 4 here because only pointers to it are used, and at 4 the local in TryElement leaves the
// stack slot sharing as it is in the game. The copy constructor keeps it in memory.
class CTilePt;
class FTilePt {
public:
    u32 field_0x0;

    FTilePt() {}
    FTilePt(const FTilePt &other);
};

enum TFGL_ACTIONS { };

// The operands of primitive 2, Expression.
struct ExpressionParam {
    s16 mLeft;      // 0x00
    s16 mRight;     // 0x02
    u8 field_0x4;   // 0x04  no code reads it
    s8 mOperator;   // 0x05
    s8 mLeftScope;  // 0x06
    s8 mRightScope; // 0x07
};

// Primitive 0xa, Tutorial.
struct TutorialParam {
    u8 mMode; // 0x00  0 claim, 1 release
};

// The operands of primitive 0x3d, ParticleEffect, passed along whole.
struct ParticleEffectParam {
    u32 field_0x0;
    u32 field_0x4;
};

// Wii: SpriteSlot, 0x6c bytes. IsActive reads the word at +0x10.
class SpriteSlot {
public:
    u32 pad_0x0[4];  // 0x00
    void *mSprite;   // 0x10
    u32 pad_0x14[0x16]; // 0x14

    bool IsActive() const { return mSprite != 0; }
};
STATIC_ASSERT(SpriteSlot_size, sizeof(SpriteSlot) == 0x6c);

// Wii: ObjectSlot. Slot 0 is the object's own; the slots a tree sees start at 1.
class ObjectSlot {
public:
    u32 field_0x0;       // 0x00
    s16 mObjectID;       // 0x04
    u8 pad_0x6[0x1e];    // 0x06
};
STATIC_ASSERT(ObjectSlot_size, sizeof(ObjectSlot) == 0x24);

// Wii: RoutingSlot, a place to stand, 0x3c bytes.
class RoutingSlot {
public:
    u32 pad_0x0[7];  // 0x00
    int mFlags;      // 0x1c  bit 14 failure trees may run
    u32 pad_0x20[7]; // 0x20

    void SetAllowFailureTrees(bool allow)
    {
        mFlags &= ~0x4000;
        if (allow)
            mFlags |= 0x4000;
    }
};
STATIC_ASSERT(RoutingSlot_size, sizeof(RoutingSlot) == 0x3c);

// The base of everything in the world that runs trees. The layout comes from the Wii
// constructor and the PS2 destructor: past TreeSim, a PS2 offset is the Wii offset plus 4,
// because PS2 keeps TreeSim's vptr after its fields and Wii keeps it among them. Fields with
// a field_ name have a known offset and an unknown meaning.
class cXObject : public TreeSim {
public:
    s16 *mAttrs;               // 0x54  scope 0 and 1
    int mNumAttrs;             // 0x58
    s16 *mSemiAttrs;           // 0x5c  scope 2 and 5
    int mNumSemiAttrs;         // 0x60
    s16 *mStaticAttrs;         // 0x64  scope 0x40 and 0x41
    int mNumStaticAttrs;       // 0x68
    s16 *mIndexedData;         // 0x6c  scope 0x1b; the destructor frees it
    s16 mNumIndexedData;       // 0x70
    s16 mTemps[8];             // 0x72  scope 8
    EdithVariableSet mVars;    // 0x84  scope 3 and 4
    ObjectModule *mModule;     // 0xb4
    cXObject *mNext;           // 0xb8  the next object in ObjectModule's list
    RelMatrix *mRelMatrix;     // 0xbc
    s16 mID;                   // 0xc0
    int mTileX;                // 0xc4  an FTilePt: fixed point, four bits of fraction
    int mTileY;                // 0xc8
    f32 field_0xcc;            // 0xcc
    u8 mRect[0x10];            // 0xd0  an FTileRect
    int mLevel;                // 0xe0
    u32 field_0xe4;            // 0xe4
    s16 *mDef;                 // 0xe8  a word of it is scope 0x15; word 9 is the object's type
    ObjSelector *mSelector;    // 0xec
    eastl::vector<ObjectSlot, eastl::allocator> mObjectSlots; // 0xf0
    eastl::vector<RoutingSlot, eastl::allocator> mRoutingSlots; // 0x100
    eastl::vector<SpriteSlot, eastl::allocator> mSpriteSlots; // 0x110
    u8 field_0x120;            // 0x120
    u32 field_0x124;           // 0x124
    f32 field_0x128;           // 0x128
    u8 field_0x12c;            // 0x12c
    u32 field_0x130;           // 0x130 the frame count when the object was made
    s32 mGraphicsTagged;       // 0x134
    u32 field_0x138;           // 0x138

    // Virtual functions after TreeSim's, in the order of the Wii vtable.
    virtual ~cXObject();
    virtual long TryElement(StackElem *elem, s16 opcode, XPrimParam *param);
    virtual void Error(s16 code);
    virtual int HandleBreakpoint(StackElem *elem, BehaviorNode *node);
    virtual int Simulate(int a);
    virtual u32 GetSpriteSlot();
    virtual u32 GetSpriteSlot(u32 index);
    virtual void TransitionToNewHouse(s16 a, s16 b, s16 c);
    virtual void Cleanup(cXObject *object);
    virtual int GosubObjectTree(cXObject *object, s16 *params, s16 treeID, bool a);
    virtual void Initialize();
    virtual void Reset(bool a);
    virtual void PostLoad(int a, Globs::TransitionType type);
    virtual void PreSave();
    virtual void ApplyDamage(const Weather::DamageEvent &event, s16 a);
    virtual void CalcArmorAgainstDamage(const Weather::DamageEvent &event, Weather::Armor &armor);
    virtual void Turn(int a);
    virtual void Pickup();
    virtual int CanPlace(const FTilePt &tile, int a, cXObject *object, int b);
    virtual int Place(const FTilePt &tile, int a, cXObject *object, int b);
    virtual int IsPartOfMe(cXObject *object);
    virtual int UserCanPlace(const FTilePt &tile, int a, cXObject *object, int b);
    virtual int UserPlace(const FTilePt &tile, int a, cXObject *object, int b);
    virtual int UserCanPickup();
    virtual int UserPickup(bool a);
    virtual int UserCanDelete();
    virtual void ComputeRect(const FTilePt &tile, FTileRect *rect);
    virtual void ForceLocation();
    virtual int ContainsData(int index);
    virtual u8 GetColorIndex() const;
    virtual void SetColorIndex(u8 index);
    virtual int IsInteractionTile(bool a);
    virtual void GetNumTiles(int &a, int &b);
    virtual int HaveGraphicsBeenTagged();
    virtual void TagGraphicsField();
    virtual void ReconStream(ReconBuffer *buffer, int a, bool b);
    virtual u32 ReconType();

    void HandleError();
    int GetIdleStatus() { return mModule->GetIdleStatus(mID); }
    void SetIdleStatus(int status) { mModule->SetIdleStatus(mID, status); }
    inline bool AllowIdleOptimization();
    s16 GetType() const { return mDef[9]; }
    bool IsPerson() const { return GetType() == 2; }
    bool IsMultiTile() const { return mDef[0xa] != 0; }
    bool IsInWorld() const
    {
        if (mTileX >> 4 < 1 || mTileY >> 4 < 1)
            return false;
        return true;
    }
    cXObject *GetNext() { return mNext; }
    s16 GetID() const { return mID; }
    u16 GetRoom() const { return mVars[0x1d]; }
    inline CTilePt GetCTilePt() const;
    int GetNumRoutingSlots() { return mRoutingSlots.size(); }
    RoutingSlot *GetRoutingSlot(int index) { return &mRoutingSlots[index]; }
    int CountObjectSlots() { return mObjectSlots.size() - 1; }
    ObjectSlot *GetObjectSlot(int index)
    {
        if (index < 0 || index >= CountObjectSlots())
            return 0;
        return &mObjectSlots[index + 1];
    }
    cXObject *GetObjectFromID(s16 id) { return mModule->GetObjectFromID(id); }

    // Reads and writes one operand of an instruction: bhav.md lists the scopes. InterpValue
    // returns 1, or -1 once it has stopped the tree with an error.
    long InterpValue(s16 scope, s16 data, const s16 **ref, f32 **floatRef, s16 *value, s16 **a);
    int InterpWriteValue(s16 scope, s16 data, const s16 **ref, s16 value);
    long TryExpression(ExpressionParam *param);
    inline void TryParticleEffect(ParticleEffectParam effect, long &result);

    // The primitives. They return what TryElement does: see TreeSim::TryElement.
    long TryTestObjectType(StackElem *, XPrimParam *);
    long TryIsPersonInArea(StackElem *, XPrimParam *);
    long TryFindGoodLocation(StackElem *, XPrimParam *, TFGL_ACTIONS *, FTilePt *, int *, s16 *);
    long TryRelationship2(StackElem *, XPrimParam *);
    long TrySetBalloon(StackElem *, XPrimParam *);
    long TryCallNamedTree(StackElem *, XPrimParam *);
    long TryMakeActionString(StackElem *, XPrimParam *);
    long TryGenericSimCall(StackElem *, XPrimParam *);
    long TryDialog(StackElem *, XPrimParam *, bool);
    long TrySnap(StackElem *, XPrimParam *);
    long TryNotifyStackObject(StackElem *, XPrimParam *);
    long TryFindFunctionalObject(StackElem *, XPrimParam *);
    long TryCallFunctionalTree(StackElem *, XPrimParam *);
    long TryPushAction(StackElem *, XPrimParam *);
    long TryDropOnto(StackElem *, XPrimParam *);
    long TrySnapObject(StackElem *, XPrimParam *);
    long TryBudget(StackElem *, XPrimParam *);
    long TryRelationship(StackElem *, XPrimParam *);
    long TryCreateObject(StackElem *, XPrimParam *);
    long TryPreloadObject(StackElem *, XPrimParam *);
    long TryKillObject(StackElem *, XPrimParam *, s16 *);
    long TryFind5WorstMotives(StackElem *, XPrimParam *);
    long TryIdle(StackElem *, XPrimParam *);
    long TryUpdate(StackElem *, XPrimParam *);
    long TryDrop();
    long TryGrab(StackElem *, XPrimParam *);
    long TryRandom(StackElem *, XPrimParam *);
    long TryDistanceTo(StackElem *, XPrimParam *);
    long TryDirectionTo(StackElem *, XPrimParam *);
    long TryPlaySound(StackElem *, PlaySoundParam *);
    long TryKillSounds(StackElem *, KillSoundsParam *);
    long TrySetToNext(StackElem *, XPrimParam *);
    long TryBurn(StackElem *, XPrimParam *);
    long TryUserEvent(StackElem *, XPrimParam *);
    long TryRoomEffect(StackElem *, XPrimParam *);
    long TryCheckObject(StackElem *, XPrimParam *);
    long TryAnimateObject(StackElem *, XPrimParam *);
    long TryCompleteGoal(StackElem *, CompleteGoalParam &);
    long TryListenForInput(StackElem *, XPrimParam *);
    long TryCameraControl(StackElem *, XPrimParam *);
    long TryHUDCreate(StackElem *, XPrimParam *);
    long TryHUDAddItem(StackElem *, XPrimParam *);
    long TryHUDManage(StackElem *, XPrimParam *);
    long TryHUDData(StackElem *, XPrimParam *);
    long TryMemory(StackElem *, XPrimParam *);
    long TryPassiveInfluence(StackElem *, XPrimParam *);
    long TrySetMotiveDelta(StackElem *, XPrimParam *);
    long TryAnimate(StackElem *, const AnimateNewParam *);
    long TryCustomUI(StackElem *, XPrimParam *);
    long TryLotTransition(StackElem *, XPrimParam *);
    long TryFamilyInventory(StackElem *, XPrimParam *);
    long TrySyncAudio(StackElem *, XPrimParam *);
    long TryNotifyPlans(StackElem *, XPrimParam *);
    long TryManageClothing(StackElem *, XPrimParam *);
};

// Whether this object can sleep through an Idle: it isn't a Sim, variable 0x19 is clear, and
// none of its sprites is active.
inline bool cXObject::AllowIdleOptimization()
{
    bool active = false;
    for (SpriteSlot *slot = mSpriteSlots.begin(); slot != mSpriteSlots.end(); ++slot) {
        if (slot->IsActive()) {
            active = true;
            break;
        }
    }
    if (GetType() != 2 && mVars[0x19] == 0 && !active)
        return true;
    return false;
}

#endif
