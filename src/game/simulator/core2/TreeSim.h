#ifndef GAME_SIMULATOR_CORE2_TREESIM_H
#define GAME_SIMULATOR_CORE2_TREESIM_H

// The vtable stays an external symbol, so the rebuild links to the game's copy at 0x4B8220.
#pragma interface

#include "common/types.h"
#include "game/simulator/core/Behavior.h"
#include "game/simulator/core2/TreeStack.h"

class cXObject;
class cXPerson;
class cXMTObject;
class cXPortal;
class XPrimParam;

// Wii: IBaseSimInstance. Only the call TreeSim makes is declared; its slot is proven by the
// PS2 vtable offset 0x28 in TreeSim::GetISimInstance.
class IBaseSimInstance {
public:
    virtual void Slot1();
    virtual void Slot2();
    virtual void Slot3();
    virtual void Slot4();
    virtual IBaseSimInstance *GetISimInstance();
};

// Set when a tree stops on an error. A word on PS2 (sw), a byte on Wii.
extern int g_bTreeAssert;

// The tree interpreter: it runs an object's behaviour trees one node at a time on a TreeStack.
// The virtual functions are in the order of the Wii vtable, which the PS2 one follows.
// cXObject and its subclasses derive from this class and supply TryElement, the switch over
// primitive opcodes, and Error.
// Stops the tree with an error. On the Wii every use is the same three steps in a row.
#define TREE_ERROR(code)   \
    g_bTreeAssert = 1;     \
    SetError(code);        \
    Error(code)
#define TREE_ERROR_LOOP(code) \
    do {                      \
        TREE_ERROR(code);     \
    } while (0)

// The same as an assert: a do/while(0) around the test. The loop shows in the game's code in
// two ways: the compiler treats a branch out of a loop as unlikely, so it fills the delay slot
// from the error path, and the label after the loop is aligned to eight bytes.
#define TREE_CHECK(sim, condition, code, failed) \
    do {                                         \
        if (!(condition)) {                      \
            g_bTreeAssert = 1;                   \
            (sim)->SetError(code);               \
            (sim)->Error(code);                  \
            failed;                              \
        }                                        \
    } while (0)

class TreeSim {
public:
    cXObject *mObject;               // 0x00
    cXPerson *mPerson;               // 0x04
    cXMTObject *mMTObject;           // 0x08
    cXPortal *mPortal;               // 0x0c
    IBaseSimInstance *mISimInstance; // 0x10
    void *mESimPerson;               // 0x14
    int mIterations;                 // 0x18
    f32 field_0x1c;                  // 0x1c  Simulate clears it, RunCheckTree saves and restores it
    TreeStack mStack;                // 0x20
    int mLastTransition;             // 0x3c  the target of the last move: a node, or 0xfe, 0xff
    int mLastResult;                 // 0x40  1 if the last primitive took its true branch
    s16 *mDefaultParams;             // 0x44  used by Gosub when a call passes none
    s16 mError;                      // 0x48
    int field_0x4c;                  // 0x4c

    static s16 sMaxIterations;
    static int sInMainSim;

    TreeSim();
    virtual cXPortal *GetPortal() const;
    virtual cXPerson *GetPerson() const;
    virtual cXMTObject *GetMTObject() const;
    virtual cXObject *GetObject() const;
    virtual ~TreeSim();
    // Returns: 1 take the true target, 0 the false target, 2 stop for this tick, 3 run the same
    // node again, -1 or 4 fail. long is 64 bits on the EE, which DoNodeActionPrimitive shows.
    virtual long TryElement(StackElem *elem, s16 opcode, XPrimParam *param) = 0;
    virtual void Error(s16 code) = 0;
    virtual void StackJustPopped();
    virtual int HandleBreakpoint(StackElem *elem, BehaviorNode *node);
    // Runs a tree to its end in one go. Returns 1 if it returned true, 0 if false or on error.
    virtual int RunCheckTree(Behavior *behavior, s16 stackObject, s16 treeID, s16 *params);
    virtual void Initialize(int stackSize, s16 *defaultParams);
    virtual int Simulate(int a);
    virtual StackElem *GetHighLevelAction();
    virtual StackElem *GetCurElem();
    virtual StackElem *GetMainSimElem();
    virtual StackElem *GetNthElem(s16 index);
    virtual s16 GetStackSize();
    virtual int GetCurrentPrimitive();
    virtual int GetIterations();
    virtual int GetLastTransition();
    virtual int GetLastResult();
    virtual IBaseSimInstance *GetISimInstance();
    virtual int HasMainTreeSim();
    virtual void SetNoMainTreeSim(bool none);

    void SetError(s16 code) { mError = code; }
    s16 GetError() { return mError; }
    void ClearError() { mError = 0; }
    // cXObject derives from TreeSim, and this header can't include it, so this is the cast.
    TreeSim *ObjectAsTreeSim() const { return (TreeSim *)mObject; }
    void Reset(Behavior *behavior, s16 treeID);
    int RunOneTickTree(Behavior *behavior, s16 stackObject, s16 treeID, s16 *params);
    int Gosub(Behavior *behavior, const s16 *params, s16 treeID);
    int DoNodeAction(StackElem *elem);
    int DoNodeActionNormal(StackElem *elem, BehaviorNode *node);
    int DoNodeActionPrimitive(StackElem *elem, BehaviorNode *node);
    int DoNodeActionTransition(StackElem *elem);
};

#endif
