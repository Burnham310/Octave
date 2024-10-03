#pragma once

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_DEVISION 480 // ticks per quarter note
#define DEFAULT_VELOCITY 64  // volumn
#define DEFAULT_CHANNEL 0


// https://en.wikipedia.org/wiki/Mode_(music)


struct _MidiEvent
{
    enum MidiEventType
    {
        _NoteOnEvent,
        _NoteOffEvent,
    } eventType;
    int delta_time;
    void *data;
    void (*callbacks[5])(int delta_time, void *data);
    void (*destroyer)(void *data);
};

#define _get_array_len(arr) sizeof(arr) / sizeof(arr[0])
#define NoteLenRatio(note_length) note_length / 100

struct _MidiTrack
{
    size_t cap;
    size_t event_count;
    struct _MidiEvent *events;
};

typedef enum
{
    WHOLE_NOTE,
    HALF_NOTE,
    QUARTER_NOTE,
    EIGHTH_NOTE,
    SIXTEENTH_NOTE,
} MidiNoteLength;

#define NoteLenRatio(note_length) note_length / 100

typedef struct
{
    unsigned char pitch;
    MidiNoteLength length;
    unsigned char velocity; // volume
    unsigned char channel;
} MidiNote;

typedef enum
{
    CMAJOR,
    CMINOR,
    DMAJOR,
    DMINOR,
    EMAJOR,
    EMINOR,

    ERRSCALE = -1,
} MidiScaleType;

// initialize the midi backend
void init_midi_backend(FILE *fp);

// add event to the midi track, e.g. NoteOn_event, NoteOff_event
void add_midi_event(struct _MidiEvent event);

// dump all events to the file
void dump_midi_to_file();

// free the backend
void free_midi_backend();

// note on event, must follow with the noteoff event with same note specified
struct _MidiEvent NoteOnEvent(MidiScaleType scaleType, MidiNote *note);

// note off event
struct _MidiEvent NoteOffEvent(MidiScaleType scaleType, MidiNote *note);
