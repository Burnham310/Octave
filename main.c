#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void usage(const char* prog_name) {
    fprintf(stderr, "Usage: %s [input_file] -o [output_file]\n", prog_name);
}


#define RETURN(x) do { exit_code = x; goto out;} while (0)
int main(int argc, char** argv) {
    int exit_code = 0;
    if (argc != 4) RETURN(1);
    char* input_path = argv[1];
    if (strcmp(argv[2], "-o") != 0) RETURN(2);;
    char* output_path = argv[3];

    FILE *input_f = NULL;
    FILE *output_f = NULL;
    if ((input_f = fopen(input_path, "r")) == NULL) {
	perror("Cannot open src file");
	RETURN(1);
    }
    if ((output_f = fopen(output_path, "w")) == NULL) {
	perror("Cannot open output file");
	RETURN(1);
    }

    // get size of input buffer
    fseek(input_f, 0, SEEK_END);
    size_t input_size = ftell(input_f);
    fseek(input_f, 0, SEEK_SET);
    char* buf = malloc(input_size);
    printf("Reading input file of size: %zu\n", input_size);
    if (fread(buf, input_size, 1, input_f) == 0) {
	perror("Cannot read src file");
    }


    printf("|%s|\n", buf);
        

    
out:
    if (input_f) fclose(input_f);
    if (output_f) fclose(output_f);
    free(buf);
    if (exit_code != 0) usage(argv[0]);
    return exit_code;

}
