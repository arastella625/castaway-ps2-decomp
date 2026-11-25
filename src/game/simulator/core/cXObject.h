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
#include "game/Weather.h"

class FTileRect;
class AnimateNewParam;
class CompleteGoalParam;
class KillSoundsParam;
class PlaySoundParam;
class ObjSelector;
class ObjectFunctionTable;
class ReconBuffer;
class RelMatrix;


class CTilePt;

// The engine's frame counter, read by cXObject::Initialize.
extern u32 _framecount; // 0x49FDB8

// Wii: ObjEntryPoint, the index of a tree in an object's entry-point table. The first four
// are The Sims 2's function-table order; the names are ours, from the callers.
enum ObjEntryPoint {
    kObjEntryInit = 0,           // PostLoad, when the selector needs a reset
    kObjEntryMain = 1,           // the tree Cleanup and Reset restart the object's stack on
    kObjEntryCleanup = 3,        // Cleanup: having it replaces the pickup-and-reset
    kObjEntryPickup = 0xa,       // Pickup
    kObjEntryLoad = 2,           // PostLoad, with the transition type as the parameter
    kObjEntryUserPlacement = 0xb, // UserPlace
    kObjEntryUserPickup = 0xc,   // UserPickup
    kObjEntryAllowIntersection = 5, // TestIntersection runs it both ways; either may allow it
    kObjEntryWallAdjacency = 6,  // UpdateWallAdjacency: having it opts in, and it runs when
                                 // the walls around the object change
    kObjEntryPlacement = 9,      // TS2's placement entry; UpdateChairFacing runs it on a part
                                 // whose chair bit changed, if the part has tree 0x19
    kObjEntryChairFacing = 0x19, // UpdateChairFacing: only a part with this tree reruns 9
    kObjEntryChair = 0x1a,       // IsChair: an object with this tree counts as a chair
    kObjEntryPortal = 0xf,       // TS2's portal entry; IsDoor needs it
    kObjEntryOnAdd = 0x1e,       // the inventory
    kObjEntryOnRemove = 0x1f,
    kObjEntryApplyDamage = 0x20  // ApplyDamage, with the damage as the parameter
};

// Wii: FTilePt, a tile position: x and y, two FInts with four bits of fraction.
class FTilePt {
public:
    int mX; // 0x00
    int mY; // 0x04

    FTilePt() {}

    // Wii: FInt::Frac on each coordinate, the four fraction bits. 8 is the tile's centre.
    int FracX() const { return mX & 0xf; }
    int FracY() const { return mY & 0xf; }

    // Wii: Set. Each coordinate is a Wii FInt, the integer above 4 bits of fraction:
    // SetInteger keeps the fraction, then SetFrac clears it.
    // Wii: FInt::SetInteger and FInt::SetFrac, one coordinate at a time.
    static void SetInteger(int *value, int integer) { *value = (*value & 0xf) | (integer << 4); }
    static void SetFrac(int *value, int frac) { *value = (*value & ~0xf) | (frac & 0xf); }
    void Set(int x, int y)
    {
        SetInteger(&mX, x);
        SetFrac(&mX, 0);
        SetInteger(&mY, y);
        SetFrac(&mY, 0);
    }

    int SquareDist(const FTilePt &other) const
    {
        int dx = other.mX - mX;
        int dy = other.mY - mY;
        return dx * dx + dy * dy;
    }

