#pragma once

#include <stdio.h>
#include <stdlib.h>

typedef enum
{
    WHOLE_NOTE,
    HALF_NOTE,
    QUARTER_NOTE,
    EIGHTH_NOTE,
    SIXTEENTH_NOTE,
} NoteLength;

int note_length(NoteLength l);

typedef struct
{
    unsigned char pitch;
    NoteLength length;
    unsigned char velocity; // volume
    unsigned char channel;
} MidiNote;

typedef struct
{
    MidiNote *notes;
    int note_count;
} Track;

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

#define NoteLenRatio(note_length) note_length / 100

#define DEFAULT_DEVISION 480 // ticks per quarter note
#define DEFAULT_VELOCITY 64  // volumn
#define DEFAULT_CHANNEL 0

void init_midi_output(FILE *f); // init the MIDI output
void add_note_to_track(Track *track, MidiNote *note);
void close_midi_output(); // finalize and close the MIDI output
void write_track(Track *track);

void set_tempo(FILE *file, unsigned int bpm); // bpm
