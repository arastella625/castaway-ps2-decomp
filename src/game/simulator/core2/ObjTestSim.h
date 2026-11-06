#ifndef GAME_SIMULATOR_CORE2_OBJTESTSIM_H
#define GAME_SIMULATOR_CORE2_OBJTESTSIM_H

#include "common/types.h"

// One ad of a tree table entry, 16 bytes.
struct CheckTreeAd {
    u32 field_0x0;       // 0x00
    s16 mPersonalityVar; // 0x04
    s16 mMin;            // 0x06
    s16 mRange;          // 0x08
    s16 field_0xa;       // 0x0a
    u32 field_0xc;       // 0x0c
};

// Wii: TTabScratchEntry, seen through the one virtual call InterpValue makes. TTabScratchEntry.h
// declares the same class as plain functions, from before its vtable was known; the slots here
// keep CountAds where the vtable has it.
class CheckTreeAdsVirtuals {
public:
    virtual void Slot0();
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual void Slot5();
    virtual void Slot6();
    virtual void Slot7();
    virtual void Slot8();
    virtual void Slot9();
    virtual void Slot10();
    virtual void Slot11();
    virtual void Slot12();
    virtual void Slot13();
    virtual void Slot14();
    virtual void Slot15();
    virtual void Slot16();
    virtual void Slot17();
    virtual void Slot18();
    virtual void Slot19();
    virtual void Slot20();
    virtual void Slot21();
    virtual void Slot22();
    virtual void Slot23();
    virtual void Slot24();
    virtual void Slot25();
    virtual void Slot26();
    virtual void Slot27();
    virtual void Slot28();
    virtual void Slot29();
    virtual void Slot30();
    virtual void Slot31();
    virtual void Slot32();
    virtual void Slot33();
    virtual void Slot34();
    virtual void Slot35();
    virtual void Slot36();
    virtual void Slot37();
    virtual void Slot38();
    virtual void Slot39();
    virtual void Slot40();
    virtual void Slot41();
    virtual void Slot42();
    virtual int CountAds() const;
};

class CheckTreeAds : public CheckTreeAdsVirtuals {
public:
    u32 field_0x4;       // 0x04
    CheckTreeAd mAds[1]; // 0x08
};

// Wii: ObjTestSim. sCheckTreeModEntry is what GetCheckTreeAds reads.
class ObjTestSim {
public:
    static CheckTreeAds *sCheckTreeModEntry;
};

#endif