    // Wii: GetApproximateDirection, the direction 0 to 7 from here to `to`, by eighths.
    int GetApproximateDirection(const FTilePt &to) const
    {
        int dx = to.mX - mX;
        int dy = to.mY - mY;
        int ax = dx > -1 ? dx : -dx;
        int ay = dy > -1 ? dy : -dy;
        int dir;
        if (ax * 2 < ay)
            dir = 4;
        else if (ay * 2 < ax)
            dir = 2;
        else
            dir = 3;
        if (dx < 0) {
            switch (dir) {
            case 2:
                dir = 6;
                break;
            case 3:
                dir = 5;
                break;
            }
        }
        if (dy < 0) {
            switch (dir) {
            case 4:
                dir = 0;
                break;
            case 3:
                dir = 1;
                break;
            case 5:
                dir = 7;
                break;
            }
        }
        return dir;
    }
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

// Primitive 0x1d: a change to a motive over time.
class SetMotiveDeltaParam {
public:
    u8 mDeltaScope; // 0x00
    u8 mLimitScope; // 0x01
    u8 mMotive;     // 0x02
    u8 mFlags;      // 0x03  bit 0 clear all, 1 influence me, 2 influence the stack object, 3 to 6 type
    s16 mDeltaData; // 0x04
    s16 mLimitData; // 0x06

    bool IsInfluence() const { return (mFlags & 6) != 0; }
    bool GetClearAll() const { return mFlags & 1; }
    bool IsInfluenceOnMe() const { return (mFlags >> 1) & 1; }
    bool IsInfluenceOnStackObject() const { return (mFlags >> 2) & 1; }
    s8 GetFlagsInTypedMotiveIncFormat() const { return (mFlags & 0x78) >> 3; }
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

    void Tick(); // 0x1C0AD8
};
STATIC_ASSERT(SpriteSlot_size, sizeof(SpriteSlot) == 0x6c);

// Wii: FTileRect, the footprint ComputeRect fills. SetWhole's four arguments land at 0xc,
// 0x8, 0x4 and 0x0 in that order.
class FTileRect {
public:
    int mMaxY; // 0x00
    int mMaxX; // 0x04
    int mMinY; // 0x08
    int mMinX; // 0x0c

    void SetWhole(int minX, int minY, int maxX, int maxY)
    {
        mMinX = minX;
        mMinY = minY;
        mMaxX = maxX;
        mMaxY = maxY;
    }

    // Wii: Sect, whether two footprints overlap; touching edges do not count.
    int Sect(const FTileRect &other) const
    {
        if (mMinY >= other.mMaxY)
            return 0;
        if (mMaxY <= other.mMinY)
            return 0;
        if (mMinX >= other.mMaxX)
            return 0;
        return mMaxX > other.mMinX;
    }
};

// Wii: SectWall, whether a footprint crosses a wall on a level.
int SectWall(FTileRect *rect, int level); // 0x1C3908

class SlotDescriptor;
// Wii: Slot. PS2 has its vtable at 0x4B8020, after RoutingSlot, SpriteSlot and ObjectSlot.
class Slot {
public:
    virtual ~Slot() {}
    virtual void WriteSlot(SlotDescriptor *descriptor) const;
};

// Wii: ObjectSlot. Slot 0 is the object's own; the slots a tree sees start at 1. The
// layout is the Wii constructor's: cXObject::Initialize copies one into mObjectSlots, and
// the copy skips +6 (so it is padding, not a member) and moves 0x18 to 0x20 through the
// float registers. Its vtable is at 0x4B8000, Slot's at 0x4B8020.
class ObjectSlot : public Slot {
public:
    s16 mObjectID;       // 0x04
    int mHeight;         // 0x08  Wii: GetHeight. UserCanPlace refuses 5 to 7 with error 0xB.
    int field_0xc;       // 0x0c  50 from the constructor
    int mFlags;          // 0x10  bit 17 a TS2 slot
    int field_0x14;      // 0x14
    f32 mXOffset;        // 0x18
    f32 mYOffset;        // 0x1c
    f32 field_0x20;      // 0x20

    // Wii: SetObjectID. Through an s16 parameter the ID is loaded with lh (HierSetSite).
    void SetObjectID(s16 id) { mObjectID = id; }

    ObjectSlot(); // 0x1E1888
    virtual ~ObjectSlot() {} // 0x1D8C88 is the vtable's copy; Initialize inlines this one
    virtual void WriteSlot(SlotDescriptor *descriptor) const;

