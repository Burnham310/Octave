#include "midi.h"
#include <string.h>

static FILE *midi_fp;
static struct _MidiTrack *midi_tracks;
static struct _MidiTrackGloEv midi_gloabl_ev;

static MidiConfig midi_config = {
    .devision = DEFAULT_DEVISION,
    .track_n = 1,
};

static inline int get_delta_time_dely(int track_id)
{
    int tmp = midi_tracks[track_id].meta.delta_time_dely_buf;
    midi_tracks[track_id].meta.delta_time_dely_buf = 0;

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

    write_var_len(delta_time + get_delta_time_dely(event_data.track_id));

    write_byte(0x90 | (event_data.channel & 0x0F)); // Note On event
    write_byte(note->pitch);
    write_byte(note->velocity);
}

EVENT_DECLARE(NoteOnEvent, MidiNote *note)
{
    midi_assert(note->pitch >= 0 && note->pitch <= 127, midi_eprintf("bad note pitch value '%d'", note->pitch));
    midi_assert(note->velocity >= 0 && note->velocity <= 127, midi_eprintf("bad velocity '%d'", note->velocity));

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
    write_byte(0x80 | (event_data.channel & 0x0F));
    write_byte(note->pitch);    // MIDI pitch
    write_byte(note->velocity); // Velocity (volume)
}

EVENT_DECLARE(NoteOffEvent, MidiNote *note, int force_immed)
{
    midi_assert(note->pitch >= 0 && note->pitch <= 127, midi_eprintf("bad note pitch value '%d'", note->pitch));

    MidiNote *note_copy = (MidiNote *)malloc(sizeof(MidiNote));
    *note_copy = *note;

    int note_length_f = 400;
    for (size_t i = 0; i < note->length; i++)
        note_length_f /= 2;

    struct _MTrkEvent event =
        {
            .eventType = _NoteOffEvent,
            .callbacks = USE_CALLBACK(NoteOffEvent),
            .delta_time = force_immed ? 0 : midi_config.devision * NoteLenRatio(note_length_f),
            .data = note_copy,
            .destroyer = free,
        };
    return event;
}

GLOBAL_CALLBACK(SetTempoEvent)
{
    (void)(event_data);
    (void)(delta_time);
    int tempo_sec = *(int *)data;

    write_var_len(0);
    write_byte(0xFF);
    write_byte(0x51);
    write_byte(0x03);

    write_byte((tempo_sec >> 16) & 0xFF);
    write_byte((tempo_sec >> 8) & 0xFF);
    write_byte(tempo_sec & 0xFF);
}

GLOBAL_DECLARE(SetTempoEvent, int bpm)
{
    midi_assert(bpm > 0, midi_eprintf("bpm should be greater than 0"));

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
    (void)(delta_time);
    write_var_len(0);

    int pc = *(int *)data;

    write_byte(0xC0 + event_data.channel);
    write_byte(pc);
}

EVENT_DECLARE(SetInstrumentEvent, int pc)
{
    midi_assert(pc >= 1 && pc <= 128, midi_eprintf("invalid PC number '%d'", pc));

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
    (void)(delta_time);
    write_var_len(0);

    int volume = *(int *)data;

    write_byte(0xB0 + event_data.channel);
    write_byte(0x07);
    write_byte(volume);

    midi_tracks[event_data.track_id].meta.volume = volume;
}

