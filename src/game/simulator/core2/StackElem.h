#ifndef GAME_SIMULATOR_CORE2_STACKELEM_H
#define GAME_SIMULATOR_CORE2_STACKELEM_H

#include "common/types.h"

class Behavior;

// One frame of a tree's call stack. A frame is this header, then its parameters, then its
// locals, as 16-bit values; TreeStack keeps frames back to back in one block of memory.
class StackElem {
public:
    s16 mTreeID;       // 0x00  low 15 bits the tree, bit 15 the break flag; -1 means no tree
    s16 mNodeIndex;    // 0x02  the node about to run, or a transition (0xfd to 0xff)
    s16 mStackObject;  // 0x04  object ID: scope 0xa reads it, other scopes pass it to GetObjectFromID
    u8 mNumLocals;     // 0x06
    u8 mNumParams;     // 0x07
    u32 mWaiting;      // 0x08  name is ours: a primitive that moves on clears it; a transition
                       //       back into a frame that has it set does not run a node
    u32 field_0xc;     // 0x0c
    Behavior *mBehavior; // 0x10  null marks the frame that TreeSim::RunCheckTree puts first

    s16 GetNodeIndex() const { return mNodeIndex; }
    s16 GetTreeID() const;
    s16 *GetParams();
    s16 *GetLocals();
    StackElem *NextFrame() const;
    void SetTreeID(s16 id);
    void SetBreak(bool on);
    void Setup(const StackElem *frame, const s16 *params);

    // The header, then the parameters and locals rounded up to a multiple of four values.
    u32 GetSize() const
    {
        u32 count = mNumLocals + mNumParams;
        count = (count + 3) & ~3;
        return count * sizeof(s16) + 0x14;
    }

    // The game inlines NextFrame in TreeStack and also has an out-of-line copy. An inline
    // function's copy lands in a linkonce section, which the tools don't check, so the inline
    // body is here under its own name. The name is ours.
    StackElem *NextFrameInline() const { return (StackElem *)((u8 *)this + GetSize()); }
    s16 *GetParamsInline() { return (s16 *)((u8 *)this + 0x14); }
};

#endif
