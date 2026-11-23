#include "game/simulator/core/cXObject.h"

#include "common/EGlobal.h"
#include "common/ReconBuffer.h"
#include "game/simulator/core/RelMatrix.h"
#include "game/GameData.h"
#include "game/simulator/core/Neighborhood.h"
#include "game/simulator/core/ObjectModule.h"
#include "game/simulator/core/FixedWorld.h"
#include "game/simulator/core/Lot.h"
#include "game/simulator/core/cXMTObject.h"
#include "game/simulator/core2/cXPerson.h"
#include "game/simulator/core2/SlotLoader.h"
#include "game/simulator/core/ObjectIterator.h"
#include "game/simulator/core/ObjSelector.h"
#include "game/simulator/core/SoundPlayer.h"
#include "game/simulator/core/PassiveInfluenceSystem.h"
#include "game/simulator/objectdata/XObjLang.h"

// UserCanPlace's placement check is inside a do { } while (0), so the original had a macro
// of this shape there: references inside one weigh extra in register priority, which is
// what puts this in s4 ahead of the condition flag. A plain if leaves this in s5 and the
// flag in s4, 13 words off.
#define RETURN_IF_CANT_PLACE(tile, level, object, slot)                \
    do {                                                        \
        if (!CanPlaceSingleTileInline(tile, level, object, slot))      \
            return 0;                                           \
    } while (0)

inline int cXObject::GetTreeID(ObjEntryPoint entry)
{
    return mSelector->GetFnTable()->HasFunction(entry);
}

inline Behavior *cXObject::GetBehavior()
{
    return mSelector->mBehavior;
}

inline bool cXObject::RequiresWallAdjacency() const
{
    return GetFnTableOutOfLine()->HasFunction(kObjEntryWallAdjacency) != 0;
}

inline bool cXObject::IsDoor()
{
    bool door = false;
    if (GetTypeOutOfLine() == 8 && GetHeightOutOfLine() == 2)
        door = (s16)GetTreeID(kObjEntryPortal) != 0;
    return door;
}

inline bool cXObject::IsChair()
{
    return GetTreeID(kObjEntryChair) != 0;
}

// The object on a tile gets its facing updated, when the tile is on the grid.
inline void cXObject::UpdateChairFacing(ObjectModule *module, int x, int y, int level)
{
    CTilePt tile(x, y, level);
    if (!GetFixedWorld()->OutOfBounds(tile)) {
        cXObject *object = module->GetObjectFromID(module->GetTileObjectID(tile));
        if (object != 0)
            object->UpdateChairFacing();
    }
}

inline int cXObject::RunTreeInline(ObjEntryPoint entry, s16 a, s16 *param)
{
    int tree = GetTreeID(entry);
    if (tree != 0)
        return RunCheckTree(mSelector->mBehavior, a, tree, param);
    return 0;
}

inline int cXObject::CanPlaceSingleTileInline(const FTilePt &tile, int level, cXObject *object, int slot)
{
    PlacementSpec spec(tile, level, object, slot);
    return TestPlace(&spec);
}

// Runs the tree at an entry point to its end, with `a` as the stack object. 0 when the
// object has no tree there.
int cXObject::RunTree(ObjEntryPoint entry, s16 a, s16 *param)
{
    return RunTreeInline(entry, a, param);
}

void cXObject::PreSave()
{
}

void cXObject::TagGraphicsField()
{
    mGraphicsTagged = 1;
}

SpriteSlot *cXObject::GetSpriteSlot()
{
    return mSpriteSlots.mpBegin;
}

void cXObject::ForceLocation()
{
}

u32 cXObject::ReconType()
{
    return 0x584f424a;
}

int cXObject::HandleBreakpoint(StackElem *elem, BehaviorNode *node)
{
    return TreeSim::HandleBreakpoint(elem, node);
}

SpriteSlot *cXObject::GetSpriteSlot(u32 index)
{
    return &mSpriteSlots[index];
}

int cXObject::IsPartOfMe(cXObject *object)
{
    return object == this;
}

int cXObject::ContainsData(int index)
{
    return mVars.Contains(index);
}

// Variable 0x37 is the colour index; only its low byte counts.
u8 cXObject::GetColorIndex() const
{
    return mVars[0x37];
}

u8 cXObject::SetColorIndex(u8 index)
{
    u8 old = GetColorIndex();
    mVars.WriteVar(0x37, index);
    return old;
}

// A plain object covers one tile; multi-tile objects override this.
void cXObject::GetNumTiles(int &width, int &depth)
{
    width = 1;
    depth = 1;
}

// True once after TagGraphicsField, then clears the tag.
int cXObject::HaveGraphicsBeenTagged()
{
    if (mGraphicsTagged) {
        mGraphicsTagged = 0;
        return 1;
    }
    return 0;
}

// A tree error: flags the object and tells the module its Sim is in error. The code is not
// kept here; TreeSim::SetError has it.
void cXObject::Error(s16 code)
{
    mMiscFlags |= 0x100;
    mModule->SetSimFlag(mID, ObjectModule::kSimFlagError, true);
}

// Not built: the sprite-slot loop is five instructions whose closing branch GCC leaves in
// reorder mode, so the assembler's implicit nop is its sixth, and tools/asm_filter.py refuses a
// short loop it cannot see whole ("a short loop ends in a branch outside .set noreorder"). The
// game's loop (0x255C90) is exactly that: jal, delay slot, addiu, lw, bne, nop. Evidence for the
// filter learning that case; until then the logic is complete and unverified.
#ifdef NON_MATCHING
// The tree step, then the object's own tick: variable 0x19 counts down and every sprite slot
// ticks.
int cXObject::Simulate(int a)
{
    int result = TreeSim::Simulate(a);
    if (result) {
        if (mVars[0x19])
            mVars.WriteVar(0x19, mVars[0x19] - 1);
        for (SpriteSlot *slot = mSpriteSlots.begin(); slot != mSpriteSlots.end(); ++slot)
            slot->Tick();
    }
    return result;
}
#endif

// Runs a tree of another object's behaviour on this stack, with that object as the stack object.
int cXObject::GosubObjectTree(cXObject *object, s16 *params, s16 treeID, bool a)
{
    mModule->ClearIdleStatus(mID);
    if (Gosub(object->mSelector->mBehavior, params, treeID)) {
        GetCurElem()->mStackObject = object->mID;
        return 1;
    }
    return 0;
}

