#include "game/misc/CTGFileImpl.h"

bool CTGFileImpl::IsWritable()
{
    return false;
}

int CTGFileImpl::Write(void *a, int b)
{
    return 0;
}

int CTGFileImpl::SetSize(int a)
{
    return 0;
}

int CTGFileImpl::Flush()
{
    return 0;
}

int CTGFileImpl::FlushCache()
{
    return 1;
}
