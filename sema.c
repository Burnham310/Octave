#include "sema.h"
	    
#include "ds.h"
#include "utils.h"
#include "assert.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>


#include "stb_ds.h"
// TODO variable scopes
#define type_only_val(ty) ((Val) {.ty = ty })
ValEnv* get_curr_env(Context *ctx, SecIdx idx) {
    if (idx < 0) return &ctx->pgm_env;
    return &ctx->sec_envs.ptr[idx];
}

Type sema_analy_expr_impl(Context *ctx, ExprIdx idx, SecIdx sec_idx);
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
    ctx->sec_envs = (SliceOf(ValEnv)) {0};
}

void sema_analy(Pgm *pgm, Lexer *lexer, Context *ctx) {
    *ctx = (Context) {
	.pgm = pgm, 
	.lexer = lexer, 
	.types = {.ptr = calloc(sizeof(Type), ast_len(pgm, exprs)), .len = ast_len(pgm, exprs)},
	.pgm_env = NULL,
	.sec_envs = {.ptr = calloc(sizeof(TypeEnv), ast_len(pgm, secs)), .len = ast_len(pgm, secs)},
	.sym_table = lexer->sym_table,
    };
#define X(x) ctx->builtin_syms.x = symt_intern(ctx->sym_table, #x);
    BUILTIN_SYMS
    CONFIG_SYMS
#undef X
    // buitin config, used for type checking
    hmput(ctx->config_builtin, ctx->builtin_syms.scale, TY_SCALE);
    hmput(ctx->config_builtin, ctx->builtin_syms.bpm, TY_INT);
    hmput(ctx->config_builtin, ctx->builtin_syms.instrument, TY_INT);
    // builtin constants
    for (size_t i = 0; i < DIATONIC; ++i) {
	ctx->builtin_syms.pitches[i] = symt_intern(ctx->sym_table, CONST_PITCH[i]);
	Val pitch_val = {.ty = TY_PITCH, .data.i = BASE_MODE[i]};
	hmput(ctx->pgm_env, ctx->builtin_syms.pitches[i], pitch_val);
	
	ctx->builtin_syms.modes[i] = symt_intern(ctx->sym_table, CONST_MODE[i]);
	Val mode_val = {.ty = TY_MODE, .data.i = i};
	hmput(ctx->pgm_env, ctx->builtin_syms.modes[i], mode_val);
    }
    // maj and min mode alias
    Val maj_mode_val = {.ty = TY_MODE, .data.i = MODE_ION};
    Val min_mode_val = {.ty = TY_MODE, .data.i = MODE_AEOL};
    ctx->builtin_syms.maj = symt_intern(ctx->sym_table, "MAJ");
    hmput(ctx->pgm_env, ctx->builtin_syms.maj, maj_mode_val);
    ctx->builtin_syms.min = symt_intern(ctx->sym_table, "MIN");
    hmput(ctx->pgm_env, ctx->builtin_syms.min, min_mode_val);

    ctx->success = sema_analy_pgm(ctx);
    
}
bool sema_analy_pgm(Context *ctx) {
    for (size_t ti = 0; ti < ast_len(ctx->pgm, toplevel); ++ti) {
	
	if (!sema_analy_formal(ctx, ast_get(ctx->pgm, toplevel, ti), false, -1)) return false;

    }
    if (hmgeti(ctx->pgm_env, ctx->builtin_syms.main) < 0) {
	report(ctx->lexer, 0, "section main is undefiend");
	return false;
    }
    return true;
}


