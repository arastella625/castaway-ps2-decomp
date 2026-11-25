#include "game/simulator/core2/SlotLoader.h"
#include "game/simulator/core/cXObject.h"
#include "game/simulator/core/Behavior.h"
#include "common/EGlobal.h"

// Wii: ObjectDataSlotDescList, a loaded list of slot descriptors. Only the entries Load calls
// are named; the vtable is in Wii order after IConsoleUnknown's three.
class ObjectDataSlotDescList : public IConsoleUnknown {
public:
    virtual void GetDataID();
    virtual bool LoadFromDataID(const ObjectDataID &id); // bool: Load's test is xori 1
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual int GetNumSlots();
    virtual SlotDescriptor *GetSlot(int index);
    // The seventeenth entry, a short Load keeps in the loader when it has none. Name ours.
    virtual s16 GetDefaultID();
};

// 0x1E1C38
SlotLoader::SlotLoader(u32 namespaceID)
{
    mNamespaceID = namespaceID;
    field_0x4 = 0;
}

// 0x1E1C48. Empty, so the body is the compiler's: delete this when the in-charge flag's low
// bit is set.
SlotLoader::~SlotLoader()
{
}

inline ObjectSlot::ObjectSlot(const SlotDescriptor *descriptor)
{
    mXOffset = descriptor->mOffset[0];
    mYOffset = descriptor->mOffset[1];
    field_0x20 = descriptor->mOffset[2];
    mObjectID = 0;
    field_0x14 = descriptor->mFacing;
    field_0xc = descriptor->field_0x10;
    mFlags = descriptor->field_0x14;
    SetHeight(descriptor->mHeight);
}

inline RoutingSlot::RoutingSlot(const SlotDescriptor *descriptor)
{
    mOffsetX = descriptor->mOffset[0];
    mOffsetY = descriptor->mOffset[1];
    mOffsetZ = descriptor->mOffset[2];
    mFlags = descriptor->mFlags;
    mMultiplier[0] = descriptor->mMultiplier[0];
    mMultiplier[1] = descriptor->mMultiplier[1];
    mMultiplier[2] = descriptor->mMultiplier[2];
    mSnapTarget = descriptor->mSnapTarget;
    mMin = descriptor->mMin;
    mMax = descriptor->mMax;
    mOptimal = descriptor->mOptimal;
    mGradient = descriptor->mGradient;
    mFacing = descriptor->mFacing;
    mResolution = descriptor->mResolution;
}

inline SpriteSlot::SpriteSlot(const SlotDescriptor *descriptor, cXObject *object)
{
    mOffsetX = descriptor->mOffset[0];
    mOffsetY = descriptor->mOffset[1];
    mAltOffset = descriptor->mOffset[2];
    mObject = object;
    Globs::pEORGlobals->AllocSpriteRenderer(object);
    mSpriteID = 0;
    field_0x1c = 0;
    field_0x20 = 400;
    field_0x24 = 0;
    field_0x28 = 0;
    field_0x30 = 0;
    mSprite = 0;
    mPriority = 0;
    mShowOverInactivePeople = 0;
    mBalloonSprite = -1;
    mOverlaySprite = -1;
    field_0x4c = descriptor->mOffset[2];
    Reset();
}

// 0x1C1148. Wii: Load.
int SlotLoader::Load(s16 id, eastl::vector<ObjectSlot, eastl::allocator> *objectSlots,
                     eastl::vector<RoutingSlot, eastl::allocator> *routingSlots,
                     eastl::vector<SpriteSlot, eastl::allocator> *spriteSlots, cXObject *object)
{
    ConsoleAutoRefCount<ObjectDataSlotDescList> list;
    if (!GetSmartDataFactory()->GetDataByResFile(0x2c030753, mNamespaceID, list.AsPPTypeParam()))
        return 0;
    if (!list->LoadFromDataID(ObjectDataID(mNamespaceID, (u16)id)))
        return 0;
    if (field_0x4 == 0)
        field_0x4 = list->GetDefaultID();
    for (int i = 0; i < list->GetNumSlots(); i++) {
        SlotDescriptor *descriptor = list->GetSlot(i);
        if (descriptor->mType == 0) {
            if (objectSlots != 0) {
                ObjectSlot slot(descriptor);
                objectSlots->push_back(slot);
            }
        } else if (descriptor->mType == 1) {
            if (spriteSlots != 0 && object != 0) {
                SpriteSlot slot(descriptor, object);
                spriteSlots->push_back(slot);
            }
        } else if (descriptor->mType == 3) {
            if (routingSlots != 0) {
                RoutingSlot slot(descriptor);
                routingSlots->push_back(slot);
            }
        }
    }
    return 1;
}