// Whether the object can be interacted with on its tile: never for definition word 0x45 == 2,
// always for a Sim, a single-tile object or a negative word 8, never for word 0x45 == 1, and
// otherwise when the object has a tree at entry point 8.
int cXObject::IsInteractionTile(bool inWorldOnly)
{
    if (inWorldOnly && !IsInWorld())
        return 0;
    s16 *def = mDef;
    if (def[0x45] == 2)
        return 0;
    // The game keeps this first `return 1` and the next two cases jump back to it, which only a
    // goto gives (see HANDOFF.md on cross-jumping).
    if (def[9] == 2) {
    returnOne:
        return 1;
    }
    if (def[0xa] == 0)
        goto returnOne;
    if (def[8] < 0)
        goto returnOne;
    // The result's 1 is set here: the game reuses it as the operand of the next compare.
    int result = 1;
    if (def[0x45] == 1)
        return 0;
    if ((s16)mSelector->GetFnTable()->HasFunction(8) == 0)
        result = 0;
    return result;
}

int cXObject::CanPlace(const FTilePt &tile, int level, cXObject *object, int slot)
{
    return CanPlaceSingleTile(tile, level, object, slot);
}

// Flags the transition in the EOR globals by type: 2 goes to the main menu, 3 clears
// neighbourhood variable 1 if it is 1, 5 carries the lot number.
void cXObject::TransitionToNewHouse(s16 lot, s16 type, s16 c)
{
    if (type == 2) {
        Globs::pEORGlobals->field_0x480 = 1;
        _gd.mHudTarget->GotoMainMenu();
    } else if (type == 3) {
        Globs::pEORGlobals->field_0x47c = 1;
        if (GetNeighborhood()->mVars[1] == 1)
            GetNeighborhood()->SetNeighborhoodVar(1, 0);
    } else {
        EGlobal *globals = Globs::pEORGlobals;
        globals->field_0x48c = 1;
        if (type == 5) {
            globals->mTransitionLot = lot;
            globals->field_0x47c = 1;
        } else {
            globals->mTransitionLot = 1;
            globals->field_0x484 = 1;
        }
    }
}

// The user may delete an object when the globals say anything goes, or when it can be picked
// up, its variable 0x2b has bit 3 set, and none of its slots holds another object.
int cXObject::UserCanDelete()
{
    if (Globs::pEORGlobals->field_0x414)
        return 1;
    // The game calls this one directly, not through the vtable.
    if (!cXObject::UserCanPickup())
        return 0;
    if (!(mVars[0x2b] & 8))
        return 0;
    for (int i = CountObjectSlots() - 1; i >= 0; i--) {
        if (GetContainedObject(i))
            return 0;
    }
    return 1;
}

// A single-tile object runs its user-pickup tree, then the tree of every object in its slots;
// a multi-tile one leaves that to its parts. Then it is picked up.
void cXObject::UserPickup(bool a)
{
    if (!IsMultiTile()) {
        RunTree(kObjEntryUserPickup, 0, 0);
        int count = CountObjectSlots();
        for (int i = 0; i < count; i++) {
            if (GetContainedObject(i))
                GetContainedObject(i)->RunTree(kObjEntryUserPickup, 0, 0);
        }
    }
    Pickup();
}

// Placed, then the user-placement trees run the same way UserPickup runs the pickup ones.
void cXObject::UserPlace(const FTilePt &tile, int level, cXObject *object, int slot)
{
    Place(tile, level, object, slot);
    if (!IsMultiTile()) {
        RunTree(kObjEntryUserPlacement, 0, 0);
        int count = CountObjectSlots();
        for (int i = 0; i < count; i++) {
            if (GetContainedObject(i))
                GetContainedObject(i)->RunTree(kObjEntryUserPlacement, 0, 0);
        }
    }
}

// After a load: the level and location are set again, a selector that needs a reset (or the
// placeholder selector) loses its people and is cleaned up and re-initialised, and outside
// author mode the load tree runs with the transition type as its parameter.
void cXObject::PostLoad(int a, Globs::TransitionType type)
{
    if (mLevel == 0)
        SetLevel(1);
    SetLocation(GetLocationRef(), mLevel);
    s16 params[4];
    params[0] = type;
    params[1] = 0;
    params[2] = 0;
    params[3] = 0;
    int needsReset = mSelector->GetNeedsReset();
    ObjSelector *placeholder = GetObjectFolder()->GetPlaceholder();
    // Both builds compute this compare as a value before testing needsReset.
    int isPlaceholder = mSelector == placeholder;
    if (needsReset || isPlaceholder) {
        mModule->CleanupPeople(this);
        Cleanup(0);
        if (needsReset)
            RunTree(kObjEntryInit, 0, 0);
    }
    if (!_gd.IsAuthorMode())
        RunTree(kObjEntryLoad, 0, params);
    UpdateSimFlags();
}

// Weather damage: each damage type whose armour is under 255 times the scale adds its amount
// less the armour to the total, which never goes below zero. A positive total runs the
// apply-damage tree with it.
void cXObject::ApplyDamage(const Weather::DamageEvent &event, s16 scale)
{
    if ((s16)GetTreeID(kObjEntryApplyDamage) == 0)
        return;
    Weather::Armor armor;
    CalcArmorAgainstDamage(event, armor);
    s16 params[8];
    memset(params, 0, sizeof(params));
    for (int i = 0; i < Weather::DAMAGE_TYPE_COUNT; i++) {
        if (armor.values[i] < scale * 255) {
            int damage = params[0] + event.amounts[i] - armor.values[i];
            if (damage < 0)
                damage = 0;
            params[0] = damage;
        }
    }
    if (params[0] > 0)
        RunTreeInline(kObjEntryApplyDamage, 0, params);
}

bool cXObject::IsPerson() const
{
    return IsPersonInline();
}

cXObject *cXObject::HierGetParent()
{
    return GetObjectFromID(mVars[0x1a]);
}

cXObject *cXObject::GetContainer()
{
    return GetContainerInline();
}

// Whether the user may pick the object up, with the reason in gPlacementError when not: 0x25
// its variable 0x2b says it can't be, 0x26 it is in use (variable 0x3e, the occupied bit, or
// a Sim holds it), 0x27 its tile may not be edited. Two objects by GUID skip the check of
// what they contain; anything else fails when something in its slots can't be picked up.
int cXObject::UserCanPickup()
{
    gPlacementError = 0;
    gPlacementConflict = 0;
    if (Globs::pEORGlobals->field_0x414)
        return 1;
    if (!(mVars[0x2b] & 2)) {
        gPlacementError = 0x25;
        return 0;
    }
    if (mVars[0x3e] > 0) {
        gPlacementError = 0x26;
        return 0;
    }
    if (IsOccupied()) {
        gPlacementError = 0x26;
        return 0;
    }
    cXObject *container = GetContainerInline();
    if (container != 0 && container->IsPerson()) {
        gPlacementError = 0x26;
        return 0;
    }
    int guid = mSelector != 0 ? mSelector->GetGUID() : 0;
    switch (guid) {
    case 0x0D4BA4C8:
    case 0x152C7A14:
        break;
    default:
        for (int i = CountObjectSlots() - 1; i >= 0; i--) {
            cXObject *object = GetContainedObject(i);
            if (object != 0 && !object->UserCanPickup())
                return 0;
        }
        break;
    }
    CTilePt tile = GetCTilePt();
    if (!gFixedWorld->OutOfGrid(tile) && !gFixedWorld->MayEditTile(tile)) {
        gPlacementError = 0x27;
        return 0;
    }
    return 1;
}

