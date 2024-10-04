#include "backend.h"
#include <string.h>

static FILE *midi_fp;
static struct _MidiTrack midi_track;
static MidiConfig midi_config = {
    .devision = DEFAULT_DEVISION,
    .volume = DEFAULT_VOLUME,
};

static int delta_time_dely_buf = 0;

static inline int get_delta_time_dely()
{
    int tmp = delta_time_dely_buf;
    delta_time_dely_buf = 0;

    return tmp;
}

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

// write a variable length value
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

EVENT_CALLBACK(NoteOnEvent)
{
    MidiNote *note = (MidiNote *)data;

    write_var_len(delta_time + get_delta_time_dely());

    write_byte(0x90 | (note->channel & 0x0F)); // Note On event
    write_byte(note->pitch);
    write_byte(note->velocity);
}

struct _MTrkEvent NoteOnEvent(MidiNote *note)
{
    midi_assert(note->pitch >= 0 && note->pitch <= 127, midi_printf("bad note pitch value '%d'", note->pitch));
    midi_assert(note->velocity >= 0 && note->velocity <= 127, midi_printf("bad velocity '%d'", note->velocity));

    MidiNote *note_copy = (MidiNote *)malloc(sizeof(MidiNote));
    *note_copy = *note;

    struct _MTrkEvent event =
        {
            .eventType = _NoteOnEvent,
            .callbacks = USE_CALLBACK(NoteOnEvent),
            .delta_time = 0,
            .data = note_copy,
            .destroyer = free,
        };

    return event;
}

EVENT_CALLBACK(NoteOffEvent)
{
    MidiNote *note = (MidiNote *)data;

    write_var_len(delta_time);
    write_byte(0x80 | (note->channel & 0x0F));
    write_byte(note->pitch);    // MIDI pitch
    write_byte(note->velocity); // Velocity (volume)
}

struct _MTrkEvent NoteOffEvent(MidiNote *note)
{
    midi_assert(note->pitch >= 0 && note->pitch <= 127, midi_printf("bad note pitch value '%d'", note->pitch));

    MidiNote *note_copy = (MidiNote *)malloc(sizeof(MidiNote));
    *note_copy = *note;

    int note_length_f = 400;
    for (int i = 0; i < note->length; i++)
        note_length_f /= 2;

    struct _MTrkEvent event =
        {
            .eventType = _NoteOffEvent,
            .callbacks = USE_CALLBACK(NoteOffEvent),
            .delta_time = midi_config.devision * NoteLenRatio(note_length_f),
            .data = note_copy,
            .destroyer = free,
        };
    return event;
}

EVENT_CALLBACK(SetTempoEvent)
{

    int tempo_sec = *(int *)data;

    write_var_len(0);
    write_byte(0xFF);
    write_byte(0x51);
    write_byte(0x03);

    write_byte((tempo_sec >> 16) & 0xFF);
    write_byte((tempo_sec >> 8) & 0xFF);
    write_byte(tempo_sec & 0xFF);
}

struct _MTrkEvent SetTempoEvent(int bpm)
{
    midi_assert(bpm > 0, midi_printf("bpm should be greater than 0"));

    int *tempo_sec = malloc(sizeof(int));
    *tempo_sec = 60 * 1000000 / bpm;

    struct _MTrkEvent event =
        {
            .eventType = _SetTempoEvent,
            .callbacks = USE_CALLBACK(SetTempoEvent),
            .delta_time = 0,
            .data = tempo_sec,
            .destroyer = free,
        };

    return event;
}

EVENT_CALLBACK(SetInstrumentEvent)
{
    write_var_len(0);

    int pc = *(int *)data;

    write_byte(0xC0 + DEFAULT_CHANNEL);
    write_byte(pc);
}

struct _MTrkEvent SetInstrumentEvent(int pc)
{
    midi_assert(pc >= 1 && pc <= 128, midi_printf("invalid PC number '%d'", pc));

    int *pc_data = malloc(sizeof(int));
    *pc_data = pc;

    struct _MTrkEvent event =
        {
            .eventType = _SetInstrumentEvent,
            .callbacks = USE_CALLBACK(SetInstrumentEvent),
            .data = pc_data,
            .destroyer = free,
        };

    return event;
}

EVENT_CALLBACK(SetVolumeEvent)
{
    write_var_len(0);

    int volume = *(int *)data;

    write_byte(0xB0 + DEFAULT_CHANNEL);
    write_byte(0x07);
    write_byte(volume);
}

struct _MTrkEvent SetVolumeEvent(int volume)
{
    midi_assert(volume >= 1 && volume <= 128, midi_printf("invalid volume number '%d'", volume));

    int *volume_data = malloc(sizeof(int));
    *volume_data = volume;

    struct _MTrkEvent event =
        {
            .eventType = _SetVolumeEvent,
            .callbacks = USE_CALLBACK(SetVolumeEvent),
            .data = volume_data,
            .destroyer = free,
        };

    return event;
}

EVENT_CALLBACK(PauseNoteEvent)
{
    int note_length_f = *(int *)data;
    delta_time_dely_buf = midi_config.devision * NoteLenRatio(note_length_f);
}

struct _MTrkEvent PauseNoteEvent(MidiNoteLength length)
{

    int note_length_f = 400;
    for (int i = 0; i < length; i++)
        note_length_f /= 2;

    int *note_length_f_data = malloc(sizeof(int));
    *note_length_f_data = note_length_f;

    struct _MTrkEvent event =
        {
            .eventType = _BufUpdateEvent,
            .callbacks = USE_CALLBACK(PauseNoteEvent),
            .data = note_length_f_data,
            .destroyer = free,
        };

    return event;
}

void init_midi_backend(FILE *fp, MidiConfig *config)
{
    midi_fp = fp;

    // init track
    midi_track.events = malloc(sizeof(struct _MTrkEvent) * 5);
    midi_track.event_count = 0;
    midi_track.cap = 5;

    // write head chunk
    fwrite("MThd", 1, 4, midi_fp);                                                // header tag
    write_dword(6);                                                               // always 6
    write_word(0);                                                                // Format Type
    write_word(1);                                                                // Number of Tracks
    write_word(config && config->devision ? config->devision : DEFAULT_DEVISION); // ticks per quarter note

    config ? midi_config = *config : midi_config;
}

void add_midi_event(struct _MTrkEvent event)
{

    if (midi_track.event_count == midi_track.cap)
    {
        midi_track.events = realloc(midi_track.events, sizeof(struct _MTrkEvent) * (midi_track.cap * 2));
        midi_track.cap *= 2;
    }

    midi_track.events[midi_track.event_count++] = event;
}

void dump_midi_to_file()
{
    midi_printf("begin dumping.");

    fwrite("MTrk", 1, 4, midi_fp);          // Chunk type
    long track_length_pos = ftell(midi_fp); // Write a placeholder for the track length (we'll go back and fill this in later)
    write_dword(0);                         // Placeholder

    midi_printf("write track chunk tag.");
    midi_printf("prepare %zu event.", midi_track.event_count);

    // write config volume
    callback_SetVolumeEvent(0, &midi_config.volume);

    for (size_t i = 0; i < midi_track.event_count; ++i)
    {
        struct _MTrkEvent event = midi_track.events[i];
        for (size_t i = 0; i < _get_array_len(event.callbacks); ++i)
        {
            if (event.callbacks[i])
                event.callbacks[i](event.delta_time, event.data);
            else
                break;
        }
    }
    midi_printf("finish events writing.");

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

    midi_printf("all done");
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
