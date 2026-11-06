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

class FTilePt;
class FTileRect;
class ObjSelector;
class ReconBuffer;
class RelMatrix;

namespace Weather {
class DamageEvent;
class Armor;
}

// Wii: ObjectSlot. Slot 0 is the object's own; the slots a tree sees start at 1.
class ObjectSlot {
public:
    u32 field_0x0;       // 0x00
    s16 mObjectID;       // 0x04
    u8 pad_0x6[0x1e];    // 0x06
};
STATIC_ASSERT(ObjectSlot_size, sizeof(ObjectSlot) == 0x24);

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
    u32 field_0xb8;            // 0xb8
    RelMatrix *mRelMatrix;     // 0xbc
    s16 mID;                   // 0xc0
    u8 mLocation[8];           // 0xc4  an FTilePt
    f32 field_0xcc;            // 0xcc
    u8 mRect[0x10];            // 0xd0  an FTileRect
    u32 field_0xe0;            // 0xe0
    u32 field_0xe4;            // 0xe4
    s16 *mDef;                 // 0xe8  a word of it is scope 0x15; word 9 is the object's type
    ObjSelector *mSelector;    // 0xec
    eastl::vector<ObjectSlot, eastl::allocator> mObjectSlots; // 0xf0
    u8 mRoutingSlots[0x10];    // 0x100 eastl::vector<RoutingSlot>
    u32 mSpriteSlots;          // 0x110 eastl::vector<SpriteSlot>, its first word
    u8 pad_0x114[0xc];         // 0x114
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
    s16 GetType() const { return mDef[9]; }
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
    int InterpValue(s16 scope, s16 data, const s16 **ref, f32 **floatRef, s16 *value, s16 **a);
    int InterpWriteValue(s16 scope, s16 data, const s16 **ref, s16 value);
};

#endif
