#include "backend.h"
#include <string.h>

static FILE *midi_fp;
static struct _MidiTrack midi_track;

// write a single byte
static void write_byte(unsigned char byte)
{
    fputc(byte, midi_fp);
}

// write two bytes (big endian)
static void write_word(unsigned short word)
{
    write_byte((word >> 8) & 0xFF);
    write_byte(word & 0xFF);
}

// write four bytes (big endian)
static void write_dword(unsigned int dword)
{
    write_byte((dword >> 24) & 0xFF);
    write_byte((dword >> 16) & 0xFF);
    write_byte((dword >> 8) & 0xFF);
    write_byte(dword & 0xFF);
}

// write a variable length value (used for delta times in MIDI)
static void write_var_len(unsigned int value)
{
    unsigned char buffer[4];
    int index = 0;

    do
    {
        buffer[index++] = value & 0x7F;
        value >>= 7;
    } while (value > 0);

    for (int i = index - 1; i >= 0; i--)
    {
        if (i > 0)
        {
            buffer[i] |= 0x80;
        }
        write_byte(buffer[i]);
    }
}

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

static void NoteOnEvent_callback(int delta_time, void *data)
{
    MidiNote *note = (MidiNote *)data;

    write_var_len(delta_time);
    write_byte(0x90 | (note->channel & 0x0F)); // Note On event
    write_byte(note->pitch);
    write_byte(note->velocity);
}

static void NoteOffEvent_callback(int delta_time, void *data)
{
    MidiNote *note = (MidiNote *)data;

    write_var_len(delta_time);
    write_byte(0x80 | (note->channel & 0x0F));
    write_byte(note->pitch);    // MIDI pitch
    write_byte(note->velocity); // Velocity (volume)
}

static const char *note_event_to_str(int MidiEventType)
{
    switch (MidiEventType)
    {
    case 0:
        return "NoteOnEevent";
    case 1:
        return "NoteOffEvent";
    default:
        return "UnknownEvent";
    }
}

static int note_length(MidiNoteLength l)
{
    int d = 400;
    for (int i = 0; i < l; i++)
    {
        d /= 2;
    }
    return d;
}

struct _MidiEvent NoteOnEvent(MidiScaleType scaleType, MidiNote *note)
{
    MidiNote *note_copy = (MidiNote *)malloc(sizeof(MidiNote));
    *note_copy = *note;
    note_copy->pitch = MIDI_SCALES[scaleType][note_copy->pitch - 1];

    struct _MidiEvent event =
        {
            .eventType = _NoteOnEvent,
            .callbacks = {
                &NoteOnEvent_callback,
            },
            .delta_time = 0,
            .data = note_copy,
            .destroyer = free,
        };
    return event;
}

struct _MidiEvent NoteOffEvent(MidiScaleType scaleType, MidiNote *note)
{
    MidiNote *note_copy = (MidiNote *)malloc(sizeof(MidiNote));
    *note_copy = *note;
    note_copy->pitch = MIDI_SCALES[scaleType][note_copy->pitch - 1];

    struct _MidiEvent event =
        {
            .eventType = _NoteOffEvent,
            .callbacks = {
                &NoteOffEvent_callback,
            },
            .delta_time = DEFAULT_DEVISION * NoteLenRatio(note_length(note->length)),
            .data = note_copy,
            .destroyer = free,
        };
    return event;
}

void init_midi_backend(FILE *fp)
{
    midi_fp = fp;

    // init track
    midi_track.events = malloc(sizeof(struct _MidiEvent) * 5);
    midi_track.event_count = 0;
    midi_track.cap = 5;

    // write head chunk
    fwrite("MThd", 1, 4, midi_fp); // header tag
    write_dword(6);                // always 6
    write_word(0);                 // Format Type
    write_word(1);                 // Number of Tracks
    write_word(DEFAULT_DEVISION);  // ticks per quarter note
}

void add_midi_event(struct _MidiEvent event)
{
    if (midi_track.event_count == midi_track.cap)
    {
        midi_track.events = realloc(midi_track.events, sizeof(struct _MidiEvent) * (midi_track.cap * 2));
        midi_track.cap *= 2;
    }

    midi_track.events[midi_track.event_count++] = event;
}

void dump_midi_to_file()
{
    printf("[MidiBackend] begin dumping.\n");

    fwrite("MTrk", 1, 4, midi_fp);          // Chunk type
    long track_length_pos = ftell(midi_fp); // Write a placeholder for the track length (we'll go back and fill this in later)
    write_dword(0);                         // Placeholder

    printf("[MidiBackend] write track chunk tag.\n");
    printf("[MidiBackend] prepare %zu event.\n", midi_track.event_count);

    for (size_t i = 0; i < midi_track.event_count; ++i)
    {
        struct _MidiEvent event = midi_track.events[i];
        for (size_t i = 0; i < _get_array_len(event.callbacks); ++i)
        {
            if (event.callbacks[i])
                event.callbacks[i](event.delta_time, event.data);
            else
                break;
        }
    }
    printf("[MidiBackend] finish events writing.\n");

    // end of track
    write_var_len(0);
    write_byte(0xFF);
    write_byte(0x2F);
    write_byte(0x00);

    // back and write the correct track length
    long end_of_file_pos = ftell(midi_fp);
    fseek(midi_fp, track_length_pos, SEEK_SET);
    write_dword(end_of_file_pos - track_length_pos - 4); // ignore length field
    fseek(midi_fp, end_of_file_pos, SEEK_SET);
    printf("[MidiBackend] write EOT tag.\n");

    printf("[MidiBackend] all done\n");
}

void free_midi_backend()
{
    if (midi_track.events)
    {
        for (size_t i = 0; i < midi_track.event_count; ++i)
        {
            if (midi_track.events[i].destroyer && midi_track.events[i].data)
                midi_track.events[i].destroyer(midi_track.events[i].data);
        }
        free(midi_track.events);
    }
    midi_track.event_count = 0;
    midi_track.events = NULL;
}
