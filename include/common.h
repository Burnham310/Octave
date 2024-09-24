#pragma once

enum MidiScaleType
{
    CMAJOR,
    CMINOR,
    DMAJOR,
    DMINOR,
    EMAJOR,
    EMINOR,

    ERRSCALE = -1,

};

enum MidiScaleType getMidiKeyType(const char *target, int len);

static unsigned char MIDI_SCALES[][8] = {
    // C Major scale: C D E F G A B C
    {60, 62, 64, 65, 67, 69, 71, 72},

    // C Minor scale: C D Eb F G Ab Bb C
    {60, 62, 63, 65, 67, 68, 70, 72},

    // D Major scale: D E F# G A B C# D
    {62, 64, 66, 67, 69, 71, 73, 74},

    // D Minor scale: D E F G A Bb C D
    {62, 64, 65, 67, 69, 70, 72, 74},

    // E Major scale: E F# G# A B C# D# E
    {64, 66, 68, 69, 71, 73, 75, 76},

    // E Minor scale: E F# G A B C D E
    {64, 66, 67, 69, 71, 72, 74, 76},
};