    bool IsTS2ObjectSlot() const { return (mFlags >> 17) & 1; }
    int GetHeight() const { return mHeight; }
    f32 GetXOffset() const { return mXOffset; }
    f32 GetYOffset() const { return mYOffset; }
};
STATIC_ASSERT(ObjectSlot_size, sizeof(ObjectSlot) == 0x24);

// Wii: PlacementSpec, what TestPlace and Place take: the tile an object goes to and the slot
// of the object it goes into. Wii's first field is a byte; PS2 stores a word.
class cXObject;
class PlacementSpec {
public:
    int mValid;        // 0x00  1 from the four-argument constructor
    FTilePt mTile;     // 0x04  the tile given, or the holder's own location for a non-TS2 slot
    cXObject *mObject; // 0x0c  the object placed into, or 0
    int mSlot;         // 0x10  the slot in it, -1 without an object
    int mLevel;        // 0x14

    PlacementSpec(const FTilePt &tile, int level, cXObject *object, int slot); // 0x255978
    PlacementSpec(cXObject *object);                                           // 0x255938  where it is now
    // Wii: PlacementSpec(bool). PS2 ignores the argument and only clears mValid.
    PlacementSpec(bool a);                                                     // 0x255968
};
STATIC_ASSERT(PlacementSpec_size, sizeof(PlacementSpec) == 0x18);

// Wii: HierarchySite, where an object sits in the hierarchy: a PlacementSpec's five fields,
// but the slot counts from 1 (slot 0 is the holder's own), so one built from a spec adds 1
// to the slot unless it is -1. HierGetSite, HierSetSite and HierGetObject take one.
class HierarchySite {
public:
    int mValid;        // 0x00
    FTilePt mTile;     // 0x04
    cXObject *mObject; // 0x0c
    int mSlot;         // 0x10
    int mLevel;        // 0x14  the holder's, from the three-argument constructor

    HierarchySite(cXObject *object);                                // 0x223D10  where it is now
    HierarchySite(const PlacementSpec *spec);                       // 0x255A28
    HierarchySite(cXObject *object, const FTilePt &tile, int slot); // 0x255A78
};
STATIC_ASSERT(HierarchySite_size, sizeof(HierarchySite) == 0x18);

// A base holding only the vptr, which puts it at offset 0.

// Wii: RoutingSlot, a place to stand, 0x3c bytes.
// One slot as the data has it: bhav.md's Routing section lists the fields.
class SlotDescriptor {
public:
    s16 mType;         // 0x00  3 for a routing slot
    f32 mOffset[3];    // 0x04
    u32 pad_0x10[3];   // 0x10
    int mMultiplier[3]; // 0x1c
    int mFlags;        // 0x28
    int mSnapTarget;   // 0x2c
    int mMin;          // 0x30
    int mMax;          // 0x34
    int mOptimal;      // 0x38
    f32 mGradient;     // 0x3c
    int mFacing;       // 0x40
    int mResolution;   // 0x44
};

class RoutingSlot : public Slot {
public:
    // The copy constructor copies the offset as three floats and the multipliers in a loop, so
    // the offset is three members and the multipliers an array.
    f32 mOffsetX;       // 0x04
    f32 mOffsetY;       // 0x08
    f32 mOffsetZ;       // 0x0c
    int mMultiplier[3]; // 0x10  SetMultiplier
    int mFlags;       // 0x1c  low byte the directions, 14 failure trees, 15 different levels
    int mSnapTarget;  // 0x20
    int mMin;         // 0x24
    int mMax;         // 0x28
    int mOptimal;     // 0x2c
    f32 mGradient;    // 0x30
    int mFacing;      // 0x34  -2 towards the object, -3 any
    int mResolution;  // 0x38

    static const f32 kStraightOffset[];
    static const f32 kCornerOffset[];

