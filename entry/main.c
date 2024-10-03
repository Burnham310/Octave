#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "backend.h"
#include "utils.h"
#include "type.h"
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
    type_check(&pgm, &lexer, &ctx);
    if (!ctx.success) {
	eprintf("typechecking fails\n");
	RETURN(1);
    }
    Decl main = pgm.decls.ptr[pgm.main];
    Sec main_sec = pgm.secs.ptr[main.sec];

    // backend initialization
    init_midi_backend(output_f);

    // default configuration
    struct _conf
    {
        int bpm;
        MidiScaleType scale;
    } midi_conf = {
        .bpm = 120,
        .scale = CMAJOR,
    };

    // update configuration for section
    for (int i = 0; i < main_sec.config.len; ++i)
    {
        Formal formal = pgm.formals.ptr[main_sec.config.ptr[i]];

        if (strncmp(formal.ident.ptr, "scale", formal.ident.len) == 0)
        {
            midi_conf.scale = str_to_MidiKeyType(pgm.exprs.ptr[formal.expr].data.ident.ptr, pgm.exprs.ptr[formal.expr].data.ident.len);
            ASSERT(midi_conf.scale == ERRSCALE, report(dummy, formal.off, "Expect identifier in `scale` attribute"));
            printf("scale => %d\n", midi_conf.scale);
            continue;
        }
        if (strncmp(formal.ident.ptr, "bpm", formal.ident.len) == 0)
        {
            midi_conf.bpm = pgm.exprs.ptr[formal.expr].data.num;
            ASSERT(pgm.exprs.ptr[formal.expr].tag == EXPR_NUM, report(dummy, formal.off, "Expect num in `bpm` attribute"));
            printf("bpm => %d\n", midi_conf.bpm);
            continue;
        }

        WARNING(dummy, formal.off, "unsupport configuration: '%.*s'", (int)formal.ident.len, formal.ident.ptr);
    }

    // add event to track
    for (int i = 0; i < main_sec.note_exprs.len; ++i)
    {
        ExprIdx idx = main_sec.note_exprs.ptr[i];
        Expr expr = pgm.exprs.ptr[idx];
        ASSERT(expr.tag == EXPR_NOTE, report(dummy, expr.off, "Expect note in this part of Section"));

        Expr pitch_expr = pgm.exprs.ptr[expr.data.note.expr];

        if (pitch_expr.tag == EXPR_NUM)
        {
            MidiNote midi_note = {
                .channel = DEFAULT_CHANNEL,
                .length = expr.data.note.dots,
                .pitch = pitch_expr.data.num,
                .velocity = DEFAULT_VELOCITY,
            };
            add_midi_event(NoteOnEvent(midi_conf.scale, &midi_note));
            add_midi_event(NoteOffEvent(midi_conf.scale, &midi_note));
        }
        else if (pitch_expr.tag == EXPR_CHORD)
        {
            size_t chord_len = pitch_expr.data.chord_notes.len;
            struct _MidiEvent note_off_buf[chord_len];
            for (size_t i = 0; i < chord_len; ++i)
            {
                AstIdx chrod_pitch_idx = pitch_expr.data.chord_notes.ptr[i];
                Expr chrod_pitch_expr = pgm.exprs.ptr[chrod_pitch_idx];
                MidiNote midi_note = {
                    .channel = DEFAULT_CHANNEL,
                    .length = expr.data.note.dots,
                    .pitch = chrod_pitch_expr.data.num,
                    .velocity = DEFAULT_VELOCITY,
                };
                add_midi_event(NoteOnEvent(midi_conf.scale, &midi_note));
                note_off_buf[i] = NoteOffEvent(midi_conf.scale, &midi_note);
            }

            // inject note off events
	    add_midi_event(note_off_buf[0]);
	    
            for (size_t i = 1; i < chord_len; ++i) {
		note_off_buf[i].delta_time = 0;
                add_midi_event(note_off_buf[i]);
	    }
        }
        else
            report(dummy, expr.off, "unknown bug");
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
    return exit_code;
}
