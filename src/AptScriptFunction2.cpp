#include "AptScriptFunction2.h"

u32 AptScriptFunction2::GetName() const
{
    return field_0x34->field_0x0;
}

u32 AptScriptFunction2::GetNumArguments()
{
    return field_0x34->field_0x4;
}

u32 AptScriptFunction2::GetByteCodeSize()
{
    return field_0x34->field_0x10;
}
