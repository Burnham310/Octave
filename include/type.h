#ifndef TYPE_H_
#define TYPE_H_
#include "ast.h"
#include "utils.h"
#include "stb_ds.h"

// x macro: https://en.wikipedia.org/wiki/X_macro

#define TYPE_LISTX \
    X(TY_ERR) \
    X(TY_VOID) \
    X(TY_INT) \
    X(TY_DEGREE) \
    X(TY_CHORD) \
    X(TY_NOTE) \
    X(TY_SCALE)
#define X(x) x,
typedef enum {
    TYPE_LISTX
} Type;
#undef X


// #define X(x) case x: \
//     printf(#x); \
//     break;
// void type_debug(Type ty) {
//     switch (ty) {
// 	TYPE_LISTX
// 	default:
// 	    printf("unknown type");
//     }
// 
// #undef X

const char *type_to_str(Type ty);
typedef struct {
    const char *key;
    Type value; // could also holds a value in the future?
} TypeKV;
typedef TypeKV * TypeEnv;

make_slice(Type);
make_slice(TypeEnv);
typedef struct {
    
    Pgm* pgm;
    Lexer* lexer;
    SliceOf(Type) types; // len == pgm->exprs.len
    TypeEnv pgm_env; // top level env
    SliceOf(TypeEnv) sec_envs; // one for each section
    SecIdx curr_sec; // this is only used internally while type_check'ing
    TypeEnv builtin;
    bool success;
} Context;
void context_deinit(Context *ctx);
void type_check(Pgm* pgm, Lexer* lexer, Context *ctx /* res arg */);
bool type_check_pgm(Context *ctx);
bool type_check_decl(Context *ctx, DeclIdx idx);
bool type_check_sec(Context *ctx, SecIdx idx);
bool type_check_formal(Context *ctx, FormalIdx idx, bool builtin);
Type type_check_expr(Context *ctx, ExprIdx idx);
#endif
