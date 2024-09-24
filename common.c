#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "common.h"

enum MidiScaleType getMidiKeyType(const char *target, int len)
{
    if (strncmp(target, "cmajor", len) == 0)
        return CMAJOR;
    if (strncmp(target, "cminor", len) == 0)
        return CMINOR;
    if (strncmp(target, "dmajor", len) == 0)
        return DMAJOR;
    if (strncmp(target, "dminor", len) == 0)
        return DMINOR;
    if (strncmp(target, "emajor", len) == 0)
        return EMAJOR;
    if (strncmp(target, "eminor", len) == 0)
        return EMINOR;

    return ERRSCALE;
}
