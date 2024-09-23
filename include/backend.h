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

#define NoteLenRatio(note_length) note_length / 100

#define DEFAULT_DEVISION 480 // ticks per quarter note
#define DEFAULT_VELOCITY 64  // volumn
#define DEFAULT_CHANNEL 0

void init_midi_output(FILE* f); // init the MIDI output
void add_note_to_track(Track *track, MidiNote *note);
void close_midi_output(); // finalize and close the MIDI output
void write_track(Track *track);

void set_tempo(FILE *file, unsigned int bpm); // bpm