    RoutingSlot();
    virtual ~RoutingSlot() {}
    int GetAllowFailureTrees() const { return (mFlags >> 14) & 1; }
    void SetAllowFailureTrees(bool allow)
    {
        mFlags &= ~0x4000;
        if (allow)
            mFlags |= 0x4000;
    }
    void SetAllowDifferentAlts(bool allow)
    {
        mFlags &= ~0x8000;
        if (allow)
            mFlags |= 0x8000;
    }
    void SetIsOnTopOfObject() { mFlags &= ~0xff; }
    int SnapsToDirection() const { return mFlags & 0x1000; }
    bool IsTS2RoutingSlot() const { return (mFlags >> 17) & 1; }
    bool GetIgnoreRooms() const
    {
        int flag = mFlags & 0x800;
        return flag != 0;
    }
    int GetSnapDirection() const
    {
        int i;
        for (i = 0; i < 7; i++) {
            if ((mFlags >> i) & 1)
                goto found;
        }
        i = 0;
    found:
        return i;
    }
    void AllowDirection(int direction);
    void SetTileDistances(f32 min, f32 max, f32 preferred);
    void SetFacingDirection(int direction);
    void SetDistances(int min, int max, int optimal);
    virtual void WriteSlot(SlotDescriptor *descriptor) const;
    f32 GetXOffset(int simType) const;
    f32 GetYOffset(int simType) const;
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
    s16 GetTemp0() const { return mTemps[0]; }
    EdithVariableSet mVars;    // 0x84  scope 3 and 4
    ObjectModule *mModule;     // 0xb4
    cXObject *mNext;           // 0xb8  the next object in ObjectModule's list
    RelMatrix *mRelMatrix;     // 0xbc
    s16 mID;                   // 0xc0
    int mTileX;                // 0xc4  an FTilePt, GetLocationRef: fixed point, four bits of fraction
    int mTileY;                // 0xc8
    f32 field_0xcc;            // 0xcc
    u8 mRect[0x10];            // 0xd0  an FTileRect
    int mLevel;                // 0xe0
    u32 mMiscFlags;            // 0xe4  Wii: SetMiscFlag; Error sets 0x100
    s16 *mDef;                 // 0xe8  a word of it is scope 0x15; word 9 is the object's type
    ObjSelector *mSelector;    // 0xec
    eastl::vector<ObjectSlot, eastl::allocator> mObjectSlots; // 0xf0
    eastl::vector<RoutingSlot, eastl::allocator> mRoutingSlots; // 0x100
    eastl::vector<SpriteSlot, eastl::allocator> mSpriteSlots; // 0x110
    u8 field_0x120;            // 0x120
    int field_0x124;           // 0x124  Initialize clears these three
    f32 field_0x128;           // 0x128
    int field_0x12c;           // 0x12c  a byte on Wii, a word on PS2 (Initialize stores sw)
    u32 mCreationFrame;        // 0x130  _framecount when Initialize ran
    s32 mGraphicsTagged;       // 0x134
    class CatalogResource *mCachedCatalogResource; // 0x138  Wii: GetCachedObjectCatalogResource

