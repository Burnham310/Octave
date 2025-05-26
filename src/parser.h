#ifndef PARSER_H_
#define PARSER_H_
#include "ast.h"
#define PR_NULL -1
void expr_debug(Pgm *pgm, SymbolTable sym_table, ExprIdx idx);
struct Gen;
typedef struct Gen Gen;
Pgm parse_ast(Lexer *lexer);
void ast_deinit(Pgm *pgm);

#endif
