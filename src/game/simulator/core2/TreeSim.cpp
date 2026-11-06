#include "game/simulator/core2/TreeSim.h"
#include "game/simulator/core/cXObject.h"

cXPortal *TreeSim::GetPortal() const
{
    return mPortal;
}

cXPerson *TreeSim::GetPerson() const
{
    return mPerson;
}

cXMTObject *TreeSim::GetMTObject() const
{
    return mMTObject;
}

cXObject *TreeSim::GetObject() const
{
    return mObject;
}

void TreeSim::StackJustPopped()
{
}

int TreeSim::GetIterations()
{
    return mIterations;
}

int TreeSim::GetLastResult()
{
    return mLastResult;
}

TreeSim::TreeSim()
    : mObject(0), mPerson(0), mMTObject(0), mPortal(0), mISimInstance(0), mESimPerson(0), mIterations(0),
      mStack(*this), mLastTransition(0xfd), mLastResult(0), mDefaultParams(0), mError(0),
      field_0x4c(0)
{
}

TreeSim::~TreeSim()
{
}

void TreeSim::Initialize(int stackSize, s16 *defaultParams)
{
    mStack.Initialize(stackSize);
    mDefaultParams = defaultParams;
}

int TreeSim::Simulate(int a)
{
    mIterations = 0;
    field_0x1c = 0.0f;
    for (;;) {
        switch (DoNodeAction(GetCurElem())) {
        case 0:
            break;
        case 1:
            return 1;
        case 2:
            return 0;
        case 3:
            break;
        }
    }
}

int TreeSim::GetLastTransition()
{
    if (mLastTransition == 0xfe)
        return 1;
    if (mLastTransition == 0xff)
        return 0;
    return 0;
}

StackElem *TreeSim::GetCurElem()
{
    return mStack.GetNthFrame(mStack.GetStackSize() - 1);
}

StackElem *TreeSim::GetNthElem(s16 index)
{
    return mStack.GetNthFrame(index);
}

s16 TreeSim::GetStackSize()
{
    return mStack.GetStackSize();
}

int TreeSim::GetCurrentPrimitive()
{
    StackElem *elem = GetCurElem();
    Behavior *behavior = elem->mBehavior;
    BehaviorNode node;
    if (behavior->GetNode(elem->GetTreeID(), elem->mNodeIndex, &node))
        return node.GetPrimCode();
    return -1;
}

void TreeSim::Reset(Behavior *behavior, s16 treeID)
{
    mStack.Reset();
    if (treeID != 0) {
        s16 params[4];
        memset(params, 0, sizeof(params));
        Gosub(behavior, params, treeID);
        mError = 0;
    }
}

int TreeSim::RunOneTickTree(Behavior *behavior, s16 a, s16 treeID, s16 *params)
{
    return RunCheckTree(behavior, a, treeID, params);
}

int TreeSim::HandleBreakpoint(StackElem *elem, BehaviorNode *node)
{
    if (elem != 0)
        elem->SetBreak(false);
    return DoNodeAction(elem);
}

IBaseSimInstance *TreeSim::GetISimInstance()
{
    return mISimInstance ? mISimInstance->GetISimInstance() : 0;
}

int TreeSim::Gosub(Behavior *behavior, const s16 *params, s16 treeID)
{
    StackElem frame;
    StackElem *top = mStack.GetNthFrame(mStack.GetStackSize() - 1);
    if (behavior == 0)
        behavior = top->mBehavior;
    if (behavior == 0)
        return 0;

    ConsoleAutoRefCount<ObjectDataBehaviorTree> tree;
    if (behavior->GetTree(treeID, tree.AsPPTypeParam(), false) == 0)
        return 0;

    frame.mBehavior = behavior;
    frame.mWaiting = 0;
    frame.SetTreeID(treeID);
    if (top != 0)
        frame.field_0x4 = top->field_0x4;
    else
        frame.field_0x4 = 0;
    if (params == 0)
        params = mDefaultParams;

    // A type 4 tree starts at the node its first parameter names, clamped to the last node.
    if (tree->GetType() == 4) {
        frame.mNodeIndex = params[0];
        if (tree->GetNumNodes() <= frame.GetNodeIndex())
            frame.mNodeIndex = tree->GetNumNodes() - 1;
    } else {
        frame.mNodeIndex = 0;
    }
    if (frame.GetNodeIndex() < 0 || tree->GetNumNodes() <= frame.GetNodeIndex())
        return 0;

    frame.mNumLocals = tree->GetNumLocals();
    frame.mNumParams = tree->GetNumParams();
    mStack.Push(&frame, params);
    return 1;
}

int TreeSim::DoNodeAction(StackElem *elem)
{
    if (elem == 0) {
        if (HasMainTreeSim()) {
            TREE_ERROR(0x44e);
            return 2;
        }
        return 1;
    }
    if (elem->mBehavior == 0) {
        TREE_CHECK(this, mStack.GetStackSize() >= 2, 0x3e9, return 2);
        mStack.Pop();
        StackJustPopped();
        return 0;
    }
    if ((u16)elem->mNodeIndex < 0xfb) {
        BehaviorNode *node = 0;
        int found = elem->mBehavior->GetNodeRef(elem->GetTreeID(), elem->mNodeIndex, &node);
        TREE_CHECK(this, found, 0x44e, return 2);
        mIterations++;
        TREE_CHECK(this, mIterations < sMaxIterations, 0x44d, return 2);
        if (node->GetPrimCode() >= 0x100)
            return DoNodeActionNormal(elem, node);
        return DoNodeActionPrimitive(elem, node);
    }
    return DoNodeActionTransition(elem);
}

