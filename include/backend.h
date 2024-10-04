#pragma once

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_DEVISION 120 // ticks per quarter note
#define DEFAULT_VOLUME 80
#define DEFAULT_CHANNEL 0

#define DEFAULT_VELOCITY 64 // Moderate press

struct _MTrkEvent
{
    enum MTrkEventType
    {
        _NoteOnEvent,
        _NoteOffEvent,
        _SetTempoEvent,
        _SetInstrumentEvent,
        _SetVolumeEvent,
        _BufUpdateEvent
    } eventType;
    int delta_time;
    void *data;
    void (*callbacks[5])(int delta_time, void *data);
    void (*destroyer)(void *data);
};

typedef struct
{
    int volume;
    int devision;
} MidiConfig;

#define _get_array_len(arr) sizeof(arr) / sizeof(arr[0])
#define NoteLenRatio(note_length) note_length / 100

#define midi_printf(fmt, ...) printf("[MidiBackend] " fmt "\n", ##__VA_ARGS__);
#define midi_assert(bool, action) \
    do                            \
    {                             \
        if (!(bool))              \
        {                         \
            action;               \
            exit(0);              \
        }                         \
    } while (0);

#define EVENT_CALLBACK(event_name) static void callback_##event_name(int delta_time, void *data)
#define USE_CALLBACK(event_name) {&callback_##event_name}

struct _MidiTrack
{
    size_t cap;
    size_t event_count;
    struct _MTrkEvent *events;
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

// initialize the midi backend, set config to NULL to use default
void init_midi_backend(FILE *fp, MidiConfig *config);

// add event to the midi track, e.g. NoteOn_event, NoteOff_event
void add_midi_event(struct _MTrkEvent event);

// dump all events to the file
void dump_midi_to_file();

// free the backend
void free_midi_backend();

/*
    MTrk Events <MTrk event>:

    formal definition: <MTrk event> = <delta-time><event>

    description:
    - NoteOnEvent: key press
    - NoteOffEvent: key down
    - SetTempoEvent: change current tempo(bpm)
    - SetInstrumentEvent: change instrument, pc refer to midi document
    - SetVolumeEvent: set volume
    -
*/

// note on event, must follow with the noteoff event with same note specified
struct _MTrkEvent NoteOnEvent(MidiNote *note);

// note off event, If the first event in a track occurs at the very beginning of a track
// or if two events occur simultaneously, a delta-time of zero is used
struct _MTrkEvent NoteOffEvent(MidiNote *note);

// pause note
struct _MTrkEvent PauseNoteEvent(MidiNoteLength length);

// change tempo (event) of current track
struct _MTrkEvent SetTempoEvent(int bpm);

// change instrument of current track
struct _MTrkEvent SetInstrumentEvent(int pc);

// change volume
struct _MTrkEvent SetVolumeEvent(int volume);
