#ifndef AST_H_
#define AST_H_
#include "lexer.h"
#include "utils.h"
#include <stdbool.h>
#include "ds.h"

typedef ssize_t AstIdx;
typedef AstIdx SecIdx;
typedef AstIdx DeclIdx;
typedef AstIdx FormalIdx;
typedef AstIdx ExprIdx;
make_slice(AstIdx);
typedef struct
{
    Symbol name;
    SecIdx sec;
    size_t off;
} Decl;
make_slice(Decl);

typedef union
{
    Symbol ident;
    ssize_t num;
    SliceOf(AstIdx) chord_notes;
    struct
    {
        size_t dots;
        ExprIdx expr;
    } note;
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
    Symbol ident;
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
#define ast_get(pgm, arr, idx) (pgm)->arr.ptr[idx]
#define ast_len(pgm, arr) (pgm)->arr.len
#endif
