#include "AptValue.h"

int AptValue::GetNativeHashVirtual()
{
    return 0;
}

int AptValue::ContainsNativeHashVirtual() const
{
    return 0;
}

int AptValue::getHasClass() const
{
    return 0;
}

void AptValue::setHasClass(int a)
{
}

int AptValue::objectMemberLookup(AptValue *a, const EAStringC *b) const
{
    return 0;
}

int AptValue::objectMemberSet(AptValue *a, const EAStringC *b, AptValue *c)
{
    return 0;
}

void AptValue::PreDestroy()
{
}

void AptValue::DestroyGCPointers()
{
}