// 0x255968. Nothing but an invalid spec; Pickup tests it anyway.
PlacementSpec::PlacementSpec(bool)
{
    mValid = 0;
}

// The tile is the one given unless the object goes into a slot of another object that is not
// a TS2 slot, when it is that object's own location.
PlacementSpec::PlacementSpec(const FTilePt &tile, int level, cXObject *object, int slot)
{
    mValid = 1;
    mTile = tile;
    if (object != 0) {
        ObjectSlot *objectSlot = object->GetObjectSlot(slot);
        if (objectSlot != 0 && !objectSlot->IsTS2ObjectSlot())
            mTile = object->GetLocationRef();
    }
    // Wii's order of the three stores; the scheduler puts the param store first either way,
    // but slot before object is what keeps -1 out of v0.
    mObject = object;
    mSlot = object != 0 ? slot : -1;
    mLevel = level;
}

int cXObject::CanPlaceSingleTile(const FTilePt &tile, int level, cXObject *object, int slot)
{
    return CanPlaceSingleTileInline(tile, level, object, slot);
}

// Whether the user may put the object at the tile, with the reason in gPlacementError when
// not: 1 the tile is off the grid or may not be edited, 0xB the slot it would go into has a
// height of 5 to 7. The tile must be a tile centre (both fractions 8) and TestPlace must
// agree, which set no code. The globals flag that lets UserCanDelete delete anything skips
// the slot check here.
int cXObject::UserCanPlace(const FTilePt &tile, int level, cXObject *object, int slot)
{
    gPlacementError = 0;
    gPlacementConflict = 0;
    if (GetFixedWorld()->OutOfBounds(CTilePt(tile, 1)) || !GetFixedWorld()->MayEditTile(CTilePt(tile, 1))) {
        gPlacementError = 1;
        return 0;
    }
    RETURN_IF_CANT_PLACE(tile, level, object, slot);
    if (tile.FracX() != 8 || tile.FracY() != 8)
        return 0;
    if (Globs::pEORGlobals->field_0x414)
        return 1;
    if (object != 0) {
        ObjectSlot *objectSlot = object->GetObjectSlot(slot);
        if (objectSlot != 0) {
            // Nested: && on one variable folds to a single unsigned range compare.
            int height = objectSlot->GetHeight();
            if (height < 8) {
                if (height >= 5) {
                    gPlacementError = 0xb;
                    return 0;
                }
            }
        }
    }
    return 1;
}

bool cXObject::IsVehicle()
{
    return GetType() == 12;
}

HierarchySite::HierarchySite(const PlacementSpec *spec)
{
    mValid = spec->mValid;
    mSlot = spec->mSlot;
    if (mSlot != -1)
        mSlot = mSlot + 1;
    mTile = spec->mTile;
    mLevel = spec->mLevel;
    mObject = spec->mObject;
}

HierarchySite::HierarchySite(cXObject *object, const FTilePt &tile, int slot)
{
    mObject = object;
    mSlot = slot;
    mTile = tile;
    mLevel = mObject->mLevel;
    mValid = 1;
}


// 0x211978. Wii: UpdateChairFacing. Looks at the four orthogonal neighbours for a chair facing
// this tile, then sets or clears bit 8 of variable 8 on every part, and reruns the placement
// tree on a part whose bit changed if it has tree 0x19.
void cXObject::UpdateChairFacing()
{
    CTilePt here = GetCTilePt();
    CTilePt tile = GetCTilePt();
    int x = here.GetX();
    int y = here.GetY();
    bool facing = false;
    cFixedWorld *world = GetFixedWorld();
    ObjectModule *module = GetObjectModule();
    for (int dir = 0; dir < 8; dir += 2) {
        tile.SetX(x + sXDirTable[dir]);
        tile.SetY(y + sYDirTable[dir]);
        if (!world->OutOfBounds(tile)) {
            cXObject *object = module->GetObjectFromID(module->GetTileObjectID(tile));
            while (object != 0 && object->HasZeroExtent())
                object = object->GetNextObjectSibling();
            if (object != 0) {
                int opposite = (dir + 4) & 7;
                if (object->IsChair()) {
                    if (object->mVars[1] == opposite) {
                        facing = true;
                        break;
                    }
                }
            }
        }
    }
    for (ObjectIterator it(this, ObjectIterator::kAll); !it.finished(); ++it) {
        cXObject *part = *it;
        bool was = ((u16)part->mVars[8] >> 8) & 1;
        s16 flags = part->GetData(8);
        if (facing)
            part->SetData(8, flags | 0x100);
        else
            part->SetData(8, flags & ~0x100);
        if ((was && !facing) || (!was && facing)) {
            s16 tree = part->GetTreeID(kObjEntryChairFacing);
            if (tree != 0)
                part->RunTree(kObjEntryPlacement, 0, 0);
        }
    }
}

// Not matched: 9 of 133 words differ (aligned), all in ApproxSqrt's float block. Four are the
// two nops the game has before each div.s (HANDOFF: "float division"; this is the first
// function with one), the rest that the game keeps the root in f0 throughout where ours uses
// f1 and f2. Tried: the root assigned to the end and returned, and returned as an expression.
// The logic is complete, and bhav.md describes it.
#ifdef NON_MATCHING
// Wii: ApproxSqrt, inlined on PS2: two Newton steps from x / 20 + 1, and 0 for x <= 0.
static inline f32 ApproxSqrt(f32 x)
{
    if (x <= 0.0f)
        return 0.0f;
    f32 root = x * 0.05f + 1.0f;
    root = (root + x / root) * 0.5f;
    root = (root + x / root) * 0.5f;
    return root;
}

