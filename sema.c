#include "sema.h"
#include "compiler.h"
#include "ds.h"
#include "utils.h"
#include "assert.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>


#include "stb_ds.h"
Type sema_analy_expr_impl(Context *ctx, ExprIdx idx);
#define X(x) case x: return #x;
const char *type_to_str(Type ty) {
    switch (ty) {
	TYPE_LISTX
	
	default: return "unknown type";

    }
    assert(false && "unreachable");
}
#undef X

// 1 <= degree <= 7
Pitch pitch_from_scale(Scale *scale, size_t degree) {
    assert(degree <= DIATONIC && degree >= 1 && "degree out of bound");
    assert(scale->tonic < DIATONIC && scale->tonic >= 0 && "tonic out of bound");

    size_t base = scale->tonic + scale->octave * 12;
    // degree is 1-based
    size_t degree_shift = degree + scale->mode - 1;
    // Step is how much we should walk from the tonic
    size_t step = degree_shift < DIATONIC 
	? BASE_MODE[degree_shift] - BASE_MODE[scale->mode]
	: 12 + BASE_MODE[degree_shift % DIATONIC] - BASE_MODE[scale->mode];
    return base + step;    
}
void context_deinit(Context *ctx) {
    assert(ctx->types.ptr && ctx->types.len > 0);
    free(ctx->types.ptr);
    ctx->types = (SliceOf(Type)) {0};

    assert(ctx->sec_envs.ptr && ctx->sec_envs.len > 0);
    for (size_t i = 0; i < ctx->sec_envs.len; ++i) {
        // stb checks for NULL for us
        shfree(ctx->sec_envs.ptr[i]);
    }
    free(ctx->sec_envs.ptr);
    ctx->sec_envs = (SliceOf(TypeEnv)) {0};
}

void sema_analy(Pgm *pgm, Lexer *lexer, Context *ctx) {
    *ctx = (Context) {
	.pgm = pgm, 
	.lexer = lexer, 
	.types = {.ptr = calloc(sizeof(Type), ast_len(pgm, exprs)), .len = ast_len(pgm, exprs)},
	.pgm_env = NULL,
	.sec_envs = {.ptr = calloc(sizeof(TypeEnv), ast_len(pgm, secs)), .len = ast_len(pgm, secs)},
	.curr_sec = 0,
	.main = 0,
	.sym_table = lexer->sym_table,
    };
#define X(x) ctx->builtin_syms.x = symt_intern(ctx->sym_table, #x);
    BUILTIN_SYMS
    CONFIG_SYMS
#undef X
    hmput(ctx->pgm_env, ctx->builtin_syms.scale, TY_SCALE);
    hmput(ctx->pgm_env, ctx->builtin_syms.bpm, TY_INT);
    for (size_t i = 0; i < DIATONIC; ++i) {
	ctx->builtin_syms.pitches[i] = symt_intern(ctx->sym_table, CONST_PITCH[i]);
	hmput(ctx->pgm_env, ctx->builtin_syms.pitches[i], TY_PITCH);

	ctx->builtin_syms.modes[i] = symt_intern(ctx->sym_table, CONST_MODE[i]);
	hmput(ctx->pgm_env, ctx->builtin_syms.modes[i], TY_MODE);
    }
    // maj and min mode alias
    ctx->builtin_syms.maj = symt_intern(ctx->sym_table, "MAJ");
    hmput(ctx->pgm_env, ctx->builtin_syms.maj, TY_MODE);
    ctx->builtin_syms.min = symt_intern(ctx->sym_table, "MIN");
    hmput(ctx->pgm_env, ctx->builtin_syms.min, TY_MODE);

    ctx->success = sema_analy_pgm(ctx);
}
bool sema_analy_pgm(Context *ctx) {
    for (size_t di = 1; di < ast_len(ctx->pgm, decls); ++di) {
	if (!sema_analy_decl(ctx, di)) return false;
    }
    if (ctx->main == 0) {
	report(ctx->lexer, 0, "section main is undefiend");
	return false;
    }
    return true;
}
bool sema_analy_decl(Context *ctx, DeclIdx idx) {
    Decl *decl = &ast_get(ctx->pgm, decls, idx);
    if (!sema_analy_sec(ctx, decl->sec)) return false;
    if (hmgeti(ctx->pgm_env, decl->name) >= 0) {
	report(ctx->lexer, decl->off, "Duplicate defination of section %s", symt_lookup(ctx->sym_table, decl->name));
	return false;
    }
    hmput(ctx->pgm_env, decl->name, TY_SEC);
    if (decl->name == ctx->builtin_syms.main) ctx->main = idx;
    return true;
}

