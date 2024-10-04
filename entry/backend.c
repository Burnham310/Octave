#include "backend.h"
#include "string.h"

int main()
{
    FILE *fp = fopen("twinkle.mid", "w");

    unsigned char notes[] = {1, 1, 5, 5, 6, 6, 5}; // twinkle

    init_midi_backend(fp, &(MidiConfig){.volume = 100});

    add_midi_event(SetTempoEvent(120));
    add_midi_event(SetInstrument(41));
    for (int i = 0; i < sizeof(notes) / sizeof(notes[0]) * 100; ++i)
    {

        if (i == 10)
        {
            add_midi_event(SetTempoEvent(240));
            add_midi_event(SetInstrument(55));
        }

        MidiNote cur_note = {
            .channel = DEFAULT_CHANNEL,
            .length = EIGHTH_NOTE,
            .pitch = notes[i % 7] + 60,
            .velocity = DEFAULT_VELOCITY,
        };

        MidiNote chord_note = {
            .channel = DEFAULT_CHANNEL,
            .length = EIGHTH_NOTE,
            .pitch = notes[6 - (i % 7)] + 60,
            .velocity = DEFAULT_VELOCITY,
        };

        add_midi_event(NoteOnEvent(&cur_note));
        add_midi_event(NoteOnEvent(&chord_note));

        add_midi_event(NoteOffEvent(&cur_note));
        add_midi_event(NoteOffEvent(&chord_note));
    }

    dump_midi_to_file();
    free_midi_backend();

    fclose(fp);
}