// 0x212C08. Wii: CalcShortDistanceIn16thsOfTiles. Each end is the object's position, moved to
// the centre of its tile unless the object, or the container it is in, is a Sim.
f32 cXObject::CalcShortDistanceIn16thsOfTiles(cXObject *object)
{
    FTilePt from = GetLocationRef();
    cXObject *container = GetContainerInline();
    if (container != 0) {
        if (container->GetType() != 2) {
            from.mX += 8;
            from.mY += 8;
        }
    } else if (GetType() != 2) {
        from.mX += 8;
        from.mY += 8;
    }
    FTilePt to = object->GetLocationRef();
    container = object->GetContainerInline();
    if (container != 0) {
        if (container->GetType() != 2) {
            to.mX += 8;
            to.mY += 8;
        }
    } else if (object->GetType() != 2) {
        to.mX += 8;
        to.mY += 8;
    }
    return ApproxSqrt(to.SquareDist(from));
}
#endif

inline void cXObject::ReconHeader(ReconBuffer *buffer, int version)
{
    buffer->Recon32(&((FTileRect *)mRect)->mMinY, 1);
    buffer->Recon32(&((FTileRect *)mRect)->mMinX, 1);
    buffer->Recon32(&((FTileRect *)mRect)->mMaxY, 1);
    buffer->Recon32(&((FTileRect *)mRect)->mMaxX, 1);
    buffer->Recon32(&mTileX, 1);
    buffer->Recon32(&mTileY, 1);
    if (version >= 0x19)
        buffer->ReconInt(&mLevel, 1);
    if (version < 0x2f)
        buffer->Recon16(&mID, 1);
    s16 one = 1;
    buffer->Recon16(&one, 1);
    if (version == 0x37)
        buffer->ReconInt((int *)&mMiscFlags, 1);
}

// 0x213158. Wii: ReconStream. Saves or loads an object. A rebuild (loading a new object)
// reallocates any array the stream has more entries for; otherwise the extra entries are read
// and dropped. The semi-global attributes are in from version 0x63, the static ones from 0x76.
void cXObject::ReconStream(ReconBuffer *buffer, int version, bool rebuild)
{
    ReconHeader(buffer, version);
    if (version <= 0) {
        buffer->Recon16(mAttrs, 8);
        buffer->Recon16(mTemps, 0x20);
    }
    s16 count = mNumAttrs;
    buffer->Recon16(&count, 1);
    if (mNumAttrs < count) {
        if (rebuild) {
            delete[] mAttrs;
            mNumAttrs = count;
            mAttrs = new s16[count];
            buffer->Recon16(mAttrs, count);
        } else {
            s16 *saved = new s16[count];
            buffer->Recon16(saved, count);
            for (int i = 0; i < mNumAttrs; i++)
                mAttrs[i] = saved[i];
            delete[] saved;
        }
    } else {
        buffer->Recon16(mAttrs, count);
    }
    if (version > 0x62) {
        s16 semiCount = mNumSemiAttrs;
        buffer->Recon16(&semiCount, 1);
        if (mNumSemiAttrs < semiCount) {
            if (rebuild) {
                delete[] mSemiAttrs;
                mSemiAttrs = 0;
                mNumSemiAttrs = semiCount;
                if (semiCount > 0) {
                    mSemiAttrs = new s16[semiCount];
                    buffer->Recon16(mSemiAttrs, semiCount);
                }
            } else if (semiCount > 0) {
                s16 *saved = new s16[semiCount];
                buffer->Recon16(saved, semiCount);
                for (int i = 0; i < mNumSemiAttrs; i++)
                    mSemiAttrs[i] = saved[i];
                delete[] saved;
            } else {
                delete[] mSemiAttrs;
                mSemiAttrs = 0;
            }
        } else if (semiCount > 0) {
            buffer->Recon16(mSemiAttrs, semiCount);
        }
    }
    if (version > 0x75) {
        s16 staticCount = mNumStaticAttrs;
        buffer->Recon16(&staticCount, 1);
        if (mNumStaticAttrs < staticCount) {
            if (rebuild) {
                delete[] mStaticAttrs;
                mStaticAttrs = 0;
                mNumStaticAttrs = staticCount;
                if (staticCount > 0) {
                    mStaticAttrs = new s16[staticCount];
                    buffer->Recon16(mStaticAttrs, staticCount);
                }
            } else if (staticCount > 0) {
                s16 *saved = new s16[staticCount];
                buffer->Recon16(saved, staticCount);
                for (int i = 0; i < mNumStaticAttrs; i++)
                    mStaticAttrs[i] = saved[i];
                delete[] saved;
            } else {
                delete[] mStaticAttrs;
                mStaticAttrs = 0;
            }
        } else if (staticCount > 0) {
            buffer->Recon16(mStaticAttrs, staticCount);
        }
    }
    buffer->Recon16(mTemps, 8);
    mVars.ReconStream(buffer, version);
    if (version < 0x6a) {
        s16 unused = 0;
        buffer->Recon16(&unused, 1);
    }
    mStack.ReconStream(buffer, version, mSelector->mFolder);
    mRelMatrix->ReconStream(buffer, version);
    ReconSlots(buffer, version);
    s16 indexedCount = mNumIndexedData;
    buffer->Recon16(&indexedCount, 1);
    if (indexedCount > mNumIndexedData && rebuild) {
        delete[] mIndexedData;
        mNumIndexedData = indexedCount;
        mIndexedData = new s16[indexedCount];
    }
    buffer->Recon16(mIndexedData, indexedCount);
    if (rebuild)
        SetData(1, 0);
    if (GetISimInstance() != 0 && GetColorIndex() != 0)
        GetISimInstance()->SetColorIndex(GetColorIndex());
}

// 0x2137D8. Wii: ReconSlots. The slot count, then for each slot a short that is always 0
// and the ID of the object in it. The slot list grows to the count, reading or writing.
void cXObject::ReconSlots(ReconBuffer *buffer, int version)
{
    if (version < 5)
        return;
    s16 count = mObjectSlots.size();
    buffer->Recon16(&count, 1);
    for (int i = 0; i < count; i++) {
        s16 unused = 0;
        s16 id = 0;
        while (i >= (int)mObjectSlots.size()) {
            ObjectSlot slot;
            mObjectSlots.push_back(slot);
        }
        unused = 0;
        id = mObjectSlots[i].mObjectID;
        buffer->Recon16(&unused, 1);
        buffer->Recon16(&id, 1);
        mObjectSlots[i].SetObjectID(id);
    }
}

