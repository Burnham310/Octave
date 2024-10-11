#include "backend.h"
#include "midi.h"

#include "stb_ds.h"

typedef struct
{
    int key;
    float (*ifunc)(int idx, int n);
    int ifunc_status;
    enum MTrkEventType eventType;
    int duration;
} iFuncMap_s;

static iFuncMap_s *iFuncMap = NULL;

void init_backend(FILE *fp, MidiConfig *config)
{
    hmdefaults(iFuncMap, (iFuncMap_s){.key = -1});
    init_midi_backend(fp, config);
}

void _add_midi_note(int track_id, MidiNote *note, size_t note_n)
{
    float mapper_val = 1;
    iFuncMap_s target = hmgets(iFuncMap, track_id);

    if (target.key == -1)
        goto add_note_start;

    switch (target.eventType)
    {
    case _SetVolumeEvent:
        mapper_val = target.ifunc(target.ifunc_status, target.duration);
        // midi_printf("%f", mapper_val);
        add_midi_event(track_id, SetVolumeRatioEvent(mapper_val));
        break;

    default:
        midi_assert(0, midi_printf("iFunc event not support"));
        break;
    }

    if (++target.ifunc_status >= target.duration)
        hmdel(iFuncMap, track_id);
    else
        hmputs(iFuncMap, target);

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

    midi_assert(target.key == -1, midi_eprintf("duplicate register iFunc"));

    iFuncMap_s update = {
        .key = track_id,
        .duration = duration,
        .eventType = event,
        .ifunc = wrapper,
        .ifunc_status = 0,
    };
    hmputs(iFuncMap, update);
}

void free_backend()
{
    free_midi_backend();
    hmfree(iFuncMap);
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