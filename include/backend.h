#pragma once
#include "midi.h"
#include <stdio.h>

#define CHORD(note) note, _get_array_len(note)
#define NOTE(note) note, 1

void init_backend(FILE *fp, MidiConfig *config);
#define add_midi_note(track_id, note) _add_midi_note(track_id, note)
void add_iFunc(int track_id, enum MTrkEventType event, int start_d, int end_d, int duration, float (*wrapper)(int start, int end, int idx, int n));

void _add_midi_note(int track_id, MidiNote *note, size_t note_n);

void free_backend();

// buildin interpolator
float iFunc_linear(int start, int end, int idx, int n);
float iFunc_zoom(int start, int end, int idx, int n);