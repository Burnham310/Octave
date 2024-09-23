#include "backend.h"

static FILE *midi_fp;

// write a single byte
static void write_byte(FILE *file, unsigned char byte)
{
    fputc(byte, file);
}

// write two bytes (big endian)
static void write_word(FILE *file, unsigned short word)
{
    write_byte(file, (word >> 8) & 0xFF);
    write_byte(file, word & 0xFF);
}

// write four bytes (big endian)
static void write_dword(FILE *file, unsigned int dword)
{
    write_byte(file, (dword >> 24) & 0xFF);
    write_byte(file, (dword >> 16) & 0xFF);
    write_byte(file, (dword >> 8) & 0xFF);
    write_byte(file, dword & 0xFF);
}

// write a variable length value (used for delta times in MIDI)
static void write_var_len(FILE *file, unsigned int value)
{
    unsigned char buffer[4];
    int index = 0;

    do
    {
        buffer[index++] = value & 0x7F;
        value >>= 7;
    } while (value > 0);

    for (int i = index - 1; i >= 0; i--)
    {
        if (i > 0)
        {
            buffer[i] |= 0x80;
        }
        write_byte(file, buffer[i]);
    }
}

void init_midi_output(FILE* fp)
{
    midi_fp = fp;
    // we will use fixed midi header at the begining
    fwrite("MThd", 1, 4, midi_fp);         // header tag
    write_dword(midi_fp, 6);               // always 6
    write_word(midi_fp, 1);                // Format Type
    write_word(midi_fp, 1);                // Number of Tracks
    write_word(midi_fp, DEFAULT_DEVISION); // ticks per quarter note
}

static void write_note_event(MidiNote *note)
{
    // fk
    write_var_len(midi_fp, 0);

    // Note On
    write_byte(midi_fp, 0x90 | (note->channel & 0x0F)); // Note On event
    write_byte(midi_fp, note->pitch);
    write_byte(midi_fp, note->velocity);

    write_var_len(midi_fp, DEFAULT_DEVISION * NoteLenRatio(note->length));
    // write_var_len(midi_fp, 480);

    // Note Off
    write_byte(midi_fp, 0x80 | (note->channel & 0x0F));
    write_byte(midi_fp, note->pitch);    // MIDI pitch
    write_byte(midi_fp, note->velocity); // Velocity (volume)
}

void add_note_to_track(Track *track, MidiNote *note)
{
    track->notes = realloc(track->notes, (track->note_count + 1) * sizeof(MidiNote));
    if (!track->notes)
    {
        perror("Failed to add note to track");
        exit(1);
    }

    track->notes[track->note_count++] = *note;
}

void write_track(Track *track)
{
    fwrite("MTrk", 1, 4, midi_fp);          // Chunk type
    long track_length_pos = ftell(midi_fp); // Write a placeholder for the track length (we'll go back and fill this in later)
    write_dword(midi_fp, 0);                // Placeholder

    // write note event
    for (int i = 0; i < track->note_count; ++i)
    {
        write_note_event(&track->notes[i]);
    }

    // end of track
    write_var_len(midi_fp, 0);
    write_byte(midi_fp, 0xFF);
    write_byte(midi_fp, 0x2F);
    write_byte(midi_fp, 0x00);

    // back and write the correct track length
    long end_of_file_pos = ftell(midi_fp);
    fseek(midi_fp, track_length_pos, SEEK_SET);
    write_dword(midi_fp, end_of_file_pos - track_length_pos - 4); // ignore length field
    fseek(midi_fp, end_of_file_pos, SEEK_SET);
}
// not used
void close_midi_output()
{
    fclose(midi_fp);
}

void set_tempo(FILE *file, unsigned int bpm)
{
    unsigned int tempo = (60000000 / bpm);

    write_byte(file, 0x00);

    write_byte(file, 0xFF);
    write_byte(file, 0x51); // Tempo ident
    write_byte(file, 0x03); // always 3 bytes

    write_dword(file, tempo);
}

int note_length(NoteLength l) {
    int d = 400;
    for (int i = 0; i < l; i++) {
	d /= 2;
    }
    return d;
}