    // Virtual functions after TreeSim's, in the order of the Wii vtable.
    virtual ~cXObject();
    virtual long TryElement(StackElem *elem, s16 opcode, XPrimParam *param);
    virtual void Error(s16 code);
    virtual int HandleBreakpoint(StackElem *elem, BehaviorNode *node);
    virtual int Simulate(int a);
    virtual SpriteSlot *GetSpriteSlot();
    virtual SpriteSlot *GetSpriteSlot(u32 index);
    virtual void TransitionToNewHouse(s16 a, s16 b, s16 c);
    virtual void Cleanup(cXObject *object);
    virtual int GosubObjectTree(cXObject *object, s16 *params, s16 treeID, bool a);
    virtual void Initialize(); // 0x20F258
    virtual void Reset(bool a);
    virtual void PostLoad(int a, Globs::TransitionType type);
    virtual void PreSave();
    virtual void ApplyDamage(const Weather::DamageEvent &event, s16 a);
    virtual void CalcArmorAgainstDamage(const Weather::DamageEvent &event, Weather::Armor &armor);
    virtual void Turn(int a);
    virtual void Pickup();
    // The level and slot: HierarchySite's three-argument constructor stores the holder's
    // mLevel where PlacementSpec's stores the second argument, and both put the fourth
    // through GetObjectSlot.
    virtual int CanPlace(const FTilePt &tile, int level, cXObject *object, int slot);
    virtual void Place(const FTilePt &tile, int level, cXObject *object, int slot);
    virtual int IsPartOfMe(cXObject *object);
    virtual int UserCanPlace(const FTilePt &tile, int level, cXObject *object, int slot);
    virtual void UserPlace(const FTilePt &tile, int level, cXObject *object, int slot);
    virtual int UserCanPickup();
    virtual void UserPickup(bool a);
    virtual int UserCanDelete();
    virtual void ComputeRect(const FTilePt &tile, FTileRect *rect);
    virtual void ForceLocation();
    virtual int ContainsData(int index);
    virtual u8 GetColorIndex() const;
    virtual u8 SetColorIndex(u8 index); // returns the old index
    virtual int IsInteractionTile(bool a);
    virtual void GetNumTiles(int &a, int &b);
    virtual int HaveGraphicsBeenTagged();
    virtual void TagGraphicsField();
    virtual void ReconStream(ReconBuffer *buffer, int a, bool b);
    virtual u32 ReconType();
    // Wii: ReconSlots, the object IDs in the slots, from save version 5.
    void ReconSlots(ReconBuffer *buffer, int version); // 0x2137D8
    // Wii: ReconHeader, inlined on PS2: the rectangle, tile, level, ID and flags.
    inline void ReconHeader(ReconBuffer *buffer, int version); // defined in cXObject.cpp

