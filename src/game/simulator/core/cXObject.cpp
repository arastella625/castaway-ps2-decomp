#include "game/simulator/core/cXObject.h"

#include "common/EGlobal.h"
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

// Not matched: 26 of 180 words differ (aligned), every one from a single register choice.
// The game keeps &mVars in s3 and the address sp+0x20 in s4, and uses that register only for
// the off-grid tile; each HierarchySite at the same slot is passed as a fresh addiu. Ours
// shares one register for the tile and both sites, so it has more uses, ranks first and takes
// s3. Tried: the tile at function scope (its own slot, frame 0x120), and the tile inside an
// inline helper (no change). The logic is complete; bhav.md describes it.
#ifdef NON_MATCHING
// 0x2112B8. Wii: Pickup, a different body from PS2's. A single-tile object runs tree 10.
// Then it leaves the hierarchy: with no spec (always, since PlacementSpec(true) is invalid on
// PS2) the object in its slot 0 takes its site; otherwise whatever is at the spec's site
// swaps places with it. Variables 2 and 3 are cleared to no holder and slot -1, it moves off
// the grid on level 1, bit 8 of variable 8 is cleared, a chair updates the facing of whatever
// is in front of where it stood, and the container it was in runs tree 9.
void cXObject::Pickup()
{
    if (!IsMultiTile())
        RunTree(kObjEntryPickup, 0, 0);
    int x = mTileX >> 4;
    int y = mTileY >> 4;
    s16 direction = GetData(1);
    PlacementSpec spec(true);
    cXObject *container = GetContainerInline();
    if (!spec.mValid) {
        HierarchySite site(this);
        HierSever();
        cXObject *child = mModule->GetObjectFromID(mObjectSlots[0].mObjectID);
        if (child != 0) {
            child->HierSever();
            child->HierSetSite(&site);
        }
    } else {
        HierarchySite site(&spec);
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
#endif

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
