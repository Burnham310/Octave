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
    // trigger interpolate mapper function
    for (int i = arrlen(hmgets(iFuncMap, track_id).ifuncs) - 1; i >= 0; --i)
    {
        int duration = hmgets(iFuncMap, track_id).duration;
        int note_c = hmgets(iFuncMap, track_id).ifuncs_status[i];

        switch (hmgets(iFuncMap, track_id).eventType)
        {
        case _SetVolumeEvent:
            float mapper_val = hmgets(iFuncMap, track_id).ifuncs[i](note_c, duration);

            add_midi_event(track_id, SetVolumeRatioEvent(mapper_val));
            break;

        default:
            midi_assert(0, midi_printf("iFunc event not support"));
            break;
        }

        if (note_c + 1 >= duration)
        {
            arrdel(hmgets(iFuncMap, track_id).ifuncs_status, i);
            arrdel(hmgets(iFuncMap, track_id).ifuncs, i);
        }
        else
            hmgets(iFuncMap, track_id).ifuncs_status[i] = note_c + 1;
    }

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
    arrput(hmgets(iFuncMap, track_id).ifuncs, wrapper);
    arrput(hmgets(iFuncMap, track_id).ifuncs_status, 1);
    hmgets(iFuncMap, track_id).eventType = event;
    hmgets(iFuncMap, track_id).duration = duration;
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