    void HandleError();
    int GetIdleStatus() { return mModule->GetIdleStatus(mID); }
    void SetIdleStatus(int status) { mModule->SetIdleStatus(mID, status); }
    inline bool AllowIdleOptimization();
    s16 GetType() const { return mDef[9]; }
    // The game also has GetType and IsInWorld as functions (0x245708, 0x2535B0), and
    // GotoRoutingSlot calls those. Here they need their own names.
    s16 GetTypeOutOfLine() const;
    int IsInWorldOutOfLine() const;
    // Wii: IsPortal, inlined on PS2. CalcArmorAgainstDamage keeps GetPortal's result as a
    // value (sltu); GotoRoutingSlot drops the result.
    bool IsPortal()
    {
        bool portal = false;
        if (GetTypeOutOfLine() == 8)
            portal = GetPortal() != 0;
        return portal;
    }
    // Wii: IsRoof and IsStructure, by type; inlined into CalcArmorAgainstDamage.
    bool IsRoof() { return GetTypeOutOfLine() == 14; }
    bool IsStructure() { return GetTypeOutOfLine() == 5; }
    // Wii: IsVehicle, type 12. A function on both; the body is the evidence for the name.
    bool IsVehicle(); // 0x254078
    // Wii: GetHeight, definition word 0x1C, as a function on PS2 too.
    s16 GetHeightOutOfLine(); // 0x2456C8
    // Wii: IsDoor, inlined on PS2: a portal of height 2 with the portal tree.
    inline bool IsDoor(); // defined in cXObject.cpp
    // Wii: GetObstacleAtLocation, the first object on a tile that a Sim cannot pass, or a
    // door it passed on the way when asked for one.
    cXObject *GetObstacleAtLocation(FTilePt &tile, int level, bool doors); // 0x20FFB8
    // Wii: IsChair, whether the object has the tree at entry point 0x1a. Defined in
    // cXObject.cpp after GetTreeID, which it calls.
    inline bool IsChair();
    // Wii: GetDef as a function (0x245700 here too, called by GetStyle in
    // CalcRoomScoreImpact), GetStyle and SetData.
    s16 *GetDefOutOfLine(); // 0x245700
    // Wii: GetFnTable, out of line on PS2 as well: UpdateWallAdjacency calls it.
    ObjectFunctionTable *GetFnTableOutOfLine() const; // 0x2455F8
    // Wii: RequiresWallAdjacency, inlined on PS2.
    inline bool RequiresWallAdjacency() const; // defined in cXObject.cpp
    s16 GetStyle() { return GetDefOutOfLine()[0x15]; }
    void SetData(int index, s16 value) { mVars.WriteVar(index, value); }
    // Wii: GetData, a variable by index through the out-of-line operator[].
    s16 GetData(int index) const { return mVars[index]; }
    // The game has this inlined and as a function (0x254090, which UserCanPickup calls).
    bool IsPersonInline() const { return GetType() == 2; }
    bool IsPerson() const;
    // Wii: IsOccupied, bit 5 of variable 8. Inlined into UserCanPickup.
    int IsOccupied() const { return ((u16)mVars[8] >> 5) & 1; }
    bool IsMultiTile() const { return mDef[0xa] != 0; }
    bool IsInWorld() const
    {
        if (mTileX >> 4 < 1 || mTileY >> 4 < 1)
            return false;
        return true;
    }
    cXObject *GetNext() { return mNext; }
    cXObject *GetInteractionLeader();
    void ParseUIString(class BString2 &text, StackElem *elem, s16 *object, ObjSelector **selector, bool a);
    bool IsInteractionLeader() const
    {
        bool leader = true;
        if (IsMultiTile() && mDef[8] > 0)
            leader = false;
        return leader;
    }
    s16 GetID() const { return mID; }
    u16 GetRoom() const { return mVars[0x1d]; }
    inline CTilePt GetCTilePt() const;
    int GetNumRoutingSlots() { return mRoutingSlots.size(); }
    RoutingSlot *GetRoutingSlot(int index) { return &mRoutingSlots[index]; }
    FTilePt &GetLocationRef() { return *(FTilePt *)&mTileX; }
    // Wii keeps this as a function; PS2 inlines it, and its two calls go to the out-of-line
    // copies of GetObjectSlot and GetObjectFromID (0x2537D0 and 0x2457B0) that the game has
    // beside the inlined ones. GCC 2.95 compiles a body in the order it meets them and
    // cannot inline a function it has not compiled yet, so this one is defined here while
    // those two are only declared and get their inline bodies after the class.
    cXObject *GetContainedObject(int index)
    {
        ObjectSlot *slot = GetObjectSlot(index);
        if (slot != 0)
            return GetObjectFromID(slot->mObjectID);
        return 0;
    }
    int CountObjectSlots() { return mObjectSlots.size() - 1; }
    inline ObjectSlot *GetObjectSlot(int index);
    inline cXObject *GetObjectFromID(s16 id);
    s16 *GetDef() { return mDef; }
    ObjSelector *GetSelector() { return mSelector; }
    // Wii: GetBehavior, the selector's tree set.
    inline Behavior *GetBehavior(); // defined in cXObject.cpp
    // Wii: GetPlacementInfo, PlacementSpec(this)'s fields one by one; the last is a bool on Wii.
    void GetPlacementInfo(FTilePt *tile, int *level, cXObject **object, s16 *slot, int *valid); // 0x253520
    int RunTree(ObjEntryPoint entry, s16 a, s16 *param); // 0x253FD8
    // Wii: GetTreeID. The tree at an entry point, or 0. Defined in cXObject.cpp.
    inline int GetTreeID(ObjEntryPoint entry);
    // RunTree's body: ApplyDamage has it inlined while UserPickup, UserPlace and PostLoad call
    // the function, so the inline copy has its own name. Defined in cXObject.cpp.
    inline int RunTreeInline(ObjEntryPoint entry, s16 a, s16 *param);
    void SetLevel(int level);                            // 0x253EC8
    void SetLocation(const FTilePt &tile, int level);    // 0x212400
    int GetLevelOutOfLine() const;                       // 0x253EC0  Wii: GetLevel
    // Wii: Place(PlacementSpec *), a function on Wii that PS2 inlines into Pickup and Place:
    // into the hierarchy at the spec's site, or out of it for an invalid spec.
    inline void Place(PlacementSpec *spec); // defined in cXObject.cpp
    // Wii: SimObjectPlaced, for a single-tile object once Place is done.
    static void SimObjectPlaced(IBaseSimInstance *instance); // 0x21A750
    void UpdateSimFlags();                               // 0x253DC0
    void SetWasPlacedByPlayer(bool placed);               // 0x253C98
    bool GetWasPlacedByPlayer() const;                    // 0x253C68
    // Wii: HierSever, out of whatever holds it; HierSetSite puts it at a site.
    void HierSever();                                     // 0x210368
    void HierSetSite(const HierarchySite *site);          // 0x210240
    // Wii: HierGetObject, whatever is at a site already.
    cXObject *HierGetObject(const HierarchySite *site);  // 0x210418
    void UpdateAge();                                     // 0x20FEB0
    // Wii: JustBorn, the creation date in variables 0x2D, 0x2E, 0x2F, 0x18 and 0x15.
    void JustBorn();                                      // 0x20F840
    int CanPlaceSingleTile(const FTilePt &tile, int level, cXObject *object, int slot); // 0x253958
    // CanPlaceSingleTile's body: UserCanPlace has it inlined while CanPlace calls the
    // function. Defined in cXObject.cpp.
    inline int CanPlaceSingleTileInline(const FTilePt &tile, int level, cXObject *object, int slot);
    // Wii: TestPlace, the placement check behind CanPlace and UserCanPlace.
    int TestPlace(PlacementSpec *spec); // 0x2104C8
    int GetTileWidth();                 // 0x214258
    // Wii: GetPersonWidth, inlined on PS2. The width of every Sim's footprint.
    static int gPersonWidth; // 0x492814
    int GetPersonWidth() const { return gPersonWidth; }
    // Wii: HasZeroExtent, bit 2 of the flags variable; UpdateChairFacing skips such objects.
    int HasZeroExtent() const { return ((u16)mVars[8] >> 2) & 1; }
    // Wii: CanIntersectPeople, inlined on PS2. Below HasZeroExtent, so that
    // it inlines that too. A zero-extent object unless bit 1 of variable 8
    // is set; anything else when bit 4 is.
    int CanIntersectPeople() const
    {
        return (HasZeroExtent() ? ((u16)mVars[8] >> 1) ^ 1 : (u16)mVars[8] >> 4) & 1;
    }
    // Wii: HierGetChild, the object in slot `slot`. GetNextObjectSibling is slot 0.
    cXObject *HierGetChild(int slot); // 0x253678
    cXObject *GetNextObjectSibling() { return HierGetChild(0); }
    cXObject *GetNextObjectSiblingOutOfLine(); // 0x2535E0
    void UpdateWallAdjacency();         // 0x214098
    // Wii: GetRequiredSegment, the wall side an object must stand against (2, 8 or 0).
    int GetRequiredSegment();           // 0x253D00
    // Wii: TestIntersection, whether the object fits at a tile without overlapping another.
    int TestIntersection(FTilePt &tile, int level); // 0x211C50
    void UpdateChairFacing();           // 0x211978
    // Wii keeps this as a function; Turn inlines both calls. Defined in cXObject.cpp.
    static inline void UpdateChairFacing(ObjectModule *module, int x, int y, int level);
    // The same as a function, which Pickup calls.
    static void UpdateChairFacingOutOfLine(ObjectModule *module, int x, int y, int level); // 0x2539B0
    void CalcRoomScoreImpact();         // 0x21ADA0
    // Wii: sXDirTable and sYDirTable, a step in each of the eight directions, north first.
    // A ninth entry of 0, 0 is the tile itself: TestIntersection loops over all nine.
    static int sXDirTable[9]; // 0x4927C8  0, 1, 1, 1, 0, -1, -1, -1, 0
    static int sYDirTable[9]; // 0x4927F0  -1, -1, 0, 1, 1, 1, 0, -1, 0