// 0x214098. Wii: UpdateWallAdjacency. For an object with tree 6: the low six bits of
// variable 5 become which sides of its tile have a wall, turned to the object's facing (bit 0
// segment 1, bit 2 segment 2, bit 1 segment 4, bit 3 segment 8), and tree 6 runs when that
// changes. Above GetCTilePt's second body, so that it calls 0x253ED0 as the game does.
void cXObject::UpdateWallAdjacency()
{
    if (!RequiresWallAdjacency())
        return;
    cFixedWorld *world = GetFixedWorld();
    s16 adjacency = GetData(5);
    adjacency &= ~0x3f;
    CTilePt tile = GetCTilePt();
    if (!world->OutOfBounds(tile)) {
        int rotation = (-GetData(1) / 2) & 3;
        TileWalls walls = world->GetWall(tile);
        walls.Rotate(rotation);
        if (walls.HasWall(1))
            adjacency |= 1;
        if (walls.HasWall(2))
            adjacency |= 4;
        if (walls.HasWall(4))
            adjacency |= 2;
        if (walls.HasWall(8))
            adjacency |= 8;
    }
    if (adjacency != GetData(5)) {
        SetData(5, adjacency);
        RunTree(kObjEntryWallAdjacency, 0, 0);
    }
}

// With no spec (always, from Pickup and the first call in Place, since PlacementSpec(true) is
// invalid on PS2) the object leaves the hierarchy and the object in its slot 0 takes its site;
// otherwise whatever is at the spec's site swaps places with it.
inline void cXObject::Place(PlacementSpec *spec)
{
    if (!spec->mValid) {
        HierarchySite site(this);
        HierSever();
        cXObject *child = mModule->GetObjectFromID(mObjectSlots[0].mObjectID);
        if (child != 0) {
            child->HierSever();
            child->HierSetSite(&site);
        }
    } else {
        HierarchySite site(spec);
        cXObject *occupant = HierGetObject(&site);
        if (occupant != this) {
            if (occupant != 0)
                occupant->HierSever();
            HierSetSite(&site);
            if (occupant != 0) {
                HierarchySite mine(this, site.mTile, 0);
                occupant->HierSetSite(&mine);
            }
        }
    }
}

// 0x211580. Wii: Place. Into an object's slot the level is that object's. The object first
// leaves where it was (and a chair updates whatever it faced there), then goes to the site:
// variables 2 and 3 name the holder and slot, it moves to the tile, the chairs around and in
// front of it update, its wall adjacency is recomputed, and tree 9 runs on a single-tile object
// and on the holder.
void cXObject::Place(const FTilePt &tile, int level, cXObject *object, int slot)
{
    int placeLevel = level;
    if (object != 0)
        placeLevel = object->GetLevelOutOfLine();
    int x = mTileX >> 4;
    int y = mTileY >> 4;
    s16 direction = GetData(1);
    PlacementSpec none(true);
    Place(&none);
    if (IsChair())
        UpdateChairFacingOutOfLine(mModule, x + sXDirTable[direction], y + sYDirTable[direction], mLevel);
    PlacementSpec spec(tile, placeLevel, object, slot);
    Place(&spec);
    if (spec.mObject != 0)
        SetData(2, spec.mObject->GetID());
    else
        SetData(2, 0);
    SetData(3, spec.mSlot);
    SetLocation(spec.mTile, spec.mLevel);
    x = mTileX >> 4;
    y = mTileY >> 4;
    UpdateChairFacingOutOfLine(mModule, x, y, mLevel);
    if (IsChair())
        UpdateChairFacingOutOfLine(mModule, x + sXDirTable[GetData(1)], y + sYDirTable[GetData(1)], mLevel);
    UpdateWallAdjacency();
    if (!IsMultiTile())
        RunTree(kObjEntryPlacement, 0, 0);
    if (object != 0)
        object->RunTree(kObjEntryPlacement, 0, 0);
    if (!IsMultiTile())
        SimObjectPlaced(GetISimInstance());
}

// 0x2112B8. Wii: Pickup. A single-tile object runs tree 10, then it leaves the hierarchy
// through Place with an invalid spec. Variables 2 and 3 are cleared to no holder and slot -1,
// it moves off the grid on level 1, bit 8 of variable 8 is cleared, a chair updates the facing
// of whatever is in front of where it stood, and the container it was in runs tree 9.
void cXObject::Pickup()
{
    if (!IsMultiTile())
        RunTree(kObjEntryPickup, 0, 0);
    int x = mTileX >> 4;
    int y = mTileY >> 4;
    s16 direction = GetData(1);
    PlacementSpec spec(true);
    cXObject *container = GetContainerInline();
    Place(&spec);
    SetData(2, 0);
    SetData(3, -1);
    FTilePt tile;
    tile.Set(-1, -1);
    SetLocation(tile, 1);
    SetData(8, GetData(8) & ~0x100);
    if (IsChair())
        UpdateChairFacingOutOfLine(mModule, x + sXDirTable[direction], y + sYDirTable[direction], mLevel);
    if (container != 0)
        container->RunTree(kObjEntryPlacement, 0, 0);
}

// 0x20F478. Wii: Reset. Back to how it was created, where it is: it leaves the hierarchy,
// the variables are reset but nine are kept (bhav.md lists them), variable 0xb is the ID
// again and 0x3a the definition's, the attribute arrays and temps are cleared, the stack
// restarts on the main tree, and it goes back to its site with variables 2 and 3 naming the
// holder and slot. Then it ages, goes idle and leaves the influence map and the listeners;
// a single-tile object runs tree 0, and when asked outside author mode it simulates once.
void cXObject::Reset(bool simulate)
{
    field_0x124 = 0;
    field_0x128 = 0;
    field_0x12c = 0;
    mCreationFrame = _framecount;
    HierarchySite site(this);
    HierSever();
    bool placed = GetWasPlacedByPlayer();
    EdithVariableSet saved(mVars);
    mVars.Reset();
    SetData(0x1d, saved[0x1d]);
    SetData(1, saved[1]);
    SetData(0x29, saved[0x29]);
    SetData(0x2d, saved[0x2d]);
    SetData(0x2e, saved[0x2e]);
    SetData(0x2f, saved[0x2f]);
    SetData(0x18, saved[0x18]);
    SetData(0x15, saved[0x15]);
    SetData(0x43, saved[0x43]);
    SetData(0xb, GetID());
    SetData(0x3a, GetDef()[0x31]);
    CalcRoomScoreImpact();
    SetWasPlacedByPlayer(placed);
    int i;
    for (i = 0; i < mNumAttrs; i++)
        mAttrs[i] = 0;
    for (i = 0; i < mNumSemiAttrs; i++)
        mSemiAttrs[i] = 0;
    for (i = 7; i >= 0; i--)
        mTemps[i] = 0;
    s16 tree = GetTreeID(kObjEntryMain);
    SetNoMainTreeSim(tree == 0);
    TreeSim::Reset(GetBehavior(), tree);
    HierSetSite(&site);
    PlacementSpec spec(this);
    if (spec.mObject != 0)
        SetData(2, spec.mObject->GetID());
    else
        SetData(2, 0);
    SetData(3, spec.mSlot);
    UpdateAge();
    SetIdleStatus(-1);
    GetSimulator()->GetPassiveInfluenceSystem()->RemoveObjectFromMap(this);
    GetSimulator()->RemoveAllForListener(GetID());
    if (!IsMultiTile()) {
        RunTree(kObjEntryInit, 0, 0);
        if (simulate && !_gd.IsAuthorMode() && HasMainTreeSim())
            Simulate(0);
        UpdateSimFlags();
    }
}

