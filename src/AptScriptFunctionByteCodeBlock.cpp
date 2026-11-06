#include "AptScriptFunctionByteCodeBlock.h"

u32 AptScriptFunctionByteCodeBlock::GetName() const
{
    return field_0x3c;
}

int AptScriptFunctionByteCodeBlock::GetNumArguments()
{
    return 0;
}

u32 AptScriptFunctionByteCodeBlock::GetByteCodeBase()
{
    return field_0x34;
}

u32 AptScriptFunctionByteCodeBlock::GetByteCodeSize()
{
    return field_0x38;
}

void AptScriptFunctionByteCodeBlock::SetArgument(AptValue *a, int b)
{
}

int AptScriptFunctionByteCodeBlock::Duplicate(AptCIH *a)
{
    return 0;
}
