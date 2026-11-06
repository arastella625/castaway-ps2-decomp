#ifndef GAME_SIMULATOR_CORE_NGHRESFILE_H
#define GAME_SIMULATOR_CORE_NGHRESFILE_H

#include "common/types.h"

namespace Memory { class HandleNode; }
class StringBuffer;

// Generated from the Wii symbol names and the PS2 code of each function. Only the functions
// decompiled so far are declared, as plain member functions: the base class, the vtable order
// and the full layout are not known yet. Return types are guesses, and the fields are only
// the ones these functions touch. The names come from pkg/castaway_ps2_names.csv, mostly its
// vtable pairing tiers; a one-instruction body cannot confirm that a name is right.
class NghResFile {
public:
    int Delete(const StringBuffer &a);
    int Open(const StringBuffer &a);
    int CloseForReopen();
    int Reopen();
    int Close();
    void Update();
    int Writable();
    int ValidFile();
    int CountTypes();
    int GetIndType(s16 a);
    void GetName(Memory::HandleNode *a, StringBuffer &b);
    int GetResType(Memory::HandleNode *a);
    void GetIndex(Memory::HandleNode *a, s16 *b);
    void FindUniqueName(int a, StringBuffer &b);
    int FindUniqueID(int a);
    void Detach(Memory::HandleNode *a);
    void Load(Memory::HandleNode *a);
    bool IsLittleEndian(Memory::HandleNode *a);
    void SetID(Memory::HandleNode *a, s16 b);
    void Write(Memory::HandleNode *a);
    void Remove(Memory::HandleNode *a);
    void SetInfo(Memory::HandleNode *a, s16 b, const StringBuffer &c, char d);
};

#endif
