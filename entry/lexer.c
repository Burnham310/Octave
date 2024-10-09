#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "ast.h"
#include "backend.h"

void usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s [input_file]\n", prog_name);
}
char *input_path;
FILE *input_f;
char *buf;

#define RETURN(x)      \
    do                 \
    {                  \
        exit_code = x; \
        goto out;      \
    } while (0)
int main(const int argc, char **argv)
{
    int exit_code = 0;
    if (argc != 2)
        RETURN(1);
    input_path = argv[1];
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

    Token tk;
    while ((tk = lexer_next(&lexer)).type > 0)
    {
        printf("%s\n", ty_str(tk.type));
    }	

out:
    if (input_f)
	fclose(input_f);
    free(buf);
    if (exit_code != 0)
        usage(argv[0]);
    return exit_code;
}
