#ifndef AST_H_
#define AST_H_
#include "lexer.h"
#include <stdbool.h>
typedef ssize_t AstIdx;
typedef AstIdx SecIdx;
typedef AstIdx DeclIdx;

typedef struct {
    Note* ptr;
    size_t len;
} NoteSlice;

typedef struct {
    Slice name;
    SecIdx sec;
} Decl;
typedef struct {
    Decl* ptr;
    size_t len;
} DeclSlice;
typedef struct {
    NoteSlice notes;
} Sec;
typedef struct {
    Sec* ptr;
    size_t len;
} SecSlice;

typedef struct {
    DeclIdx main;
    DeclSlice decls; // array of decls
    SecSlice secs;
    bool success;
} Pgm;
typedef enum { // if > 0, then the res is an index into some array
    PR_ERR = -1,
    PR_NULL = 0,
} ParseRes;
#define da_free(da) do { \
    if ((da).cap > 0) {(da).cap = 0; free((da.items));}\
} while (0);
#define da_move(da, dest_slice) do { \
    dest_slice.ptr = da.items; \
    dest_slice.len = da.size; \
    da.cap = 0; \
    da.size = 0; \
    da.items = NULL; \
} while (0);
#define da_append(da, x) do { \
    if ((da).cap == 0) { \
	(da).cap = 10; \
	(da).items = calloc((da).cap, sizeof((da).items[0])); \
    } \
    if ((da).cap <= (da).size) { \
	(da).cap *= 2; \
	(da).items = realloc((da).items, (da).cap * sizeof((da).items[0])); \
    } \
    (da).items[(da).size++] = (x); \
} while (0) \



struct Gen;
typedef struct Gen Gen;
DeclIdx parse_decl(Lexer* lexer, Gen* gen);
SecIdx parse_section(Lexer* lexer, Gen* gen);
Pgm parse_ast(Lexer* lexer);
void ast_deinit(Pgm* pgm);

#endif
