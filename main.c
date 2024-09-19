#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "ast.h"
#include "backend.h"

void usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s [input_file] -o [output_file]\n", prog_name);
}
char *input_path, *output_path;
FILE *input_f;
FILE *output_f;
char *buf;
Pgm pgm;

#define RETURN(x)      \
    do                 \
    {                  \
        exit_code = x; \
        goto out;      \
    } while (0)
int main(const int argc, char **argv)
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

    Lexer lexer = lexer_init(buf, input_size, input_path);

    // Token tk;
    // while ((tk = lexer_next(&lexer)).type > 0)
    // {
    //     TOKEN_DEBUG(tk)
    //     // printf("%d\n", tk.type);
    //     printf("\n");
    // }	
    // lexer = lexer_init(buf, input_size, input_path);

    pgm = parse_ast(&lexer);
    if (!pgm.success) {
	eprintf("Failed to parse ast\n");
	RETURN(1);
    }
    Decl main = pgm.decls.ptr[pgm.main];
    Sec main_sec =  pgm.secs.ptr[main.sec];
    init_midi_output(output_path);

    Track track = {
	.note_count = 0,
    };
    unsigned char cmajor[] = {60, 62, 64, 65, 67, 69, 71, 72};

    for (int i = 0; i < main_sec.notes.len; ++i) {
	Note note = main_sec.notes.ptr[i];
	printf("%zi.%zu => %i\n", note.pitch, note.dots, cmajor[note.pitch - 1]);
	MidiNote midi_note = {
	    .channel = DEFAULT_CHANNEL,
	    .length = note_length(note.dots - 1),
	    .pitch = cmajor[note.pitch - 1],
	    .velocity = DEFAULT_VELOCITY,
	};
	add_note_to_track(&track, &midi_note);
    }
    write_track(&track);
    close_midi_output();



out:
    if (input_f)
	fclose(input_f);
    free(buf);
    if (exit_code != 0)
        usage(argv[0]);
    if (pgm.success)
	ast_deinit(&pgm);
    return exit_code;
}