// 0x20F840. Wii: JustBorn. The simulator's date becomes the object's birth date: year,
// month and day in variables 0x2D, 0x2E and 0x2F, and globals 0 and 5 in 0x18 and 0x15.
void cXObject::JustBorn()
{
    cSimulator *sim = GetSimulator();
    SetData(0x2d, sim->GetGlobal(8));
    SetData(0x2e, sim->GetGlobal(7));
    SetData(0x2f, sim->GetGlobal(1));
    SetData(0x18, sim->GetGlobal(0));
    SetData(0x15, sim->GetGlobal(5));
}

// 0x20F8C0. Wii: Cleanup. Only with no object given. An object with tree 3 runs it, clears
// variable 0x3e and bit 5 of variable 8 (occupied) and restarts on its main tree; a multi-tile
// object does that for each part that has the tree, with the lead's tree set. Without tree 3
// it is picked up, reset, and put back where it was unless misc flag 0x40 is set. Then the
// object goes idle, its sounds stop and it lets go of the tutorial.
void cXObject::Cleanup(cXObject *object)
{
    if (object != 0)
        return;
    if ((s16)GetTreeID(kObjEntryCleanup) != 0) {
        if (IsMultiTile()) {
            cXMTObject *lead = GetMTObject();
            if (lead == 0)
                return;
            for (cXMTObject *part = lead->GetFirstMultiTileObject(); part != 0;
                 part = part->GetNextMultiTileObject()) {
                if ((s16)part->GetTreeID(kObjEntryCleanup) != 0) {
                    part->RunTree(kObjEntryCleanup, 0, 0);
                    part->SetData(0x3e, 0);
                    part->SetData(8, part->GetData(8) & ~0x20);
                    part->TreeSim::Reset(GetBehavior(), GetTreeID(kObjEntryMain));
                }
            }
        } else {
            RunTree(kObjEntryCleanup, 0, 0);
            SetData(0x3e, 0);
            SetData(8, GetData(8) & ~0x20);
            TreeSim::Reset(GetBehavior(), GetTreeID(kObjEntryMain));
        }
    } else {
        FTilePt tile;
        int level;
        cXObject *container;
        s16 slot;
        int placed;
        GetPlacementInfo(&tile, &level, &container, &slot, &placed);
        Pickup();
        if (!(mMiscFlags & 0x40)) {
            Reset(true);
            if (placed)
                Place(tile, level, container, slot);
        }
    }
    SetIdleStatus(-1);
    cSoundPlayer *player = GetSoundPlayer();
    if (player != 0)
        player->QuietBySourceID(GetID());
    if (mModule->GetTutorialObject() == this)
        mModule->SetTutorialObjectOutOfLine(0);
}

// 0x20FEB0. Wii: UpdateAge. Whole years since the birth date go in variable 0x12 and whole
// months in 0x30: a month not yet complete by the day counts one fewer, and a negative
// month count borrows a year.
void cXObject::UpdateAge()
{
    cSimulator *sim = GetSimulator();
    s16 years = sim->GetGlobal(8) - (u16)mVars[0x2d];
    s16 months = sim->GetGlobal(7) - (u16)mVars[0x2e];
    s16 days = sim->GetGlobal(1) - (u16)mVars[0x2f];
    if (days < 0)
        months--;
    if (months < 0) {
        months += 12;
        years--;
    }
    SetData(0x12, years);
    SetData(0x30, years * 12 + months);
}

// 0x20FFB8. Wii: GetObstacleAtLocation. Climbs the stack on a tile from its root object
// past everything a Sim can walk through.
cXObject *cXObject::GetObstacleAtLocation(FTilePt &tile, int level, bool doors)
{
    cXObject *object = GetRootObject(tile, level);
    cXObject *door = 0;
    if (object != 0) {
        do {
            if (!object->CanIntersectPeople())
                break;
            if (doors && object->IsDoor())
                door = object;
            object = object->GetNextObjectSiblingOutOfLine();
        } while (object != 0);
    }
    if (object == 0 && door != 0)
        return door;
    return object;
}

// 0x2100E0. Wii: GetPlacementSpec. A slot of 0 in variable 0xE means none, so the first
// object's variable 0xE is set back to -1 when it is found there.
void cXObject::GetPlacementSpec(PlacementSpec *spec)
{
    CTilePt tile(GetLocationRef(), 0);
    if (GetFixedWorld()->OutOfBounds(tile)) {
        spec->mValid = 0;
        spec->mTile.Set(-1, -1);
    } else {
        spec->mValid = 1;
        spec->mTile = GetLocationRef();
    }
    spec->mLevel = mLevel;
    cXObject *first = this;
    while (first->GetPrevObjectSiblingOutOfLine() != 0)
        first = first->GetPrevObjectSiblingOutOfLine();
    spec->mObject = GetObjectFromID(first->mVars[0x1a]);
    spec->mSlot = first->GetData(0xe);
    if (spec->mSlot != -1) {
        spec->mSlot--;
        if (spec->mSlot == -1)
            first->SetData(0xe, -1);
    }
}

// 0x210240. Wii: HierSetSite. Into a holder's slot: the slot records this object, variable
// 0xE is the slot and 0x1A the holder. On a tile: the tile records it, 0xE is -1 and 0x1A 0.
// Either way the level is the site's. An invalid site only severs it.
void cXObject::HierSetSite(const HierarchySite *site)
{
    if (!site->mValid)
        HierSever();
    CTilePt tile(site->mTile, site->mLevel);
    if (site->mObject == 0) {
        if (site->mValid) {
            GetObjectModule()->GetTileObjectID(tile);
            GetObjectModule()->SetTileObjectID(tile, mID);
            SetData(0xe, -1);
            SetData(0x1a, 0);
            SetLevel(site->mLevel);
        }
    } else {
        int slot = site->mSlot;
        site->mObject->mObjectSlots[slot].SetObjectID(GetID());
        SetData(0xe, slot);
        SetData(0x1a, site->mObject->GetID());
        SetLevel(site->mLevel);
    }
}

