#include "backend.h"
#include "midi.h"

inline void init_backend(FILE *fp, MidiConfig *config)
{
    init_midi_backend(fp, config);
}

inline void free_backend()
{
    free_midi_backend();
}


void _add_midi_note(int track_id, MidiNote *note, size_t note_n)
{
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