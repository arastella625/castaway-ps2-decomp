#include "game/simulator/core/cXObject.h"

// Writes one operand back. ref is what InterpValue returned for it, so the scopes that are
// plain memory only have to store through it.
int cXObject::InterpWriteValue(s16 scope, s16 data, const s16 **ref, s16 value)
{
    switch (scope) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x05:
    case 0x06:
    case 0x08:
    case 0x09:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0d:
    case 0x12:
    case 0x13:
    case 0x16:
    case 0x18:
    case 0x19:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x1f:
    case 0x22:
    case 0x24:
    case 0x25:
    case 0x2e:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39:
    case 0x3a:
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f:
    case 0x40:
    case 0x41:
        *(s16 *)*ref = value;
        break;
    case 0x03: {
        EdithVariableSet *vars = &mVars;
        int found = vars->Contains(data);
        TREE_CHECK(this, found, 7, return -1);
        *ref = vars->WriteVar(data, value);
        break;
    }
    case 0x04: {
        cXObject *object = GetObjectFromID(GetCurElem()->mStackObject);
        if (object) {
            EdithVariableSet *vars = &object->mVars;
            int found = vars->Contains(data);
            TREE_CHECK(this, found, 7, return -1);
            *ref = vars->WriteVar(data, value);
            break;
        }
        TREE_CHECK(this, 0, 10, return -1);
    }
    default:
        TREE_ERROR(9);
        return -1;
    }
    return 1;
}