// 0x210368. Wii: HierSever. Clears whatever records this object, its holder's slot or its
// tile, and sets variable 0x1A to no holder and 0xE to no slot.
void cXObject::HierSever()
{
    HierarchySite site(this);
    if (site.mObject != 0) {
        site.mObject->mObjectSlots[site.mSlot].mObjectID = 0;
    } else if (site.mValid) {
        CTilePt tile(site.mTile, site.mLevel);
        GetObjectModule()->SetTileObjectID(tile, 0);
    }
    SetData(0x1a, 0);
    SetData(0xe, -1);
}

// 0x210418. Wii: HierGetObject. What a site holds: the object in the holder's slot, or the
// object on the tile for levels 1 to 3; 0 for an invalid site.
cXObject *cXObject::HierGetObject(const HierarchySite *site)
{
    if (site->mValid) {
        cXObject *holder = site->mObject;
        if (holder != 0)
            return holder->GetObjectFromID(holder->mObjectSlots[site->mSlot].mObjectID);
        if ((unsigned int)(site->mLevel - 1) < 3) {
            CTilePt tile(site->mTile, site->mLevel);
            return GetObjectFromID(GetObjectModule()->GetTileObjectID(tile));
        }
    }
    return 0;
}

// GetCTilePt's body again (cXObjectPrims.cpp has the first), here so that
// CalcArmorAgainstDamage inlines it, as the game does. It must come after UserCanPickup,
// which calls the game's copy at 0x253ED0: a function compiled before an inline's body has
// been seen calls it out of line.
inline CTilePt cXObject::GetCTilePt() const
{
    return CTilePt(mTileX >> 4, mTileY >> 4, mLevel);
}

// How much of a damage event the object's surroundings absorb. A roof absorbs nothing. A wall
// or portal is sheltered when both of its parts are in a room; anything else takes its room's
// two flags, and without a roofed room looks for a roof on its tile. Walls and a roof together
// absorb damage types 0 and 1 and the skin amount; walls alone half of type 0; a roof alone
// type 1 and the skin amount.
void cXObject::CalcArmorAgainstDamage(const Weather::DamageEvent &event, Weather::Armor &armor)
{
    int walled = 0;
    int roofed = 0;
    if (!IsRoof()) {
        if (IsStructure() || IsPortal()) {
            cXMTObject *first = GetMTObject()->GetFirstMultiTileObject();
            walled = first->GetData(0x1d) != 0;
            if (walled > 0) {
                if (first->GetNextMultiTileObject()->GetData(0x1d) == 0)
                    walled = 0;
            }
        } else {
            Room *room = GetRoomManager()->GetRoom(GetRoom());
            if (room != 0) {
                walled = room->mEnclosed;
                roofed = room->mRoofed;
            }
            if (roofed == 0) {
                CTilePt tile = GetCTilePt();
                if (!GetFixedWorld()->OutOfBounds(tile) && GetFixedWorld()->HasRoof(tile))
                    roofed = 1;
            }
        }
    }
    for (int i = Weather::DAMAGE_TYPE_COUNT; i >= 0; i--)
        armor.values[i] = 0;
    if (walled > 0) {
        if (roofed > 0) {
            armor.values[0] = event.amounts[0];
            armor.values[1] = event.amounts[1];
            armor.values[3] = event.skinAmount;
        } else {
            armor.values[0] = event.amounts[0] / 2;
        }
    } else if (roofed > 0) {
        armor.values[1] = event.amounts[1];
        armor.values[3] = event.skinAmount;
    }
}

// Variable 7, the object's impact on its room's score, from the definition's style value:
// halved for a wall, portal or roof, nothing for a Sim, a quarter more for definition word
// 0x45 equal to 9, rounded to the nearest. Only when variable 7 is still 0, and only for the
// lead part of a multi-tile object.
void cXObject::CalcRoomScoreImpact()
{
    if (IsMultiTile()) {
        if (!GetMTObject()->IsLeadObject())
            return;
    }
    if (GetData(7) != 0)
        return;
    f32 score = GetStyle();
    if (IsStructure() || IsPortal() || IsRoof()) {
        score *= 0.5f;
    } else if (IsPersonInline()) {
        score = 0.0f;
    } else if (mDef[0x45] == 9) {
        score *= 1.25f;
    }
    SetData(7, (s16)(int)(score + 0.5f));
}

// The footprint: a square of half the tile width around the tile, then shrunk on each side by
// a nibble of definition word 0x4d (less a Sim's person data 0x21 when variable 1's low bit is
// clear), the four nibbles turned by the object's direction in variable 1. Both builds call
// GetTileWidth twice and read variable 1 three times for one use; the first reads are
// kept here for the bytes, their shape in the original unknown.
void cXObject::ComputeRect(const FTilePt &tile, FTileRect *rect)
{
    s16 personOffsets = 0;
    GetTileWidth();
    int half = GetTileWidth() / 2;
    rect->SetWhole(tile.mX - half, tile.mY - half, tile.mX + half, tile.mY + half);
    s16 offsets = mDef[0x4d];
    if (GetType() == 2) {
        cXPerson *person = GetPerson();
        // GetData's s16 value, not the reference: the reference is loaded unsigned and tested
        // with xori.
        if ((GetData(1) & 1) == 0)
            personOffsets = person->GetPersonData(0x21);
    }
    if (offsets != 0 || personOffsets != 0) {
        int edges[4];
        edges[0] = offsets & 0xf;
        edges[1] = (offsets >> 4) & 0xf;
        edges[2] = (offsets >> 8) & 0xf;
        edges[3] = (offsets >> 12) & 0xf;
        if (personOffsets != 0) {
            edges[0] -= personOffsets & 0xf;
            edges[1] -= (personOffsets >> 4) & 0xf;
            edges[2] -= (personOffsets >> 8) & 0xf;
            edges[3] -= (personOffsets >> 12) & 0xf;
        }
        mVars[1];
        mVars[1];
        s16 direction = mVars[1] % 8;
        int turn = (8 - direction) / 2;
        rect->mMinY += edges[turn % 4];
        rect->mMaxX -= edges[(turn + 1) % 4];
        rect->mMaxY -= edges[(turn + 2) % 4];
        rect->mMinX += edges[(turn + 3) % 4];
    }
}

