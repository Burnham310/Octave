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

    return ERRSCALE;
}
