#ifndef PARSER_H_
#define PARSER_H_
#include "ast.h"
typedef enum
{ // if > 0, then the res is an index into some array
    PR_NULL = -1,
} ParseRes;
void expr_debug(Pgm *pgm, SymbolTable sym_table, ExprIdx idx);
struct Gen;
typedef struct Gen Gen;
Pgm parse_ast(Lexer *lexer);
void ast_deinit(Pgm *pgm);

#endif
