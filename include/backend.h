#pragma once
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
    WHOLE_NOTE = 400,
    HALF_NOTE = 200,
    QUARTER_NOTE = 100,
    EIGHTH_NOTE = 50,
    SIXTEENTH_NOTE = 25
} NoteLength;

typedef struct
{
    unsigned char pitch;
    NoteLength length;
    unsigned char velocity; // volume
    unsigned char channel;
} Note;

typedef struct
{
    Note *notes;
    int note_count;
} Track;

#define NoteLenRatio(note_length) note_length / 100

#define DEFAULT_DEVISION 480 // ticks per quarter note
#define DEFAULT_VELOCITY 64  // volumn
#define DEFAULT_CHANNEL 0

void init_midi_output(const char *filename); // init the MIDI output
void add_note_to_track(Track *track, Note *note);
void close_midi_output(); // finalize and close the MIDI output
void write_track(Track *track);

void set_tempo(FILE *file, unsigned int bpm); // bpm