#include "sema.h"

#include "ds.h"
#include "utils.h"
#include "assert.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

#include "stb_ds.h"
// TODO variable scopes
Type sema_analy_infix(Context *ctx, ExprIdx idx, SecIdx sec_idx, Token op, Type lhs_t, Type rhs_t);
#define type_only_val(ty) ((Val){.ty = ty})
ValEnv *get_curr_env(Context *ctx, SecIdx idx)
{
	if (idx < 0)
		return &ctx->pgm_env;
	return &ctx->sec_envs.ptr[idx];
}

Type sema_analy_expr_impl(Context *ctx, ExprIdx idx, SecIdx sec_idx);
#define X(x) \
	case x:  \
		return #x;
const char *type_to_str(Type ty)
{
	switch (ty)
	{
		TYPE_LISTX

	default:
		return "unknown type";
	}
	assert(false && "unreachable");
}
#undef X

// 1 <= degree <= 7

void context_deinit(Context *ctx)
{
	assert(ctx->types.ptr && ctx->types.len > 0);
	free(ctx->types.ptr);
	ctx->types = (SliceOf(Type)){0};

	assert(ctx->sec_envs.ptr && ctx->sec_envs.len > 0);
	for (size_t i = 0; i < ctx->sec_envs.len; ++i)
	{
		// stb checks for NULL for us
		shfree(ctx->sec_envs.ptr[i]);
	}
	free(ctx->sec_envs.ptr);
	ctx->sec_envs = (SliceOf(ValEnv)){0};
}

