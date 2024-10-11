#pragma once

#include <stdio.h>
#include <stdlib.h>
#define DEFAULT_DEVISION 120 // ticks per quarter note
#define DEFAULT_VOLUME 80
#define DEFAULT_VELOCITY 64 // Moderate press

typedef enum
{
    WHOLE_NOTE,
    HALF_NOTE,
    QUARTER_NOTE,
    EIGHTH_NOTE,
    SIXTEENTH_NOTE,
} MidiNoteLength;

typedef struct
{
    unsigned char pitch;
    MidiNoteLength length;
    unsigned char velocity; // volume
} MidiNote;

typedef struct
{
    int volume;
    int devision;
    int track_n;
} MidiConfig;

#define midi_printf(fmt, ...) printf("[MidiBackend] " fmt "\n", ##__VA_ARGS__);

enum MTrkEventType
{
    _NoteOnEvent,
    _NoteOffEvent,
    _SetTempoEvent,
    _SetInstrumentEvent,
    _SetVolumeEvent,
    _SetVolumeRatioEvent,
    _BufUpdateEvent,
};

struct _EventData
{
    int track_id;
    unsigned char channel;
    int nidx;
    int nt;
};

struct _MTrkEvent
{
    enum MTrkEventType eventType;
    int delta_time;
    void *data;
    void (*callbacks[5])(int delta_time, struct _EventData, void *data);
    void (*destroyer)(void *data);
};

#define TRACK1 0
#define TRACK2 1
#define TRACK3 2
#define TRACK4 3
#define TRACK5 4
#define TRACK6 5

#define GLOBAL 114514

#define _get_array_len(arr) sizeof(arr) / sizeof(arr[0])
#define NoteLenRatio(note_length) note_length / 100

#define midi_assert(bool, action) \
    do                            \
    {                             \
        if (!(bool))              \
        {                         \
            action;               \
            exit(0);              \
        }                         \
    } while (0);

#define EVENT_CALLBACK(event_name) static void callback_##event_name(int delta_time, struct _EventData event_data, void *data)
#define USE_CALLBACK(event_name) {&callback_##event_name}

struct MidiTrackMeta
{
    int delta_time_dely_buf;
    int volume;
};

struct _MidiTrack
{
    size_t cap;
    size_t event_count;
    int channel;
    struct MidiTrackMeta meta;
    struct _MTrkEvent *events;
};

#define NoteLenRatio(note_length) note_length / 100

// initialize the midi backend, set config to NULL to use default
void init_midi_backend(FILE *fp, MidiConfig *config);
// add event to the midi track, e.g. NoteOn_event, NoteOff_event
void add_midi_event(int track_id, struct _MTrkEvent event);
// dump all events to the file
void dump_midi_to_file();
// free the backend
void free_midi_backend();
// get metadata for track
struct MidiTrackMeta *get_midi_tr_meta(int track_id);

/*
    MTrk Events <MTrk event>:

    formal definition: <MTrk event> = <delta-time><event>
    for those event that start with *, means they are gloabl event

    description:
    - NoteOnEvent: key press
    - NoteOffEvent: key down
    - *SetTempoEvent: change current tempo(bpm)
    - SetInstrumentEvent: change instrument, pc refer to midi document
    - SetVolumeEvent: set volume
    - SetVolumeRatioEvent: set volume to ratio wrt current volume
*/

#define EVENT_DECLARE(event_name, ...) struct _MTrkEvent event_name(__VA_ARGS__)

// note on event, must follow with the noteoff event with same note specified
EVENT_DECLARE(NoteOnEvent, MidiNote *note);
// note off event, If the first event in a track occurs at the very beginning of a track
// or if two events occur simultaneously, a delta-time of zero is used, if set force_immed
// equal to true, note off triggered immediately
EVENT_DECLARE(NoteOffEvent, MidiNote *note, int force_immed);
// pause note
EVENT_DECLARE(PauseNoteEvent, MidiNoteLength length);
// change tempo (event) of current track
EVENT_DECLARE(SetTempoEvent, int bpm);
// change instrument of current track
EVENT_DECLARE(SetInstrumentEvent, int pc);
// change volume
EVENT_DECLARE(SetVolumeEvent, int volume);
// set volume to ratio wrt current volume
EVENT_DECLARE(SetVolumeRatioEvent, float ratio);
