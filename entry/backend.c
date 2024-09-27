#include "backend.h"
#include "string.h"

int main()
{
    FILE *fp = fopen("twinkle.mid", "w");

    unsigned char notes[] = {1, 1, 5, 5, 6, 6, 5}; // twinkle
    MidiScaleType scaleType = CMAJOR;

    init_midi_backend(fp);

    for (int i = 0; i < sizeof(notes) / sizeof(notes[0]) * 100; ++i)
    {
        MidiNote cur_note = {
            .channel = DEFAULT_CHANNEL,
            .length = EIGHTH_NOTE,
            .pitch = notes[i % 7],
            .velocity = DEFAULT_VELOCITY,
        };

        MidiNote chord_note = {
            .channel = DEFAULT_CHANNEL,
            .length = EIGHTH_NOTE,
            .pitch = notes[(6 - i) % 7],
            .velocity = DEFAULT_VELOCITY,
        };

        add_midi_event(NoteOnEvent(scaleType, &cur_note));
        add_midi_event(NoteOnEvent(scaleType, &chord_note));

        add_midi_event(NoteOffEvent(scaleType, &cur_note));
        add_midi_event(NoteOffEvent(scaleType, &chord_note));
    }

    dump_midi_to_file();
    free_midi_backend();

    fclose(fp);
}