    // Reads and writes one operand of an instruction: bhav.md lists the scopes. InterpValue
    // returns 1, or -1 once it has stopped the tree with an error.
    long InterpValue(s16 scope, s16 data, const s16 **ref, f32 **floatRef, s16 *value, s16 **a);
    int InterpWriteValue(s16 scope, s16 data, const s16 **ref, s16 value);
    // Read and write an operand given as LotTransition's one-byte code (see GetShortComboValue
    // in cXObjectPrims.cpp). Wii keeps both as functions; PS2 inlines them.
    inline s16 GetValueFromShortOwnerCombo(u8 code);
    inline void SetValueInShortOwnerCombo(u8 code, s16 value);
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
    f32 CalcShortDistance(cXObject *object) const;
    // Wii: CalcShortDistanceIn16thsOfTiles, from tile centres for anything but a Sim.
    f32 CalcShortDistanceIn16thsOfTiles(cXObject *object); // 0x212C08
    // The object in variable 0x1a, the one this is in. Wii: HierGetParent.
    cXObject *HierGetParent(); // 0x253640
    // Wii keeps this as a function; PS2 inlines it: none when variable 0xe is set.
    cXObject *GetPrevObjectSibling()
    {
        if (mVars[0xe] == 0)
            return HierGetParent();
        return 0;
    }
    // The container of the first object in this one's chain of previous siblings. The game
    // has it as a function (0x253730) and inlined in UserCanPickup. Defined after the class,
    // so that its GetObjectFromID is the inlined one.
    inline cXObject *GetContainerInline();
    cXObject *GetContainer();
    // Wii: IsContained, GetContainerID and GetContainedSlotNum, each from PlacementSpec(this).
    bool IsContained();                                  // 0x2536B0
    s16 GetContainerID();                                // 0x2536D8
    s16 GetContainedSlotNum();                           // 0x253708
    // Wii: ExtractContainedSims. Every Sim held anywhere inside this object is cleaned up.
    void ExtractContainedSims();                         // 0x253818
    // The same as a function, which GetPlacementSpec calls.
    cXObject *GetPrevObjectSiblingOutOfLine();           // 0x253600
    // Wii: GetPlacementSpec, where the object is now: its tile when on the grid, its level,
    // and the holder and slot of the first object in its chain of previous siblings.
    void GetPlacementSpec(PlacementSpec *spec);          // 0x2100E0
    // Wii: GetRootObject, the object that has a tile, or 0.
    cXObject *GetRootObject(FTilePt &tile, int level);   // 0x2534B8
    long ShowAllObjectDialogs(StackElem *elem, XPrimParam *param);
    static void ChangeSelectedSimL();
    long TryDialog(StackElem *, XPrimParam *, bool);
    long TrySnap(StackElem *, XPrimParam *);
    long TrySnap(const FTilePt &tile, int level, cXObject *object, int slot, bool ignoreRooms, int direction, bool footprint);
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
    void TransitionToLot(s16 destination, s16 type, s16 transition);
    long TryFamilyInventory(StackElem *, XPrimParam *);
    long TrySyncAudio(StackElem *, XPrimParam *);
    long TryNotifyPlans(StackElem *, XPrimParam *);
    long TryManageClothing(StackElem *, XPrimParam *);
};

// Whether this object can sleep through an Idle: it isn't a Sim, variable 0x19 is clear, and
// none of its sprites is active.
inline ObjectSlot *cXObject::GetObjectSlot(int index)
{
    if (index < 0 || index >= CountObjectSlots())
        return 0;
    return &mObjectSlots[index + 1];
}

inline cXObject *cXObject::GetObjectFromID(s16 id)
{
    return mModule->GetObjectFromID(id);
}

inline cXObject *cXObject::GetContainerInline()
{
    cXObject *object = this;
    while (object->GetPrevObjectSibling() != 0)
        object = object->GetPrevObjectSibling();
    return GetObjectFromID(object->mVars[0x1a]);
}

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

inline int ObjectModule::GetNumGlobalRoutingSlots()
{
    return mGlobalRoutingSlotsEnd - mGlobalRoutingSlots;
}

inline RoutingSlot *ObjectModule::GetGlobalRoutingSlot(int index)
{
    return &mGlobalRoutingSlots[index];
}

#endif