Type sema_analy_sec(Context *ctx, SecIdx idx) {
    Sec *sec = &ast_get(ctx->pgm, secs, idx);
    for (size_t i = 0; i < sec->vars.len; ++i) {
	if (!sema_analy_formal(ctx, sec->vars.ptr[i], true, idx)) return false;
    }   
    for (size_t i = 0; i < sec->config.len; ++i) {
	if (!sema_analy_formal(ctx, sec->config.ptr[i], true, idx)) return false;
    }
    for (size_t i = 0; i < sec->note_exprs.len; ++i) {
	ExprIdx expr_idx = sec->note_exprs.ptr[i];
	Type ty = sema_analy_expr(ctx, sec->note_exprs.ptr[i], idx);
	if (ty == TY_ERR) return TY_ERR;
	if (ty != TY_NOTE) {
	    report(ctx->lexer, ast_get(ctx->pgm, exprs, expr_idx).off, "Expect type `note` in this part of section, found %s", type_to_str(ty));
	    return TY_ERR;
	}
    }
    return TY_SEC;
}
bool sema_analy_formal(Context *ctx, FormalIdx idx, bool builtin, SecIdx sec_idx) {
    // we have either a section environment or toplevel environment
    // return false;

    Formal *formal = &ast_get(ctx->pgm, formals, idx);
    Type ty = sema_analy_expr(ctx, formal->expr, sec_idx);
    if (ty == TY_ERR) return false;
    if (sec_idx < 0 && (ty != TY_SEC)) {
	report(ctx->lexer, formal->off, "Toplevel declaration %s must be Section, got %s", symt_lookup(ctx->sym_table, formal->ident), type_to_str(ty));
	return false;
    }
    ValEnv *env = get_curr_env(ctx, sec_idx);
    ptrdiff_t local_i = hmgeti(*env, formal->ident);
    if (local_i > 0) {
	    report(ctx->lexer, formal->off, "Duplicate defination of variable %s", symt_lookup(ctx->sym_table , formal->ident));
	    return false;
    }
    ptrdiff_t config_builtin_i = hmgeti(ctx->config_builtin, formal->ident);
    if (builtin) {
	if (config_builtin_i < 0) {
	    report(ctx->lexer, formal->off, "Unknown name %s in attribute", symt_lookup(ctx->sym_table, formal->ident));
	    return false;
	}
	Type expect_ty = ctx->config_builtin[config_builtin_i].value;
	if (ty == TY_ERR) return false;
	if (ty != expect_ty) {
	    report(
		    ctx->lexer, 
		    formal->off, 
		    "Type mismatch for attribute %s, expect %s, got %s", 
		    symt_lookup(ctx->sym_table, formal->ident), type_to_str(expect_ty), type_to_str(ty));
	    return false;
	}
    } else {
	if (config_builtin_i > 0) {
	    report(ctx->lexer, formal->off, "Variable name %s clashes with attribute", symt_lookup(ctx->sym_table, formal->ident));
	    return false; 
	}
    }
    hmput(*env, formal->ident, type_only_val(ty));
    return true;
    
}
Type sema_analy_expr(Context *ctx, ExprIdx idx, SecIdx sec_idx) {
    Type ty = sema_analy_expr_impl(ctx, idx, sec_idx);
    ctx->types.ptr[idx] = ty;
    return ty;
}
Type sema_analy_expr_impl(Context *ctx, ExprIdx idx, SecIdx sec_idx) {
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    Type sub_t;
    ptrdiff_t env_i;
    switch (expr->tag) {
	case EXPR_NUM: return TY_INT;
	case EXPR_IDENT:
	    
	    env_i = hmgeti(ctx->sec_envs.ptr[sec_idx], expr->data.ident);
	    if (env_i < 0) {
		env_i = hmgeti(ctx->pgm_env, expr->data.ident);
		if (env_i < 0) {
		    report(ctx->lexer, expr->off, "Undefined variable %s", symt_lookup(ctx->sym_table, expr->data.ident));
		    return TY_ERR;

		}
		return ctx->pgm_env[env_i].value.ty;
	    }
	    return ctx->sec_envs.ptr[sec_idx][env_i].value.ty;
	case EXPR_NOTE:
	    assert(expr->data.note.dots > 0);
	    sub_t = sema_analy_expr(ctx, expr->data.note.expr, sec_idx);
	    if (sub_t == TY_ERR) return TY_ERR;
	    if (sub_t != TY_INT && sub_t != TY_CHORD) {
		report(ctx->lexer, expr->off, "Expect either type `int` or `chord` in note, found %s", type_to_str(sub_t));
		return TY_ERR;
	    }
	    return TY_NOTE;
	case EXPR_CHORD:
	    for (size_t i = 0; i < expr->data.chord_notes.len; ++i) {
		Type sub_t = sema_analy_expr(ctx, expr->data.chord_notes.ptr[i], sec_idx);
		if (sub_t == TY_ERR) return TY_ERR;
		if (sub_t != TY_INT && sub_t != TY_CHORD) {
		    report(ctx->lexer, expr->off, "Expect either type `int` or `chord` in chord, found %s", type_to_str(sub_t));
		    return TY_ERR;
		}
	    }
	    return TY_CHORD;
	case EXPR_SCALE:
	    sub_t = sema_analy_expr(ctx, expr->data.scale.tonic, sec_idx);
	    if (sub_t == TY_ERR) return TY_ERR;
	    if (sub_t != TY_PITCH) {
		report(ctx->lexer, expr->off, "Expect %s, got %s in tonic of scale", type_to_str(TY_PITCH), type_to_str(sub_t));
		return TY_ERR;
	    }
	    sub_t = sema_analy_expr(ctx, expr->data.scale.octave, sec_idx);
	    if (sub_t == TY_ERR) return TY_ERR;
	    if (sub_t != TY_INT) {
		report(ctx->lexer, expr->off, "Expect %s, got %s in octave of scale", type_to_str(TY_INT), type_to_str(sub_t));
		return TY_ERR;
	    }
	    sub_t = sema_analy_expr(ctx, expr->data.scale.mode, sec_idx);
	    if (sub_t == TY_ERR) return TY_ERR;
	    if (sub_t != TY_MODE) {
		report(ctx->lexer, expr->off, "Expect %s, got %s in tonic of scale", type_to_str(TY_MODE), type_to_str(sub_t));	
		return TY_ERR;
	    }
	    return TY_SCALE;
	case EXPR_PREFIX:
	    // current only have pitch qualifiers as prefix operator
	    sub_t = sema_analy_expr(ctx, expr->data.prefix.expr, sec_idx);
	    if (sub_t == TY_ERR) return TY_ERR;
	    if (sub_t != TY_PITCH && sub_t != TY_INT && sub_t != TY_CHORD) {
		report(ctx->lexer, expr->off, "Expect INT, PITCH, or CHORD after pitch qualifier, got %s", type_to_str(sub_t));
		return TY_ERR;
	    }
	    return sub_t;
	case EXPR_SEC:
	    return sema_analy_sec(ctx, expr->data.sec);
	default:
	    eprintf("tag %i\n", expr->tag);
	    assert(false && "unknown tag");
    }
    
    assert(false && "unreachable");
}


