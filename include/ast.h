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
typedef AstIdx LabelIdx;
make_slice(AstIdx);


typedef union
{
    Symbol ident;
    ssize_t num;
    SliceOf(AstIdx) chord_notes;
    struct {
	ExprIdx tonic;
	ExprIdx octave;
	ExprIdx mode;
    } scale;
    struct {
        size_t dots;
        ExprIdx expr;
    } note;
    struct {
	Token op;
	ExprIdx expr;
    } prefix;
    struct {
	Token op;
	ExprIdx lhs;
	ExprIdx rhs;
    } infix;
    SecIdx sec;
    struct { 
	ExprIdx cond_expr;
	ExprIdx then_expr;
	ExprIdx else_expr;
    } if_then_else;
    struct {
	ExprIdx lower_bound;
	ExprIdx upper_bound;
	bool is_leq;
	SliceOf(AstIdx) body;
    } for_expr;
} ExprData;
typedef enum
{
    EXPR_IDENT = 1,
    EXPR_NUM,
    EXPR_NOTE,
    EXPR_LIST,
    EXPR_SCALE,
    // EXPR_PREFIX,
    EXPR_INFIX,
    EXPR_SEC,
    EXPR_BOOL,
    EXPR_VOID,
    EXPR_IF,
    EXPR_FOR,
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
typedef struct {
    Symbol name;
    int volume;
    bool linear;
    size_t note_pos; // label is after this note
} Label; // TODO lable is the same as config ??
make_slice(Label);
typedef struct
{
    SliceOf(AstIdx) note_exprs;
    SliceOf(AstIdx) vars;
    SliceOf(AstIdx) config;
    SliceOf(AstIdx) labels;
    size_t off;
} Sec;
make_slice(Sec);
typedef struct
{
    SliceOf(Sec) secs;
    SliceOf(Formal) formals;
    SliceOf(Expr) exprs;
    SliceOf(Label) labels;
    SliceOf(AstIdx) toplevel;
    bool success;
} Pgm;

void expr_debug(Pgm *pgm, SymbolTable sym_table, ExprIdx idx);
#define ast_get(pgm, arr, idx) (pgm)->arr.ptr[idx]
#define ast_len(pgm, arr) (pgm)->arr.len
#endif
