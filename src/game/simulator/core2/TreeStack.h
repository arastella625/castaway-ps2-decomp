#ifndef GAME_SIMULATOR_CORE2_TREESTACK_H
#define GAME_SIMULATOR_CORE2_TREESTACK_H

#include "common/eastl.h"
#include "common/types.h"
#include "game/simulator/core2/StackElem.h"

class TreeSim;

class EAHeap {
public:
    void *MallocWrapper(unsigned int size, int flags);
    void Free(void *block);
};
EAHeap *MainHeap();

// The call stack of one TreeSim. Frames live in one block of memory, in order; mFrames holds
// a pointer to each.
class TreeStack {
public:
    u8 *mMemory;                                          // 0x00
    u8 *mMemoryEnd;                                       // 0x04
    eastl::vector<StackElem *, eastl::allocator> mFrames; // 0x08
    TreeSim *mOwner;                                      // 0x18

    TreeStack(TreeSim &owner) : mFrames(eastl::allocator()), mOwner(&owner)
    {
        mMemory = 0;
        mMemoryEnd = 0;
    }
    ~TreeStack()
    {
        MainHeap()->Free(mMemory);
        mMemory = 0;
    }

    void Initialize(int count) { mFrames.reserve(count); }
    void Reset();
    StackElem *GetNthFrame(int index);
    int GetStackSize();
    StackElem *GetNewFrame();
    u32 GetMemUsed();
    u32 GetMemReserved();
    void AssignFrames(int count);
    StackElem *MakeNewFrame(u32 size);
    void Push(const StackElem *frame, const s16 *params);
    void Pop();
};

#endif
