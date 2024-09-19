#include <stdio.h>
#include <stdlib.h>

// Function to write a single byte
void write_byte(FILE *file, unsigned char byte)
{
    fputc(byte, file);
}

// Function to write two bytes (big endian)
void write_word(FILE *file, unsigned short word)
{
    write_byte(file, (word >> 8) & 0xFF);
    write_byte(file, word & 0xFF);
}

// Function to write four bytes (big endian)
void write_dword(FILE *file, unsigned int dword)
{
    write_byte(file, (dword >> 24) & 0xFF);
    write_byte(file, (dword >> 16) & 0xFF);
    write_byte(file, (dword >> 8) & 0xFF);
    write_byte(file, dword & 0xFF);
}

// Function to write a variable length value (used for delta times in MIDI)
void write_var_len(FILE *file, unsigned int value)
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

// Function to write a MIDI note on event
void write_note_on(FILE *file, unsigned char channel, unsigned char note, unsigned char velocity)
{
    write_byte(file, 0x90 | (channel & 0x0F)); // Note on event
    write_byte(file, note);                    // Note number
    write_byte(file, velocity);                // Velocity
}

// Function to write a MIDI note off event
void write_note_off(FILE *file, unsigned char channel, unsigned char note, unsigned char velocity)
{
    write_byte(file, 0x80 | (channel & 0x0F)); // Note off event
    write_byte(file, note);                    // Note number
    write_byte(file, velocity);                // Velocity
}

int main()
{
    FILE *file = fopen("twinkle_twinkle.mid", "wb");

    // MIDI header chunk
    fwrite("MThd", 1, 4, file);
    write_dword(file, 6);
    write_word(file, 1);
    write_word(file, 1);
    write_word(file, 480);

    // MIDI track chunk
    fwrite("MTrk", 1, 4, file); // Chunk type

    // Write a placeholder for the track length (we'll go back and fill this in later)
    long track_length_pos = ftell(file);
    write_dword(file, 0); // Placeholder

    // MIDI events
    unsigned char notes[] = {60, 60, 67, 67, 69, 69, 67}; // MIDI note numbers for C4, G4, A4
    int num_notes = sizeof(notes) / sizeof(notes[0]);

    for (int i = 0; i < num_notes; i++)
    {
        write_var_len(file, 0);              // Wait 480 ticks (1 quarter note)
        write_note_on(file, 0, notes[i], 64);  // Note on, channel 0, velocity 64
        write_var_len(file, 240);              // Wait 480 ticks (1 quarter note)
        write_note_off(file, 0, notes[i], 64); // Note off, channel 0, velocity 64
    }

    // End of track event
    write_var_len(file, 0); // Delta time
    write_byte(file, 0xFF); // Meta event
    write_byte(file, 0x2F); // End of track
    write_byte(file, 0x00); // Meta event length

    // Go back and write the correct track length
    long end_of_file_pos = ftell(file);
    fseek(file, track_length_pos, SEEK_SET);
    write_dword(file, end_of_file_pos - track_length_pos - 4); // Subtract 4 for the length field itself
    fseek(file, end_of_file_pos, SEEK_SET);

    fclose(file);

    printf("MIDI file 'twinkle_twinkle.mid' created successfully!\n");
    return 0;
}
