#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lexer.h"
#include "ast.h"
#include "midi.h"
#include "parser.h"
#include "backend.h"
#include "utils.h"
#include "sema.h"
#include "compiler.h"
void usage(const char *prog_name)
{
    eprintf("Usage: %s [input_file] -o [output_file]\n", prog_name);
}
char *input_path, *output_path;
FILE *input_f;
FILE *output_f;
char *buf;
Pgm pgm;
Context ctx;
#define RETURN(x)      \
    do                 \
    {                  \
        exit_code = x; \
        goto out;      \
    } while (0)
extern int main(const int argc, char **argv)
{
    int exit_code = 0;
    if (argc != 4)
        RETURN(1);
    input_path = argv[1];
    if (strcmp(argv[2], "-o") != 0)
        RETURN(2);
    output_path = argv[3];

    if ((input_f = fopen(input_path, "r")) == NULL)
    {
        perror("Cannot open src file");
        RETURN(1);
    }
    if (strcmp(output_path, "-") == 0)
        output_f = stdout;
    else if ((output_f = fopen(output_path, "w")) == NULL)
    {
        perror("Cannot create output file");
        RETURN(1);
    }

    // get size of input buffer
    fseek(input_f, 0, SEEK_END);
    size_t input_size = ftell(input_f);
    fseek(input_f, 0, SEEK_SET);
    buf = malloc(input_size);
    eprintf("Reading input file of size: %zu\n", input_size);
    if (fread(buf, input_size, 1, input_f) == 0)
    {
        perror("Cannot read src file");
    }
    // lexing
    Lexer lexer = lexer_init(buf, input_size, input_path);
    // parsing
    pgm = parse_ast(&lexer);
    if (!pgm.success)
    {
        eprintf("Failed to parse ast\n");
        RETURN(1);
    }
    // semantic analysis
    sema_analy(&pgm, &lexer, &ctx);
    if (!ctx.success)
    {
        eprintf("typechecking fails\n");
        RETURN(1);
    }
    SliceOf(Track) tracks = eval_pgm(&ctx);

    // backend initialization
    init_backend(output_f, &(MidiConfig){.devision = 120, .track_n = tracks.len, .volume = 100});

    // add track
    for (size_t ti = 0; ti < tracks.len; ++ti)
    {
        Track *track = &tracks.ptr[ti];
	
        // printf("instr %i\n", track->config.instr);
        add_midi_event(ti, SetInstrumentEvent(track->config.instr));
        add_midi_event(GLOBAL, SetTempoEvent(track->config.bpm));

        size_t label_c = 0;
	eprintf("note size %zu\n", track->notes.size);
        for (size_t ni = 0; ni < track->notes.size; ++ni)
        {
            if (label_c < track->labels.len && track->labels.ptr[label_c].note_pos == ni)
            {
                int is_last_label = label_c == track->labels.len - 1;

                Label cur_label = track->labels.ptr[label_c];
                int duration = track->notes.size - cur_label.note_pos;
                if (!is_last_label)
                {
                    duration = track->labels.ptr[label_c + 1].note_pos - cur_label.note_pos;
                }
                // midi_printf("add iFunc, volumne %d", cur_label.volume);
                add_iFunc(
                    ti,
                    _SetVolumeEvent,
                    cur_label.volume,
                    is_last_label ? cur_label.volume : track->labels.ptr[label_c + 1].volume,
                    duration,
                    cur_label.linear ? iFunc_linear : NULL);

                label_c++;
            }
            Note note = track->notes.items[ni];
            SliceOf(Pitch) pitches = note.chord;
            size_t dots = note.dots;
	    if (dots == 0) continue;
            if (pitches.len == 0)
            {
                add_midi_event(ti, PauseNoteEvent(NLF(dots)));
                continue;
            }

            MidiNote midi_notes[pitches.len];
            for (size_t mi = 0; mi < pitches.len; ++mi)
            {
                midi_notes[mi].length = NLF(dots);
                midi_notes[mi].pitch = resolve_pitch(&track->config.scale, pitches.ptr[mi]);
                midi_notes[mi].velocity = DEFAULT_VELOCITY;
            }
            add_midi_note(ti, CHORD(midi_notes));
        }
    }

    // dump events to file
    dump_midi_to_file();
    // free backend
    free_backend();
out:
    if (input_f)
        fclose(input_f);
    if (output_f)
        fclose(output_f);
    free(buf);
    if (exit_code != 0)
        usage(argv[0]);
    if (pgm.success)
        ast_deinit(&pgm);
    if (ctx.success)
        context_deinit(&ctx);
    // lexer_deinit(&lexer);
    return exit_code;
}
