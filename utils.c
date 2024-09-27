#include "utils.h"
#include "string.h"

MidiScaleType str_to_MidiKeyType(const char *target, int len)
{
    const char *keys[] = {"cmajor", "cminor", "dmajor", "dminor", "emajor", "eminor"};

    for (int i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
    {
        if (strncmp(target, keys[i], len) == 0)
            return i;
    }

    return ERRSCALE;
}