bool sema_analy_sec(Context *ctx, SecIdx idx) {
    Sec *sec = &ast_get(ctx->pgm, secs, idx);
    ctx->curr_sec = idx;
    for (size_t i = 0; i < sec->config.len; ++i) {
	if (!sema_analy_formal(ctx, sec->config.ptr[i], true)) return false;
    }
    for (size_t i = 0; i < sec->note_exprs.len; ++i) {
	ExprIdx expr_idx = sec->note_exprs.ptr[i];
	Type ty = sema_analy_expr(ctx, sec->note_exprs.ptr[i]);
	if (ty != TY_NOTE) {
	    report(ctx->lexer, ast_get(ctx->pgm, exprs, expr_idx).off, "Expect type `note` in this part of section, found %s", type_to_str(ty));
	    return false;
	}
    }
    return true;
}
bool sema_analy_formal(Context *ctx, FormalIdx idx, bool builtin) {
    Formal *formal = &ast_get(ctx->pgm, formals, idx);
    Type ty = sema_analy_expr(ctx, formal->expr);
    if (ty == TY_ERR) return false;
    TypeEnv *sec_env = &ctx->sec_envs.ptr[ctx->curr_sec];
    hmput(ctx->pgm_env, formal->ident, ty);
    ptrdiff_t local_i = hmgeti(*sec_env, formal->ident);
    if (local_i > 0) {
	    report(ctx->lexer, formal->off, "Duplicate defination of attribute %s", symt_lookup(ctx->sym_table , formal->ident));
	    return false;
    }
    ptrdiff_t global_i = hmgeti(ctx->pgm_env, formal->ident);
    if (builtin) {
	if (global_i < 0) {
	    report(ctx->lexer, formal->off, "Unknown name %s in attribute", symt_lookup(ctx->sym_table, formal->ident));
	    return false;
	}
	
	Type expect_ty = ctx->pgm_env[global_i].value;
	if (ty != expect_ty) {
	    report(
		    ctx->lexer, 
		    formal->off, 
		    "Type mismatch for attribute %s, expect %s, got %s", 
		    symt_lookup(ctx->sym_table, formal->ident), type_to_str(expect_ty), type_to_str(ty));
	    return false;
	}
    } else {
	if (global_i > 0) {
	    report(ctx->lexer, formal->off, "Variable name %s clashes with attribute", symt_lookup(ctx->sym_table, formal->ident));
	    return false; 
	}
    }
    hmput(*sec_env, formal->ident, ty);
    return true;
    
}
Type sema_analy_expr(Context *ctx, ExprIdx idx) {
    Type ty = sema_analy_expr_impl(ctx, idx);
    ctx->types.ptr[idx] = ty;
    return ty;
}
Type sema_analy_expr_impl(Context *ctx, ExprIdx idx) {
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    Type sub_t;
    ptrdiff_t env_i;
    switch (expr->tag) {
	case EXPR_NUM: return TY_INT;
	case EXPR_IDENT:
	    env_i = hmgeti(ctx->sec_envs.ptr[ctx->curr_sec], expr->data.ident);
	    if (env_i >= 0) return ctx->sec_envs.ptr[ctx->curr_sec][env_i].value;
	    env_i = hmgeti(ctx->pgm_env, expr->data.ident);
	    if (env_i < 0) {
		report(ctx->lexer, expr->off, "Undefined variable %s", symt_lookup(ctx->sym_table, expr->data.ident));
		return false;
	    }
	    return ctx->pgm_env[env_i].value;
	case EXPR_NOTE:
	    assert(expr->data.note.dots > 0);
	    sub_t = sema_analy_expr(ctx, expr->data.note.expr);
	    if (sub_t != TY_INT && sub_t != TY_CHORD) {
		report(ctx->lexer, expr->off, "Expect either type `int` or `chord` in note, found %s", type_to_str(sub_t));
		return TY_ERR;
	    }
	    return TY_NOTE;
	case EXPR_CHORD:
	    for (size_t i = 0; i < expr->data.chord_notes.len; ++i) {
		Type sub_t = sema_analy_expr(ctx, expr->data.chord_notes.ptr[i]);
		if (sub_t != TY_INT && sub_t != TY_CHORD) {
		    report(ctx->lexer, expr->off, "Expect either type `int` or `chord` in chord, found %s", type_to_str(sub_t));
		    return TY_ERR;
		}
	    }
	    return TY_CHORD;
	case EXPR_SCALE:
	    sub_t = sema_analy_expr(ctx, expr->data.scale.tonic);
	    if (sub_t != TY_PITCH) {
		report(ctx->lexer, expr->off, "Expect %s, got %s in tonic of scale", type_to_str(TY_PITCH), type_to_str(sub_t));
		return TK_ERR;
	    }
	    sub_t = sema_analy_expr(ctx, expr->data.scale.octave);
	    if (sub_t != TY_INT) {
		report(ctx->lexer, expr->off, "Expect %s, got %s in octave of scale", type_to_str(TY_INT), type_to_str(sub_t));
		return TY_ERR;
	    }
	    sub_t = sema_analy_expr(ctx, expr->data.scale.mode);
	    if (sub_t != TY_MODE) {
		report(ctx->lexer, expr->off, "Expect %s, got %s in tonic of scale", type_to_str(TY_MODE), type_to_str(sub_t));	
		return TY_ERR;
	    }
	    return TY_SCALE;
	case EXPR_PREFIX:
	    // current only have pitch qualifiers as prefix operator
	    sub_t = sema_analy_expr(ctx, expr->data.prefix.expr);
	    if (sub_t != TY_PITCH && sub_t != TY_INT && sub_t != TY_CHORD) {
		report(ctx->lexer, expr->off, "Expect INT, PITCH, or CHORD after pitch qualifier, got %s", type_to_str(sub_t));
		return false;
	    }
	    return sub_t;
	default:
	    assert(false && "unknown tag");
    }
    
    assert(false && "unreachable");
}