EVENT_DECLARE(SetVolumeEvent, int volume)
{
    midi_assert(volume >= 0 && volume <= 128, midi_eprintf("invalid volume number '%d'", volume));

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

EVENT_CALLBACK(SetVolumeRatioEvent)
{
    (void)(delta_time);
    write_var_len(0);

    float ratio = *(float *)data;
    int volume = get_midi_tr_meta(event_data.track_id)->volume * ratio;

    if (ratio <= 0)
    {
        volume = 1;
    }

    write_byte(0xB0 + event_data.channel);
    write_byte(0x07);
    write_byte(volume);

    // midi_printf("volume: %d %f", volume, ratio);
    get_midi_tr_meta(event_data.track_id)->volume = volume;
}

EVENT_DECLARE(SetVolumeRatioEvent, float ratio)
{

    float *ratio_data = malloc(sizeof(float));
    *ratio_data = ratio;

    struct _MTrkEvent event =
        {
            .eventType = _SetVolumeRatioEvent,
            .callbacks = USE_CALLBACK(SetVolumeRatioEvent),
            .data = ratio_data,
            .destroyer = free,
        };

    return event;
}

EVENT_CALLBACK(PauseNoteEvent)
{
    (void)(delta_time);
    int note_length_f = *(int *)data;
    get_midi_tr_meta(event_data.track_id)->delta_time_dely_buf += midi_config.devision * NoteLenRatio(note_length_f);
}

EVENT_DECLARE(PauseNoteEvent, MidiNoteLength length)
{

    int note_length_f = 400;
    for (unsigned int i = 0; i < length; i++)
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

struct MidiTrackMeta *get_midi_tr_meta(int track_id)
{
    return &midi_tracks[track_id].meta;
}

void init_midi_backend(FILE *fp, MidiConfig *config)
{
    midi_fp = fp;
    config ? midi_config = *config : midi_config;

    // init global events
    midi_gloabl_ev.events = malloc(sizeof(struct _MTrkEvent) * 5);
    midi_gloabl_ev.event_count = 0;
    midi_gloabl_ev.cap = 5;

    // init tracks
    midi_tracks = malloc(sizeof(struct _MidiTrack) * config->track_n);

    for (int i = 0; i < config->track_n; ++i)
    {
        midi_tracks[i].events = malloc(sizeof(struct _MTrkEvent) * 5);
        midi_tracks[i].event_count = 0;
        midi_tracks[i].cap = 5;
        midi_tracks[i].channel = i;
    }

    // write header
    fwrite("MThd", 1, 4, midi_fp);    // header tag
    write_dword(6);                   // always 6
    write_word(1);                    // Format Type
    write_word(config->track_n);      // Number of Tracks
    write_word(midi_config.devision); // ticks per quarter note
}

void add_midi_event(int track_id, struct _MTrkEvent event)
{
    midi_assert(track_id == GLOBAL || track_id < midi_config.track_n, midi_eprintf("no such track id"));

    midi_assert((track_id == GLOBAL) == (event.eventType == _SetTempoEvent),
                midi_eprintf("set invalid GLOABL event flag for: %d", event.eventType));

    // global event
    if (track_id == GLOBAL)
    {
        if (midi_gloabl_ev.event_count == midi_gloabl_ev.cap)
        {
            midi_gloabl_ev.events = realloc(midi_tracks[track_id].events, sizeof(struct _MTrkEvent) * (midi_gloabl_ev.cap * 2));
            midi_gloabl_ev.cap *= 2;
        }
        midi_gloabl_ev.events[midi_gloabl_ev.event_count++] = event;
        return;
    }

    // track event
    struct _MidiTrack *cur_track = &midi_tracks[track_id];
    if (cur_track->event_count == cur_track->cap)
    {
        cur_track->events = realloc(midi_tracks[track_id].events, sizeof(struct _MTrkEvent) * (cur_track->cap * 2));
        cur_track->cap *= 2;
    }

    cur_track->events[cur_track->event_count++] = event;
}

static void dump_gloabl_to_file()
{
    for (size_t i = 0; i < midi_gloabl_ev.event_count; ++i)
    {
        for (size_t j = 0; j < _get_array_len(midi_gloabl_ev.events[i].callbacks); ++j)
        {
            if (midi_gloabl_ev.events[i].callbacks[j])
            {
                midi_gloabl_ev.events[i].callbacks[j](
                    midi_gloabl_ev.events[i].delta_time,
                    (struct _EventData){},
                    midi_gloabl_ev.events[i].data);
            }
            else
                break;
        }
    }
}

static void dump_track_to_file(int track_id, int global_inj_flag)
{
    struct _MidiTrack cur_track = midi_tracks[track_id];

    midi_printf("dumping track id: %d (in c%d)", track_id, midi_tracks[track_id].channel);

    // write header
    fwrite("MTrk", 1, 4, midi_fp);          // Chunk type
    long track_length_pos = ftell(midi_fp); // Write a placeholder for the track length (we'll go back and fill this in later)
    write_dword(0);                         // Placeholder

    if (global_inj_flag)
    {
        midi_printf("dumping global event align to Track: %d", track_id);
        dump_gloabl_to_file();
    }

    midi_printf("prepare %zu event.", cur_track.event_count);

    int note_n = 0;
    int note_on_state = 0;

    int note_on_t = 0;
    int note_off_t = 0;

    // check track, and count note number for iFunc
    for (size_t i = 0; i < cur_track.event_count; ++i)
    {
        struct _MTrkEvent event = cur_track.events[i];

        // simple state machine with two states to count total notes in track
        if (event.eventType == _NoteOnEvent)
        {
            note_on_state = 1;
            note_on_t++;
        }
        else if (event.eventType == _NoteOffEvent)
        {
            midi_assert(note_on_state, midi_eprintf("bad midi, standalone NoteOff event (NoteOn: %d NoteOff: %d)", note_on_t, note_off_t));
            note_n++;
            note_off_t++;
            note_on_state = note_on_t > note_off_t;
        }
    }
    midi_assert(note_on_t == note_off_t, midi_eprintf("bad midi, NoteOn event diff than NoteOff Event"));

    // force writing global volume
    callback_SetVolumeEvent(0, (struct _EventData){.track_id = track_id}, &midi_config.volume);

    int cur_note_idx = 0;
    note_on_state = 0;

    for (size_t i = 0; i < cur_track.event_count; ++i)
    {
        struct _MTrkEvent event = cur_track.events[i];

        // calculate the number of note to update _EventData
        if (event.eventType == _NoteOnEvent)
        {
            note_on_state = 1;
        }
        else if (event.eventType == _NoteOffEvent)
        {
            note_on_state = note_on_t > note_off_t;
            if (!note_on_state)
                cur_note_idx++;
        }
        for (size_t j = 0; j < _get_array_len(event.callbacks); ++j)
        {
            if (event.callbacks[j])
                event.callbacks[j](
                    event.delta_time,
                    (struct _EventData){
                        .track_id = track_id,
                        .nidx = cur_note_idx,
                        .nt = note_n,
                        .channel = midi_tracks[track_id].channel,
                    },
                    event.data);
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
}

void dump_midi_to_file()
{
    midi_printf("begin dumping.");

    for (int i = 0; i < midi_config.track_n; ++i)
    {
        // inject track event and global event align to track 1
        dump_track_to_file(i, i == 0);
    }

    midi_printf("all done");
}

void free_midi_backend()
{
    for (int i = 0; i < midi_config.track_n; ++i)
    {
        struct _MidiTrack cur_track = midi_tracks[i];
        if (cur_track.events)
        {
            for (size_t j = 0; j < cur_track.event_count; ++j)
            {
                if (cur_track.events[j].destroyer && cur_track.events[j].data)
                    cur_track.events[j].destroyer(cur_track.events[j].data);
            }
            free(cur_track.events);
        }
        cur_track.event_count = 0;
        cur_track.events = NULL;
    }

    if (midi_gloabl_ev.events)
    {
        for (size_t j = 0; j < midi_gloabl_ev.event_count; ++j)
        {
            if (midi_gloabl_ev.events[j].destroyer && midi_gloabl_ev.events[j].data)
                midi_gloabl_ev.events[j].destroyer(midi_gloabl_ev.events[j].data);
        }
        free(midi_gloabl_ev.events);
    }
    midi_gloabl_ev.event_count = 0;
    midi_gloabl_ev.events = NULL;
}
