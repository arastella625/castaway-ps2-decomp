#ifndef GAME_SIMULATOR_CORE_OBJECTMODULE_H
#define GAME_SIMULATOR_CORE_OBJECTMODULE_H

#include "common/types.h"

class cXObject;

// Wii: ObjectModule. Holds every object in the world. Only what the decompiled code calls is
// declared so far.
class ObjectModule {
public:
    cXObject *GetObjectFromID(int id);
    void ClearIdleStatus(int id);
};

#endif
