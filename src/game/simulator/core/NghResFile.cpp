#include "game/simulator/core/NghResFile.h"

int NghResFile::Delete(const StringBuffer &a)
{
    return 0;
}

int NghResFile::Open(const StringBuffer &a)
{
    return 0;
}

int NghResFile::CloseForReopen()
{
    return 0;
}

int NghResFile::Reopen()
{
    return 0;
}

int NghResFile::Close()
{
    return 0;
}

void NghResFile::Update()
{
}

int NghResFile::Writable()
{
    return 0;
}

int NghResFile::ValidFile()
{
    return 1;
}

int NghResFile::CountTypes()
{
    return 0;
}

int NghResFile::GetIndType(s16 a)
{
    return 0;
}

void NghResFile::GetName(Memory::HandleNode *a, StringBuffer &b)
{
}

int NghResFile::GetResType(Memory::HandleNode *a)
{
    return 0;
}

void NghResFile::GetIndex(Memory::HandleNode *a, s16 *b)
{
}

void NghResFile::FindUniqueName(int a, StringBuffer &b)
{
}

int NghResFile::FindUniqueID(int a)
{
    return 0;
}

void NghResFile::Detach(Memory::HandleNode *a)
{
}

void NghResFile::Load(Memory::HandleNode *a)
{
}

bool NghResFile::IsLittleEndian(Memory::HandleNode *a)
{
    return true;
}

void NghResFile::SetID(Memory::HandleNode *a, s16 b)
{
}

void NghResFile::Write(Memory::HandleNode *a)
{
}

void NghResFile::Remove(Memory::HandleNode *a)
{
}

void NghResFile::SetInfo(Memory::HandleNode *a, s16 b, const StringBuffer &c, char d)
{
}
