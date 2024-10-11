#include "backend.h"
#include "midi.h"

#include "stb_ds.h"

typedef struct
{
    int key;
    float (**ifuncs)(int idx, int n);
    int *ifuncs_status;
    enum MTrkEventType eventType;
    int duration;
} iFuncMap_s;

static iFuncMap_s *iFuncMap = NULL;

inline void init_backend(FILE *fp, MidiConfig *config)
{
    init_midi_backend(fp, config);
}

void _add_midi_note(int track_id, MidiNote *note, size_t note_n)
{
    float mapper_val = 1;
    iFuncMap_s target = hmgets(iFuncMap, track_id);

    if (!target.key)
        goto add_note_start;

    // trigger interpolate mapper function
    for (int i = arrlen(target.ifuncs) - 1; i >= 0; --i)
    {
        int duration = target.duration;
        int note_c = target.ifuncs_status[i];

        switch (target.eventType)
        {
        case _SetVolumeEvent:
            mapper_val = target.ifuncs[i](note_c, duration);

            add_midi_event(track_id, SetVolumeRatioEvent(mapper_val));
            break;

        default:
            midi_assert(0, midi_printf("iFunc event not support"));
            break;
        }

        if (note_c + 1 >= duration)
        {
            arrdel(target.ifuncs_status, i);
            arrdel(target.ifuncs, i);
        }
        else
            target.ifuncs_status[i] = note_c + 1;
    }

add_note_start:
    for (int i = 0; i < note_n; ++i)
    {
        add_midi_event(track_id, NoteOnEvent(note + i));
    }

    // add note off
    for (int i = 0; i < note_n; ++i)
    {
        add_midi_event(track_id, NoteOffEvent(note + i, i != 0));
    }
}

void add_iFunc(int track_id, enum MTrkEventType event, int duration, float (*wrapper)(int idx, int n))
{
    iFuncMap_s target = hmgets(iFuncMap, track_id);

    if (target.key)
    {
        arrput(target.ifuncs, wrapper);
        arrput(target.ifuncs_status, 1);
        hmgets(iFuncMap, track_id).eventType = event;
        hmgets(iFuncMap, track_id).duration = duration;
    }
}

void free_backend()
{
    free_midi_backend();

    for (int i = 0; i < hmlen(iFuncMap); ++i)
    {
        arrfree(iFuncMap[i].ifuncs);
        arrfree(iFuncMap[i].ifuncs_status);
    }
}

// sample interpolator
float iFunc_linear(int idx, int n)
{
    return (n - idx) / (float)n;
}

float iFunc_zoom(int idx, int n)
{
    float t = (float)idx / n;

    if (t >= 0.8)
        return 0;

    return (1.0f - t * t * t * t); // starts slow, then speeds up as idx approaches n
}