#ifdef NON_MATCHING
// 51 of 186 words by the aligned diff, every one a register name: the game holds the old
// direction in s2 and this and the variable set's address in s3 and s4, this compiler the
// other way round (s2, s3, s4). The instructions are the game's otherwise. Tried: the old
// direction as s16 and as int, read through mVars[1] and through GetData(1), copied into a
// second local before the lookup, the table lookups through an inline taking the direction
// (which reads variable 1 once where the game reads it twice), the table term on the left of
// the adds as the Wii has it for the first call (70 words) and for both (a shorter function),
// and the chair block inside a do { } while (0).
// Turns the object by amount steps of variable 0x17 (0 means it can't turn), keeping variable 1
// to eight directions. A chair tells the objects a step away in its old and new directions,
// which is what UpdateChairFacing does for them.
void cXObject::Turn(int amount)
{
    if (amount == 0)
        return;
    if (GetData(0x17) == 0)
        return;
    s16 oldDirection = mVars[1];
    SetData(1, (GetData(1) + GetData(0x17) * amount) & 7);
    if (IsChair()) {
        UpdateChairFacing(mModule, (mTileX >> 4) + sXDirTable[oldDirection],
                          (mTileY >> 4) + sYDirTable[oldDirection], mLevel);
        UpdateChairFacing(mModule, (mTileX >> 4) + sXDirTable[GetData(1)],
                          (mTileY >> 4) + sYDirTable[GetData(1)], mLevel);
    }
    UpdateWallAdjacency();
}
#endif

// 0x20F258. Wii keeps the slot, the loader and the tile as calls; PS2 inlines the slot's copy
// into push_back and both destructors, so the last store before the return is Slot's vtable
// pointer going into the dead stack slot.
void cXObject::Initialize()
{
    u32 namespaceID = GetSelector()->GetNamespaceID();
    field_0x124 = 0;
    field_0x128 = 0.0f;
    field_0x12c = 0;
    mCreationFrame = _framecount;
    GetObjectFolder()->GetNamespaceSelector(namespaceID);
    TreeSim::Initialize(GetSelector()->GetDefinition()[2], mTemps);
    ObjectSlot slot;
    mObjectSlots.push_back(slot);
    if (GetDef()[0x14] != 0) {
        s16 slotCount = GetDef()[0x14];
        u32 loaderNamespace;
        if (slotCount < 1000)
            loaderNamespace = namespaceID;
        else
            loaderNamespace = GetSelector()->GetSemiNamespaceID();
        SlotLoader loader(loaderNamespace);
        loader.Load(slotCount, &mObjectSlots, &mRoutingSlots, &mSpriteSlots, this);
    }
    mVars.Initialize(this);
    FTilePt tile;
    tile.Set(-1, -1);
    SetLocation(tile, 1);
    // A member-to-member s16 copy: lhu and sh, then the member's value sign-extended (sll, sra)
    // for the test and the size. A promoted local would load with lh.
    mNumIndexedData = mDef[0x41];
    if (mNumIndexedData != 0)
        mIndexedData = new s16[mNumIndexedData];
    else
        mIndexedData = 0;
    for (int i = 0; i < mNumIndexedData; i++)
        mIndexedData[i] = 0;
    CalcRoomScoreImpact();
}

// 0x253678
cXObject *cXObject::HierGetChild(int slot)
{
    return mModule->GetObjectFromID(mObjectSlots[slot].mObjectID);
}

// 0x214258. Wii: GetTileWidth. A Sim's width is a global, 0 for a ghost; anything else has it
// in definition word 0x43, 16 when that is 0.
int cXObject::GetTileWidth()
{
    if (GetType() == 2) {
        cXPerson *person = GetPerson();
        if (person != 0 && person->IsGhost())
            return 0;
        return GetPersonWidth();
    }
    int width = 16;
    if (mDef[0x43] != 0)
        width = mDef[0x43];
    return width;
}

// 0x2455F8. Wii: GetFnTable.
ObjectFunctionTable *cXObject::GetFnTableOutOfLine() const
{
    return mSelector->GetFnTable();
}

// 0x253520. Wii: GetPlacementInfo.
void cXObject::GetPlacementInfo(FTilePt *tile, int *level, cXObject **object, s16 *slot, int *valid)
{
    PlacementSpec spec(this);
    *tile = spec.mTile;
    *level = spec.mLevel;
    *slot = spec.mSlot;
    *object = spec.mObject;
    *valid = spec.mValid;
}

// 0x255478. Wii: SetTutorialObject, out of line.
bool ObjectModule::SetTutorialObjectOutOfLine(cXObject *object)
{
    if (mTutorialObject && object)
        return false;
    if (mTutorialObject == object)
        return true;
    mTutorialObject = object;
    return true;
}

// 0x2539B0. UpdateChairFacing(module, x, y, level) as a function.
void cXObject::UpdateChairFacingOutOfLine(ObjectModule *module, int x, int y, int level)
{
    UpdateChairFacing(module, x, y, level);
}

// 0x2534B8. Wii: GetRootObject.
cXObject *cXObject::GetRootObject(FTilePt &tile, int level)
{
    CTilePt where(tile, level);
    s16 id = GetObjectModule()->GetTileObjectID(where);
    if (id == 0)
        return 0;
    return GetObjectFromID(id);
}

// 0x253600. GetPrevObjectSibling as a function.
cXObject *cXObject::GetPrevObjectSiblingOutOfLine()
{
    return mVars[0xe] == 0 ? HierGetParent() : 0;
}

// 0x2456C8. Wii: GetHeight.
s16 cXObject::GetHeightOutOfLine()
{
    return GetDefOutOfLine()[0x1c];
}

// 0x2535E0. GetNextObjectSibling as a function.
cXObject *cXObject::GetNextObjectSiblingOutOfLine()
{
    return HierGetChild(0);
}

// 0x2536B0. Wii: IsContained.
bool cXObject::IsContained()
{
    PlacementSpec spec(this);
    return spec.mObject != 0;
}

// 0x2536D8. Wii: GetContainerID, 0 when not in anything.
s16 cXObject::GetContainerID()
{
    PlacementSpec spec(this);
    if (spec.mObject == 0)
        return 0;
    return spec.mObject->GetID();
}

// 0x253708. Wii: GetContainedSlotNum.
s16 cXObject::GetContainedSlotNum()
{
    PlacementSpec spec(this);
    return spec.mSlot;
}

// 0x253818. Wii: ExtractContainedSims. A Sim in a slot is cleaned up, which takes it out;
// anything else is searched in turn.
void cXObject::ExtractContainedSims()
{
    for (int i = 0; i < CountObjectSlots(); i++) {
        cXObject *object = GetContainedObject(i);
        if (object != 0) {
            if (object->IsPerson())
                object->GetPerson()->Cleanup(0);
            else
                object->ExtractContainedSims();
        }
    }
}

// 0x253EC0. Wii: GetLevel.
int cXObject::GetLevelOutOfLine() const
{
    return mLevel;
}
