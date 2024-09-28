#ifndef AST_H_
#define AST_H_
#include "lexer.h"
#include "utils.h"
#include <stdbool.h>
typedef ssize_t AstIdx;
typedef AstIdx SecIdx;
typedef AstIdx DeclIdx;
typedef AstIdx FormalIdx;
typedef AstIdx ExprIdx;
make_slice(AstIdx);
make_slice(Note);
typedef struct
{
    Slice name;
    SecIdx sec;
    size_t off;
} Decl;
make_slice(Decl);

typedef union
{
    Slice ident;
    ssize_t num;
    SliceOf(AstIdx) chord_notes;
    struct { size_t dots; ExprIdx expr; } note;
} ExprData;
typedef enum
{
    EXPR_IDENT = 1,
    EXPR_NUM,
    EXPR_NOTE,
    EXPR_CHORD,
} ExprTag;
typedef struct
{
    ExprTag tag;
    ExprData data;
    size_t off;
} Expr;
make_slice(Expr);

typedef struct
{
    Slice ident;
    ExprIdx expr;
    size_t off;
} Formal;
make_slice(Formal);

typedef struct
{
    SliceOf(AstIdx) note_exprs;
    SliceOf(AstIdx) config;
    size_t off;
} Sec;
make_slice(Sec);
typedef struct
{
    DeclIdx main;
    DeclSlice decls; // array of decls
    SecSlice secs;
    FormalSlice formals;
    ExprSlice exprs;
    bool success;
} Pgm;
typedef enum
{ // if > 0, then the res is an index into some array
    PR_ERR = -1,
    PR_NULL = 0,
} ParseRes;
void expr_debug(Pgm* pgm, ExprIdx idx);
struct Gen;
typedef struct Gen Gen;
Pgm parse_ast(Lexer *lexer);
void ast_deinit(Pgm *pgm);
#define ast_get(pgm, arr, idx) (pgm)->arr.ptr[idx]
#define ast_len(pgm, arr) (pgm)->arr.len
#endif
