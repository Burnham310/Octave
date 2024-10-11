#include "midi.h"
#include <stdio.h>

#define CHORD(note) note, _get_array_len(note)
#define NOTE(note) note, 1

#define add_midi_note(track_id, note) _add_midi_note(track_id, note)

void _add_midi_note(int track_id, MidiNote *note, size_t note_n);