void sema_analy(Pgm *pgm, Lexer *lexer, Context *ctx)
{
	*ctx = (Context){
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
	for (size_t i = 0; i < DIATONIC; ++i)
	{
		ctx->builtin_syms.pitches[i] = symt_intern(ctx->sym_table, CONST_PITCH[i]);
		Val pitch_val = {.ty = TY_ABSPITCH, .data.i = BASE_MODE[i]};
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
bool sema_analy_pgm(Context *ctx)
{
	for (size_t ti = 0; ti < ast_len(ctx->pgm, toplevel); ++ti)
	{

		if (!sema_analy_formal(ctx, ast_get(ctx->pgm, toplevel, ti), false, -1))
			return false;
	}
	if (hmgeti(ctx->pgm_env, ctx->builtin_syms.main) < 0)
	{
		report(ctx->lexer, 0, "section main is undefiend");
		return false;
	}
	return true;
}
Type sema_analy_sec(Context *ctx, SecIdx idx)
{

	Sec *sec = &ast_get(ctx->pgm, secs, idx);
	for (size_t i = 0; i < sec->vars.len; ++i)
	{
		if (!sema_analy_formal(ctx, sec->vars.ptr[i], false, idx))
			return false;
	}
	for (size_t i = 0; i < sec->config.len; ++i)
	{
		if (!sema_analy_formal(ctx, sec->config.ptr[i], true, idx))
			return false;
	}
	for (size_t i = 0; i < sec->note_exprs.len; ++i)
	{
		ExprIdx expr_idx = sec->note_exprs.ptr[i];
		Type ty = sema_analy_expr(ctx, sec->note_exprs.ptr[i], idx);
		if (ty == TY_ERR)
			return TY_ERR;
		if (ty != TY_NOTE)
		{
			report(ctx->lexer, ast_get(ctx->pgm, exprs, expr_idx).off, "Expect type `note` in this part of section, found %s", type_to_str(ty));
			return TY_ERR;
		}
	}
	return TY_SEC;
}
bool sema_analy_formal(Context *ctx, FormalIdx idx, bool builtin, SecIdx sec_idx)
{
	// we have either a section environment or toplevel environment
	// return false;

	Formal *formal = &ast_get(ctx->pgm, formals, idx);
	Type ty = sema_analy_expr(ctx, formal->expr, sec_idx);
	if (ty == TY_ERR)
		return false;
	if (sec_idx < 0 && (ty != TY_SEC && ty != TY_CHORUS))
	{
		report(ctx->lexer, formal->off, "Toplevel declaration %s must be Section or Chorus, got %s", symt_lookup(ctx->sym_table, formal->ident), type_to_str(ty));
		return false;
	}
	ValEnv *env = get_curr_env(ctx, sec_idx);
	ptrdiff_t local_i = hmgeti(*env, formal->ident);
	if (local_i > 0)
	{
		report(ctx->lexer, formal->off, "Duplicate defination of variable %s", symt_lookup(ctx->sym_table, formal->ident));
		return false;
	}
	ptrdiff_t config_builtin_i = hmgeti(ctx->config_builtin, formal->ident);
	if (builtin)
	{
		if (config_builtin_i < 0)
		{
			report(ctx->lexer, formal->off, "Unknown name %s in attribute", symt_lookup(ctx->sym_table, formal->ident));
			return false;
		}
		Type expect_ty = ctx->config_builtin[config_builtin_i].value;
		if (ty == TY_ERR)
			return false;
		if (ty != expect_ty)
		{
			report(
				ctx->lexer,
				formal->off,
				"Type mismatch for attribute %s, expect %s, got %s",
				symt_lookup(ctx->sym_table, formal->ident), type_to_str(expect_ty), type_to_str(ty));
			return false;
		}
	}
	else
	{
		if (config_builtin_i > 0)
		{
			report(ctx->lexer, formal->off, "Variable name %s clashes with attribute", symt_lookup(ctx->sym_table, formal->ident));
			return false;
		}
	}
	hmput(*env, formal->ident, type_only_val(ty));
	return true;
}
Type ty_coerce(Context *ctx, SecIdx sec_idx, Type from, Type to, size_t off)
{

	if ((from == TY_INT || from == TY_DEGREE || from == TY_ABSPITCH) && to == TY_PITCH)
	{
		// if (sec_idx < 0) {
		//     report(ctx->lexer, off, "TY_INT or TY_CHORD cannot be ty_coerced to TY_PITCH outside of a section");
		//     return from;
		// }
		// ValEntry *scale = hmgetp_null(ctx->sec_envs.ptr[sec_idx], ctx->builtin_syms.scale);
		// if (scale == NULL) {
		//     report(ctx->lexer, off, "attribute scale must be defined for coercion from TY_INT or TY_CHORD to TY_PITCH");
		//     return from;
		// }
		return to;
	}
	else if ((from == TY_INT || from == TY_DEGREE || from == TY_ABSPITCH || from == TY_PITCH) && to == TY_CHORD)
	{
		return to;
	}
	else if (from == TY_SEC && to == TY_CHORUS)
	{
		return to;
	}
	return from;
}
#define assert_type(ty, expect, off)                                                            \
	do                                                                                          \
	{                                                                                           \
		if (ty_coerce(ctx, sec_idx, ty, expect, off) != expect)                                 \
		{                                                                                       \
			report(ctx->lexer, off, "Expect %s, got %s", type_to_str(expect), type_to_str(ty)); \
			return TY_ERR;                                                                      \
		}                                                                                       \
	} while (0);
Type sema_analy_infix(Context *ctx, ExprIdx idx, SecIdx sec_idx, Token op, Type lhs_t, Type rhs_t)
{
	if (op.type == '&')
	{
		assert_type(lhs_t, TY_CHORUS, op.off);
		assert_type(rhs_t, TY_CHORUS, op.off);
		return TY_CHORUS;
	}
	else if (op.type == '\'')
	{
		assert_type(lhs_t, TY_INT, op.off);
		assert_type(lhs_t, TY_CHORD, op.off);
		return TY_CHORD;
	}
	else if (op.type == '+')
	{
		assert_type(lhs_t, TY_INT, op.off);
		assert_type(rhs_t, TY_INT, op.off);
		return TY_INT;
	}
	else if (op.type == '-')
	{
		assert_type(lhs_t, TY_INT, op.off);
		assert_type(rhs_t, TY_INT, op.off);
		return TY_INT;
	}
	else if (op.type == '*')
	{
		assert_type(lhs_t, TY_INT, op.off);
		assert_type(rhs_t, TY_INT, op.off);
		return TY_INT;
	}
	else if (op.type == TK_EQ)
	{
		assert_type(lhs_t, TY_INT, op.off);
		assert_type(rhs_t, TY_INT, op.off);
		return TY_BOOL;
	}
	assert(false && "unreachable");
}

Type sema_analy_expr(Context *ctx, ExprIdx idx, SecIdx sec_idx)
{
	Type ty = sema_analy_expr_impl(ctx, idx, sec_idx);
	ctx->types.ptr[idx] = ty;
	return ty;
}
Type sema_analy_expr_impl(Context *ctx, ExprIdx idx, SecIdx sec_idx)
{
	Expr *expr = &ast_get(ctx->pgm, exprs, idx);
	Type sub_t, sub_t2;
	ptrdiff_t env_i = -1;
	switch (expr->tag)
	{
	case EXPR_NUM:
		return TY_INT;
	case EXPR_BOOL:
		return TY_BOOL;
	case EXPR_IDENT:

		if (sec_idx >= 0)
		{
			env_i = hmgeti(ctx->sec_envs.ptr[sec_idx], expr->data.ident);
			if (env_i >= 0)
				return ctx->sec_envs.ptr[sec_idx][env_i].value.ty;
		}
		env_i = hmgeti(ctx->pgm_env, expr->data.ident);
		if (env_i < 0)
		{
			report(ctx->lexer, expr->off, "Undefined variable %s", symt_lookup(ctx->sym_table, expr->data.ident));
			return TY_ERR;
		}

		return ctx->pgm_env[env_i].value.ty;
	case EXPR_NOTE:
		assert(expr->data.note.dots > 0);
		sub_t = sema_analy_expr(ctx, expr->data.note.expr, sec_idx);
		if (sub_t == TY_ERR)
			return TY_ERR;
		if (ty_coerce(ctx, sec_idx, sub_t, TY_CHORD, expr->off) != TY_CHORD)
		{
			report(ctx->lexer, expr->off, "Expect either TY_PITCH or TY_CHORD in note, found %s", type_to_str(sub_t));
			return TY_ERR;
		}
		return TY_NOTE;
	case EXPR_CHORD:
	{
		for (size_t i = 0; i < expr->data.chord_notes.len; ++i)
		{
			Type sub_t = sema_analy_expr(ctx, expr->data.chord_notes.ptr[i], sec_idx);
			if (sub_t == TY_ERR)
				return TY_ERR;
			if (ty_coerce(ctx, sec_idx, sub_t, TY_CHORD, expr->off) != TY_CHORD)
			{
				report(ctx->lexer, expr->off, "Expect either TY_CHORD, or TY_PITCH in chord, found %s", type_to_str(sub_t));
				return TY_ERR;
			}
		}
		return TY_CHORD;
	}
	case EXPR_SCALE:
		sub_t = sema_analy_expr(ctx, expr->data.scale.tonic, sec_idx);
		if (sub_t == TY_ERR)
			return TY_ERR;
		assert_type(sub_t, TY_PITCH, expr->off);
		sub_t = sema_analy_expr(ctx, expr->data.scale.octave, sec_idx);
		if (sub_t == TY_ERR)
			return TY_ERR;
		assert_type(sub_t, TY_INT, expr->off);
		sub_t = sema_analy_expr(ctx, expr->data.scale.mode, sec_idx);
		if (sub_t == TY_ERR)
			return TY_ERR;
		assert_type(sub_t, TY_MODE, expr->off);
		return TY_SCALE;
	// case EXPR_PREFIX:
	//     // current only have pitch qualifiers as prefix operator
	//     sub_t = sema_analy_expr(ctx, expr->data.prefix.expr, sec_idx);
	//     if (sub_t == TY_ERR) return TY_ERR;
	//     if (ty_coerce(ctx, sec_idx, sub_t, TY_CHORD, expr->off) != TY_CHORD) {
	// 	report(ctx->lexer, expr->off, "Expect TY_PITCH, or TY_CHORD after pitch qualifier, got %s", type_to_str(sub_t));
	// 	return TY_ERR;
	//     }
	//     return sub_t;
	case EXPR_SEC:
		return sema_analy_sec(ctx, expr->data.sec);
	case EXPR_INFIX:
		// eprintf("sub_t %i\n", ast_get(ctx->pgm, exprs, expr->data.infix.lhs).tag);
		sub_t = sema_analy_expr(ctx, expr->data.infix.lhs, sec_idx);
		sub_t2 = sema_analy_expr(ctx, expr->data.infix.rhs, sec_idx);
		return sema_analy_infix(ctx, idx, sec_idx, expr->data.infix.op, sub_t, sub_t2);
	default:
		eprintf("tag %i\n", expr->tag);
		assert(false && "unknown tag");
	}

	assert(false && "unreachable");
}
