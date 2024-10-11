#include "backend.h"
#include "string.h"

int main()
{
    FILE *fp = fopen("twinkle.mid", "w");

    unsigned char notes[] = {1, 1, 5, 5, 6, 6, 5}; // twinkle

    init_backend(fp, &(MidiConfig){.volume = 100, .devision = 120, .track_n = 2});
    add_midi_event(TRACK2, SetInstrumentEvent(28));
    add_midi_event(TRACK1, SetInstrumentEvent(37));

    add_midi_event(GLOBAL, SetTempoEvent(140));

    add_midi_event(TRACK2, SetVolumeEvent(80));
    add_midi_event(TRACK1, SetVolumeEvent(80));

    for (int i = 0; i < sizeof(notes); ++i)
    {
        MidiNote note = {
            .length = EIGHTH_NOTE,
            .pitch = notes[i] + 60,
            .velocity = DEFAULT_VELOCITY,
        };

        MidiNote note2 = {
            .length = EIGHTH_NOTE,
            .pitch = notes[i] + 75,
            .velocity = DEFAULT_VELOCITY,
        };

        MidiNote note3 = {
            .length = EIGHTH_NOTE,
            .pitch = notes[i] + 50,
            .velocity = DEFAULT_VELOCITY,
        };

        MidiNote chord[] = {note, note2};

        add_midi_note(TRACK1, CHORD(chord));
        add_midi_note(TRACK2, NOTE(&note3));
    }

    MidiNote note = {
        .length = EIGHTH_NOTE,
        .pitch = 43,
        .velocity = DEFAULT_VELOCITY,
    };

    add_midi_event(TRACK2, SetInstrumentEvent(12));
    add_midi_event(TRACK2, PauseNoteEvent(QUARTER_NOTE));
    add_midi_note(TRACK2, NOTE(&note));

    add_iFunc(TRACK1, _SetVolumeEvent, 80, 0, sizeof(notes), iFunc_linear);

    for (int i = 0; i < sizeof(notes); ++i)
    {
        MidiNote note = {
            .length = EIGHTH_NOTE,
            .pitch = notes[i] + 60,
            .velocity = DEFAULT_VELOCITY,
        };

        MidiNote note2 = {
            .length = EIGHTH_NOTE,
            .pitch = notes[i] + 75,
            .velocity = DEFAULT_VELOCITY,
        };

        MidiNote note3 = {
            .length = EIGHTH_NOTE,
            .pitch = notes[i] + 50,
            .velocity = DEFAULT_VELOCITY,
        };

        MidiNote chord[] = {note, note2};

        add_midi_note(TRACK1, CHORD(chord));
    }

    dump_midi_to_file();
    free_midi_backend();
    fclose(fp);
}