#ifdef NON_MATCHING
// One word of 66 differs. After the Error call the game has beql where this has beq, before the
// destructor's Release call. The delay slot holds the same instruction, return value 2, so
// behaviour is the same. It says the game's compiler thought v0 was needed on the fall-through
// path; what in the source makes it think so isn't known.
int TreeSim::DoNodeActionNormal(StackElem *elem, BehaviorNode *node)
{
    const s16 *params = 0;
    if (!Behavior::IsDefaultParam(node->mParams))
        params = node->mParams;
    if (Gosub(0, params, node->GetTreeID()))
        return 0;

    ConsoleAutoRefCount<ObjectDataBehaviorTree> tree;
    if (elem->mBehavior->GetTree(node->GetTreeID(), tree.AsPPTypeParam(), true)) {
        TREE_ERROR_LOOP(0x3e8);
        return 2;
    }
    TREE_ERROR_LOOP(0x44e);
    return 2;
}
#endif

int TreeSim::DoNodeActionPrimitive(StackElem *elem, BehaviorNode *node)
{
    switch (TryElement(elem, node->GetPrimCode(), (XPrimParam *)node->mParams)) {
    case 1:
        TREE_CHECK(this, node->mTrueTarget != 0xfd, 0x44c, return 2);
        elem->mNodeIndex = node->mTrueTarget;
        mLastTransition = node->mTrueTarget;
        mLastResult = 1;
        elem->mWaiting = 0;
        return 0;
    case 0:
        TREE_CHECK(this, node->mFalseTarget != 0xfd, 0x44c, return 2);
        elem->mNodeIndex = node->mFalseTarget;
        mLastTransition = node->mFalseTarget;
        mLastResult = 0;
        elem->mWaiting = 0;
        return 0;
    case 2:
        return 1;
    case -1:
        return 2;
    case 3:
        return 0;
    case 4:
        return 2;
    }
    return 2;
}

int TreeSim::DoNodeActionTransition(StackElem *elem)
{
    s16 transition = elem->mNodeIndex;
    switch (transition) {
    case 0xfd:
        TREE_ERROR(0x44c);
        return 2;
    case 0xfe:
    case 0xff: {
        TREE_CHECK(this, mStack.GetStackSize() >= 2, 0x3e9, return 2);
        mStack.Pop();
        StackJustPopped();
        StackElem *caller = GetCurElem();
        if (transition == 0xfe)
            mLastResult = 1;
        else
            mLastResult = 0;
        if (caller->mWaiting != 0)
            return 0;
        BehaviorNode node;
        int found = caller->mBehavior->GetNode(caller->GetTreeID(), caller->mNodeIndex, &node);
        BehaviorNode *result = 0;
        TREE_CHECK(this, found, 0x44e, return 2);
        if (found)
            result = &node;
        if (transition == 0xfe) {
            TREE_CHECK(this, result->mTrueTarget != 0xfd, 0x44c, return 2);
            caller->mNodeIndex = result->mTrueTarget;
            mLastTransition = result->mTrueTarget;
        } else {
            TREE_CHECK(this, result->mFalseTarget != 0xfd, 0x44c, return 2);
            caller->mNodeIndex = result->mFalseTarget;
            mLastTransition = result->mFalseTarget;
        }
        return 0;
    }
    }
    TREE_ERROR(0x44c);
    return 2;
}

StackElem *TreeSim::GetMainSimElem()
{
    int found = 0;
    int last = mStack.GetStackSize() - 1;
    for (int i = 0; i <= last; i++) {
        if (GetNthElem(i)->GetTreeID() == -1) {
            last = i - 1;
            break;
        }
    }
    if (last == -1) {
        HasMainTreeSim();
        return 0;
    }
    return GetNthElem(last);
}

StackElem *TreeSim::GetHighLevelAction()
{
    s16 i = GetStackSize() - 2;
    Behavior *behavior = GetCurElem()->mBehavior;
    for (; i >= 0; i--) {
        if (GetNthElem(i)->mBehavior != behavior)
            return GetNthElem(i + 1);
    }
    return GetNthElem(0);
}

int TreeSim::RunCheckTree(Behavior *behavior, s16 a, s16 treeID, s16 *params)
{
    if (mError != 0)
        return 0;

    // A frame with no behaviour marks the bottom of the check, so the loop below knows when
    // the tree has returned.
    StackElem frame;
    frame.mWaiting = 1;
    frame.mNodeIndex = 0;
    frame.mBehavior = 0;
    frame.SetTreeID(-1);
    frame.field_0x4 = 0;
    frame.mNumLocals = 0;
    frame.mNumParams = 0;
    mStack.Push(&frame, 0);

    if (Gosub(behavior, params, treeID)) {
        GetCurElem()->field_0x4 = a;
        int inMainSim = sInMainSim;
        sInMainSim = 0;
        int iterations = mIterations;
        f32 saved = field_0x1c;
        if (ObjectAsTreeSim()->GetError() != 0)
            ObjectAsTreeSim()->ClearError();
        mIterations = 0;
        field_0x1c = 0.0f;
        StackElem *elem;
        while ((elem = GetCurElem())->GetTreeID() != -1) {
            switch (DoNodeAction(elem)) {
            case 0:
                break;
            case 1:
            case 2:
                TREE_CHECK(this, ObjectAsTreeSim()->GetError() != 0, 0x44f, return 0);
                mObject->HandleError();
                return 0;
            }
        }
        elem->GetTreeID();
        mStack.Pop();
        StackJustPopped();
        int result = GetLastTransition();
        mIterations = iterations;
        field_0x1c = saved;
        sInMainSim = inMainSim;
        return result;
    }
    if (GetCurElem() != 0)
        GetCurElem()->GetTreeID();
    mStack.Pop();
    StackJustPopped();
    return 0;
}
