#include "type.h"
#include "assert.h"

Type type_check_expr_impl(Context *ctx, ExprIdx idx);

#define X(x) case x: return #x;
const char *type_to_str(Type ty) {
    switch (ty) {
	TYPE_LISTX
	
	default: return "unknown type";

    }
    assert(false && "unreachable");
}
#undef X
void context_deinit(Context *ctx) {
    assert(ctx->types.ptr && ctx->types.len > 0);
    free(ctx->types.ptr);
    ctx->types.ptr = NULL;
    ctx->types.len = 0;
}

void type_check(Pgm *pgm, Lexer *lexer, Context *ctx) {
    *ctx = (Context) {
	.pgm = pgm, 
	.lexer = lexer, 
	.types = {.ptr = calloc(sizeof(Type), ast_len(pgm, exprs)), .len = ast_len(pgm, exprs)},
    };
    ctx->success = type_check_pgm(ctx);
}
bool type_check_pgm(Context *ctx) {
    for (size_t di = 1; di < ast_len(ctx->pgm, decls); ++di) {
	if (!type_check_decl(ctx, di)) return false;
    }
    return true;
}
bool type_check_decl(Context *ctx, DeclIdx idx) {
    // TODO add binding
    return type_check_sec(ctx, (ast_get(ctx->pgm, decls, idx)).sec);
}

bool type_check_sec(Context *ctx, SecIdx idx) {
    Sec *sec = &ast_get(ctx->pgm, secs, idx);
    for (size_t i = 0; i < sec->config.len; ++i) {
	if (!type_check_formal(ctx, sec->config.ptr[i], true)) return false;
    }
    for (size_t i = 0; i < sec->note_exprs.len; ++i) {
	ExprIdx expr_idx = sec->note_exprs.ptr[i];
	Type ty = type_check_expr(ctx, sec->note_exprs.ptr[i]);
	if (ty != TY_NOTE) {
	    report(ctx->lexer, ast_get(ctx->pgm, exprs, expr_idx).off, "Expect type `note` in this part of section, found %s", type_to_str(ty));
	    return false;
	}
    }

    return true;
}
bool type_check_formal(Context *ctx, SecIdx idx, bool builtin) {
    // TODO: add binding
    return true;
}
Type type_check_expr(Context *ctx, ExprIdx idx) {
    Type ty = type_check_expr_impl(ctx, idx);
    ctx->types.ptr[idx] = ty;
    return ty;
}
Type type_check_expr_impl(Context *ctx, ExprIdx idx) {
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    switch (expr->tag) {
	case EXPR_NUM: return TY_INT;
	case EXPR_IDENT: assert(false && "unimplemented");
	case EXPR_NOTE:
	    assert(expr->data.note.dots > 0);
	    Type sub_t = type_check_expr(ctx, expr->data.note.expr);
	    if (sub_t != TY_INT && sub_t != TY_CHORD) {
		report(ctx->lexer, expr->off, "Expect either type `int` or `chord` in note, found %s", type_to_str(sub_t));
		return TY_ERR;
	    }
	    return TY_NOTE;
	case EXPR_CHORD:
	    for (size_t i = 0; i < expr->data.chord_notes.len; ++i) {
		Type sub_t = type_check_expr(ctx, expr->data.chord_notes.ptr[i]);
		if (sub_t != TY_INT && sub_t != TY_CHORD) {
		    report(ctx->lexer, expr->off, "Expect either type `int` or `chord` in chord, found %s", type_to_str(sub_t));
		    return TY_ERR;
		}
	    }
	    return TY_CHORD;
	default:
	    assert(false && "unknown tag");
    }
    
    assert(false && "unreachable");
}


