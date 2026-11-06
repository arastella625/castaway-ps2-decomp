#include "game/simulator/core2/TreeStack.h"
#include "game/simulator/core2/TreeSim.h"

// StackElem and TreeStack share this file because the order of definitions decides what the
// compiler inlines: it only inlines a body it has already seen. In the game AssignFrames, Push
// and Pop inline NextFrame but call GetTreeID and the memory functions.

s16 *StackElem::GetParams()
{
    return GetParamsInline();
}

s16 *StackElem::GetLocals()
{
    return (s16 *)((u8 *)this + (mNumParams * sizeof(s16) + 0x14));
}

StackElem *StackElem::NextFrame() const
{
    return NextFrameInline();
}

void TreeStack::AssignFrames(int count)
{
    mFrames.erase(mFrames.begin(), mFrames.end());
    StackElem *frame = (StackElem *)mMemory;
    for (int i = 0; i < count; i++) {
        mFrames.push_back(frame);
        frame = frame->NextFrameInline();
    }
}

inline StackElem *TreeStack::MakeNewFrame(u32 size)
{
    u32 reserved = GetMemReserved();
    u32 used = GetMemUsed();
    if (reserved - used >= size)
        return GetNewFrame();

    u32 newSize = reserved * 2;
    if (newSize == 0) {
        newSize = 0x200;
        if (size * 2 > newSize)
            newSize = size * 2;
    }
    newSize = (newSize + 7) & ~7;
    u8 *memory = (u8 *)MainHeap()->MallocWrapper(newSize, 0);
    memcpy(memory, mMemory, reserved);
    if (mMemory != 0)
        MainHeap()->Free(mMemory);
    mMemory = memory;
    mMemoryEnd = memory + newSize;
    AssignFrames(mFrames.size());
    GetMemReserved();
    GetMemUsed();
    return GetNewFrame();
}

inline void StackElem::Setup(const StackElem *frame, const s16 *params)
{
    mTreeID = frame->GetTreeID();
    mNodeIndex = frame->mNodeIndex;
    field_0x4 = frame->field_0x4;
    mNumLocals = frame->mNumLocals;
    mNumParams = frame->mNumParams;
    mWaiting = frame->mWaiting;
    mBehavior = frame->mBehavior;
    if (params != 0) {
        s16 *to = GetParamsInline();
        for (int i = mNumParams - 1; i >= 0; i--)
            to[i] = params[i];
    }
}

void TreeStack::Push(const StackElem *frame, const s16 *params)
{
    StackElem *top = MakeNewFrame(frame->GetSize());
    top->Setup(frame, params);
    mFrames.push_back(top);
}

void TreeStack::Pop()
{
    TREE_CHECK(mOwner, mFrames.size() != 0, 0x3e9, );
    mFrames.pop_back();
}

StackElem *TreeStack::GetNewFrame()
{
    u32 size = mFrames.size();
    if (size != 0)
        return mFrames[size - 1]->NextFrameInline();
    return (StackElem *)mMemory;
}

u32 TreeStack::GetMemUsed()
{
    u32 size = mFrames.size();
    if (size != 0)
        return (u8 *)mFrames[size - 1]->NextFrameInline() - mMemory;
    return 0;
}

u32 TreeStack::GetMemReserved()
{
    return mMemoryEnd - mMemory;
}

void TreeStack::Reset()
{
    mFrames.erase(mFrames.begin(), mFrames.end());
}

StackElem *TreeStack::GetNthFrame(int index)
{
    if (index < 0 || (u32)index >= mFrames.size())
        return 0;
    return mFrames[index];
}

int TreeStack::GetStackSize()
{
    return mFrames.size();
}

s16 StackElem::GetTreeID() const
{
    if (mTreeID == -1)
        return -1;
    return mTreeID & 0x7fff;
}

void StackElem::SetTreeID(s16 id)
{
    if (id == -1)
        mTreeID = id;
    else
        mTreeID = id | (mTreeID & 0x8000);
}

void StackElem::SetBreak(bool on)
{
    if (mTreeID == -1)
        return;
    if (on)
        mTreeID |= 0x8000;
    else
        mTreeID &= 0x7fff;
}
