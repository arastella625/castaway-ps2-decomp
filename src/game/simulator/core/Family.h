#ifndef GAME_SIMULATOR_CORE_FAMILY_H
#define GAME_SIMULATOR_CORE_FAMILY_H

#include "common/types.h"

class InventoryItems;

// Wii: Family, the interface FamilyImpl implements. Only the slots the decompiled code calls
// have real signatures; the others are placeholders that keep them in place.
class Family {
public:
    virtual void slot0();
    virtual void slot1();
    virtual void slot2();
    virtual void slot3();
    virtual void slot4();
    virtual void slot5();
    virtual void slot6();
    virtual void slot7();
    virtual void slot8();
    virtual void slot9();
    virtual void slot10();
    virtual void slot11();
    virtual int slot12(int a); // true if the neighbour with this field is in the family
    virtual void slot13();
    virtual void slot14();
    virtual void slot15();
    virtual void slot16();
    virtual void slot17();
    virtual void slot18();
    virtual void slot19();
    virtual int GetNumber(); // slot 20, likely: Neighborhood::GetFamily compares it
    virtual void slot21();
    virtual void slot22();
    virtual void slot23();
    virtual void slot24();
    virtual void slot25();
    virtual void slot26();
    virtual void slot27();
    virtual void slot28();
    virtual void slot29();
    virtual void slot30();
    virtual void slot31();
    virtual int GetFunds();  // slot 32, likely: GenericSimCall 5 adds it to the funds
    virtual void slot33();
    virtual void slot34();
    virtual void slot35();
    virtual void slot36();
    virtual void slot37();
    virtual void slot38();
    virtual void slot39();
    virtual void slot40();
    virtual void slot41();
    virtual void slot42();
    virtual void slot43();
    virtual void slot44();
    virtual void slot45();
    virtual InventoryItems *GetInventory();
    virtual void slot47();
    virtual int GetVarRef(s16 index, const s16 **ref);
};

#endif
