#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lexer.h"
#include "ast.h"
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
    if ((output_f = fopen(output_path, "w")) == NULL)
    {
        perror("Cannot create output file");
        RETURN(1);
    }
    // get size of input buffer
    fseek(input_f, 0, SEEK_END);
    size_t input_size = ftell(input_f);
    fseek(input_f, 0, SEEK_SET);
    buf = malloc(input_size);
    printf("Reading input file of size: %zu\n", input_size);
    if (fread(buf, input_size, 1, input_f) == 0)
    {
        perror("Cannot read src file");
    }
    // lexing
    Lexer lexer = lexer_init(buf, input_size, input_path);
    Lexer *dummy = &lexer;
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
    Decl main = pgm.decls.ptr[ctx.main];
    Sec main_sec = pgm.secs.ptr[main.sec];

    // backend initialization
    init_midi_backend(output_f, &(MidiConfig){.volume = 100});

    // default configuration

    // update configuration for section

    // add event to track

    SecConfig config = eval_config(&ctx, ctx.main);
    // eprintf("scale: tonic: %i mode: %i octave: %i\n", config.scale.tonic, config.scale.mode, config.scale.octave);
    // Scale test_scale = {.tonic = PTCH_C, .octave = 5, .mode = MODE_MAJ};
    // for (int i = 0; i < DIATONIC; ++i) {
    //     printf("%i -> %i\n", i+1, pitch_from_scale(&test_scale, i+1));
    // }
    for (int i = 0; i < main_sec.note_exprs.len; ++i)
    {
        Expr note_expr = ast_get(ctx.pgm, exprs, main_sec.note_exprs.ptr[i]);
	SliceOf(Pitch) pitches = eval_chord(&ctx, note_expr.data.note.expr, &config.scale);
	assert(pitches.len > 0);
	size_t dots = note_expr.data.note.dots;
	MidiNote first_note = {
	    .channel = DEFAULT_CHANNEL,
	    .length = dots,
	    .pitch = pitches.ptr[0],
	    .velocity = DEFAULT_VELOCITY,
	};
	add_midi_event(NoteOnEvent(&first_note));
	for (size_t p = 1; p < pitches.len; ++p) {
	    MidiNote midi_note = {
		.channel = DEFAULT_CHANNEL,
		.length = dots,
		.pitch = pitches.ptr[p],
		.velocity = DEFAULT_VELOCITY,
	    };
	    add_midi_event(NoteOnEvent(&midi_note));
	}
	add_midi_event(NoteOffEvent(&first_note));
	for (size_t p = 1; p < pitches.len; ++p) {
	    MidiNote midi_note = {
		.channel = DEFAULT_CHANNEL,
		.length = dots,
		.pitch = pitches.ptr[p],
		.velocity = DEFAULT_VELOCITY,
	    };
	    struct _MTrkEvent event = NoteOffEvent(&midi_note);
	    event.delta_time = 0;
	    add_midi_event(event);
	}
	// if (pitch_expr.tag == EXPR_NUM)

        // {
        //     MidiNote midi_note = {
        //         .channel = DEFAULT_CHANNEL,
        //         .length = expr.data.note.dots,
        //         .pitch = pitch_expr.data.num,
        //         .velocity = DEFAULT_VELOCITY,
        //     };
        //     add_midi_event(NoteOnEvent(midi_conf.scale, &midi_note));
        //     add_midi_event(NoteOffEvent(midi_conf.scale, &midi_note));
        // }
        // else if (pitch_expr.tag == EXPR_CHORD)
        // {
        //     size_t chord_len = pitch_expr.data.chord_notes.len;
        //     struct _MidiEvent note_off_buf[chord_len];
        //     for (size_t i = 0; i < chord_len; ++i)
        //     {
        //         AstIdx chrod_pitch_idx = pitch_expr.data.chord_notes.ptr[i];
        //         Expr chrod_pitch_expr = pgm.exprs.ptr[chrod_pitch_idx];
        // 	MidiNote midi_note = {
        // 	    .channel = DEFAULT_CHANNEL,
        // 	    .length = expr.data.note.dots,
        // 	    .pitch = chrod_pitch_expr.data.num,
        // 	    .velocity = DEFAULT_VELOCITY,
        // 	};
        // 	add_midi_event(NoteOnEvent(midi_conf.scale, &midi_note));
        // 	note_off_buf[i] = NoteOffEvent(midi_conf.scale, &midi_note);
        //     }

        //     // inject note off events
        //     add_midi_event(note_off_buf[0]);
        //
        //     for (size_t i = 1; i < chord_len; ++i) {
        // 	note_off_buf[i].delta_time = 0;
        //         add_midi_event(note_off_buf[i]);
        //     }
        // }
        // else
        //     report(dummy, expr.off, "unknown bug");
    }

    // dump events to file
    dump_midi_to_file();
    // free backend
    free_midi_backend();
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
