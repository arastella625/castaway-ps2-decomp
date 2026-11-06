#include "AptScriptFunction1.h"

u32 AptScriptFunction1::GetName() const
{
    return field_0x34->field_0x0;
}

u32 AptScriptFunction1::GetNumArguments()
{
    return field_0x34->field_0x4;
}

u32 AptScriptFunction1::GetByteCodeSize()
{
    return field_0x34->field_0xc;
}
