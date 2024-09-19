#include "backend.h"

int main()
{
    unsigned char notes[] = {60, 60, 67, 67, 69, 69, 67}; // twinkle
    const char *output_file = "output.mid";

    init_midi_output("twinkle.mid");

    Track track = {
        .note_count = 0,
    };

    for (int i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i)
    {
        MidiNote cur_note = {
            .channel = DEFAULT_CHANNEL,
            .length = EIGHTH_NOTE,
            .pitch = notes[i],
            .velocity = DEFAULT_VELOCITY,
        };
        add_note_to_track(&track, &cur_note);
    }

    write_track(&track);
    close_midi_output();

    return 0;
}
