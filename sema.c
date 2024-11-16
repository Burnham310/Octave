#include "sema.h"

#include "ds.h"
#include "utils.h"
#include "assert.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

#include "stb_ds.h"

TypeIntern ty_pool = NULL;

Type ty_coerce(Context *ctx, SecIdx sec_idx, Type from, Type to, size_t off);
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
    TypeFull ty_full = lookup_ty(ty);
    switch (ty_full.ty)
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

	hmput(ctx->config_builtin, ctx->builtin_syms.scale, intern_simple_ty(TY_SCALE));
    hmput(ctx->config_builtin, ctx->builtin_syms.bpm, intern_simple_ty(TY_INT));
    hmput(ctx->config_builtin, ctx->builtin_syms.instrument, intern_simple_ty(TY_INT));
    // builtin constants
    Type mode_ty = intern_simple_ty(TY_MODE);
    for (size_t i = 0; i < DIATONIC; ++i)
    {
	ctx->builtin_syms.pitches[i] = symt_intern(ctx->sym_table, CONST_PITCH[i]);
	Val pitch_val = {.ty = intern_simple_ty(TY_ABSPITCH), .data.i = BASE_MODE[i]};
	hmput(ctx->pgm_env, ctx->builtin_syms.pitches[i], pitch_val);

	ctx->builtin_syms.modes[i] = symt_intern(ctx->sym_table, CONST_MODE[i]);
	Val mode_val = {.ty = mode_ty, .data.i = i};
	hmput(ctx->pgm_env, ctx->builtin_syms.modes[i], mode_val);
    }
    // maj and min mode alias
    Val maj_mode_val = {.ty = mode_ty, .data.i = MODE_ION};
    Val min_mode_val = {.ty = mode_ty, .data.i = MODE_AEOL};
    ctx->builtin_syms.maj = symt_intern(ctx->sym_table, "MAJ");
    hmput(ctx->pgm_env, ctx->builtin_syms.maj, maj_mode_val);
    ctx->builtin_syms.min = symt_intern(ctx->sym_table, "MIN");
    hmput(ctx->pgm_env, ctx->builtin_syms.min, min_mode_val);
    // "true" and "false" are its own expression, not identifiers
    ctx->success = sema_analy_pgm(ctx);
}
bool sema_analy_pgm(Context *ctx)
{
    for (size_t ti = 0; ti < ast_len(ctx->pgm, toplevel); ++ti)
    {

	if (!sema_analy_formal(ctx, ast_get(ctx->pgm, toplevel, ti), false, -1))
	    return false;
    }
    ssize_t main_i = hmgeti(ctx->pgm_env, ctx->builtin_syms.main);
    if (main_i  < 0)
    {
	report(ctx->lexer, 0, "section main is undefiend");
	return false;
    }

    Type main_type = ctx->pgm_env[main_i].value.ty;
    Type sec = intern_simple_ty(TY_SEC);
    Type chorus = intern_ty(((TypeFull) {.ty = TY_LIST, .more = sec}));
    eprintf("main %s\n", type_to_str(lookup_ty(main_type).more));
    if (main_type.i != sec.i && main_type.i != chorus.i) {
	report(ctx->lexer, 0, "main must be either section or a list of section, got %s", type_to_str(main_type));
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
	    THROW_EXCEPT();
    }
    for (size_t i = 0; i < sec->config.len; ++i)
    {
	if (!sema_analy_formal(ctx, sec->config.ptr[i], true, idx))
	    THROW_EXCEPT();
    }
    for (size_t i = 0; i < sec->note_exprs.len; ++i)
    {
	ExprIdx expr_idx = sec->note_exprs.ptr[i];
	size_t off = ast_get(ctx->pgm, exprs, expr_idx).off;
	Type ty = sema_analy_expr(ctx, sec->note_exprs.ptr[i], idx);
	Type note_ty = intern_simple_ty(TY_NOTE);
	if (ty_coerce(ctx, idx, ty, note_ty, off).i != note_ty.i 
		// && ty_coerce(ctx, idx, ty, TY_FOR, off) != TY_FOR
	   )
	{
	    report(ctx->lexer, off, "Expect type `note` in this part of section, found %s", type_to_str(ty));
	    THROW_EXCEPT();
	}
    }
    return intern_simple_ty(TY_SEC);
}
bool sema_analy_formal(Context *ctx, FormalIdx idx, bool builtin, SecIdx sec_idx)
{
    // we have either a section environment or toplevel environment
    // return false;

    Formal *formal = &ast_get(ctx->pgm, formals, idx);
    Type ty = sema_analy_expr(ctx, formal->expr, sec_idx);
    // if (sec_idx < 0 && (ty.i != intern_simple_ty(TY_SEC).i && ty != TY_CHORUS))
    // {
    //     report(ctx->lexer, formal->off, "Toplevel declaration %s must be Section or Chorus, got %s", symt_lookup(ctx->sym_table, formal->ident), type_to_str(ty));
    //     return false;
    // }
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
	if (ty.i != expect_ty.i)
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
    (void)(ctx);
    (void)(sec_idx);
    (void)(off);
    TypeFull from_full = lookup_ty(from);
    TypeFull to_full = lookup_ty(to);
    // recurse down the list
    if (from_full.ty == TY_ANY) return to;
    if (from_full.ty == TY_LIST && to_full.ty == TY_LIST) {
	Type sub_ty = ty_coerce(ctx, sec_idx, from_full.more, to_full.more, off);	
	TypeFull list_ty = {.ty = TY_LIST, .more = sub_ty };
	return intern_ty(list_ty);
    }
    if (from_full.ty == TY_SPREAD && to_full.ty == TY_SPREAD) {
	Type sub_ty = ty_coerce(ctx, sec_idx, from_full.more, to_full.more, off);	
	TypeFull list_ty = {.ty = TY_SPREAD, .more = sub_ty };
	return intern_ty(list_ty);
    }
    if (from_full.ty == TY_SPREAD) {
	Type sub_ty = ty_coerce(ctx, sec_idx, from_full.more, to, off);
	return sub_ty;	
    }
    if ((from_full.ty == TY_INT || from_full.ty == TY_DEGREE || from_full.ty == TY_ABSPITCH) && to_full.ty == TY_PITCH)
    {
	return to;
    }
    if ((from_full.ty == TY_INT && to_full.ty == TY_DEGREE)) {
	return to;
    }
    else if (
	    (from_full.ty == TY_INT || from_full.ty == TY_DEGREE || from_full.ty == TY_ABSPITCH || from_full.ty == TY_PITCH) 
	    && (to_full.ty == TY_LIST && ty_pool[to_full.more.i].key.ty == TY_PITCH))
    {
	return to;
    }
    else if (from_full.ty == TY_SEC && (to_full.ty == TY_LIST && ty_pool[to_full.more.i].key.ty == TY_SEC))
    {
	return to;
    } else if (from_full.ty == TY_VOID && to_full.ty == TY_NOTE) // may be TY_VOID should coerced to anything
    {
	return to; 
    }
    return from;
}
#define assert_type(ty, expect, off)                                                            \
    do                                                                                          \
{                                                                                           \
    if (ty_coerce(ctx, sec_idx, ty, expect, off).i != expect.i)                                 \
    {                                                                                       \
	report(ctx->lexer, off, "Expect %s, got %s", type_to_str(expect), type_to_str(ty)); \
	THROW_EXCEPT();                                                                      \
    }                                                                                       \
} while (0);
Type sema_analy_infix(Context *ctx, ExprIdx idx, SecIdx sec_idx, Token op, Type lhs_t, Type rhs_t)
{
    (void)(idx);
    Type int_ty = intern_simple_ty(TY_INT);

    if (op.type == '&')
    {
	// Type sec_ty = intern_simple_ty(TY_SEC);	
	// TypeFull chorus_ty_full = {.ty = TY_LIST, .more = sec_ty.i };
	// Type chorus_ty = intern_ty(chorus_ty_full);
	// assert_type(lhs_t, chorus_ty, op.off);
	// assert_type(rhs_t, chorus_ty, op.off);
	// return chorus_ty;
	assert(false && "obslete");
    }
    else if (op.type == '\'')
    {
	assert_type(lhs_t, int_ty, op.off);

	TypeFull rhs_full = lookup_ty(rhs_t);
	Type deg_ty = intern_simple_ty(TY_DEGREE);
	Type abspitch_ty = intern_simple_ty(TY_ABSPITCH);
	Type pitch_ty = intern_simple_ty(TY_PITCH);
	if (rhs_full.ty == TY_INT) {
	    return intern_simple_ty(TY_DEGREE); 
	} else if (rhs_t.i == deg_ty.i || rhs_t.i == abspitch_ty.i || rhs_t.i == pitch_ty.i) {
	    return rhs_t;
	} else if (rhs_full.ty == TY_LIST && (rhs_full.more.i == int_ty.i || rhs_full.more.i == deg_ty.i || rhs_full.more.i == abspitch_ty.i || rhs_full.more.i == pitch_ty.i)) {
	    return rhs_t;
	}
	report(ctx->lexer, op.off, "Invalid rhs for qualifier operation");
	THROW_EXCEPT();
    }
    else if (op.type == '+')
    {
	assert_type(lhs_t, int_ty, op.off);
	assert_type(rhs_t, int_ty, op.off);
	return int_ty;
    }
    else if (op.type == '-')
    {
	assert_type(lhs_t, int_ty, op.off);
	assert_type(rhs_t, int_ty, op.off);
	return int_ty;
    }
    else if (op.type == '*')
    {
	assert_type(lhs_t, int_ty, op.off);
	assert_type(rhs_t, int_ty, op.off);
	return int_ty;
    }
    else if (op.type == TK_EQ)
    {
	assert_type(lhs_t, int_ty, op.off);
	assert_type(rhs_t, int_ty, op.off);
	return intern_simple_ty(TY_BOOL);
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
	    return intern_simple_ty(TY_INT);
	case EXPR_BOOL:
	    return intern_simple_ty(TY_BOOL);
	case EXPR_VOID:
	    return intern_simple_ty(TY_VOID);
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
		THROW_EXCEPT();
	    }

	    return ctx->pgm_env[env_i].value.ty;
	case EXPR_NOTE:
	    assert(expr->data.note.dots > 0);
	    sub_t = sema_analy_expr(ctx, expr->data.note.expr, sec_idx);
	    TypeFull chord_ty_full = {.ty = TY_LIST, .more = intern_simple_ty(TY_PITCH).i };
	    Type chord_ty = intern_ty(chord_ty_full);

	    assert_type(sub_t, chord_ty, expr->off);
	    return intern_simple_ty(TY_NOTE);
	case EXPR_LIST:
	    {
		size_t len = expr->data.chord_notes.len;
		Type sub_t = intern_simple_ty(TY_ANY);
		for (size_t i = 0; i < len; ++i)
		{
		    Type rest_t = sema_analy_expr(ctx, expr->data.chord_notes.ptr[i], sec_idx);
		    if (sub_t.i == ty_coerce(ctx, sec_idx, rest_t, sub_t, expr->off).i) {
			// does nothing	
		    } else if (rest_t.i == ty_coerce(ctx, sec_idx, sub_t, rest_t, expr->off).i) {
			sub_t = rest_t;
		    } else {
			report(ctx->lexer, expr->off, "Type mistmatched in list, previously inferred as %s, %zuth is %s", type_to_str(sub_t), i, type_to_str(rest_t));
			THROW_EXCEPT();
		    }
		}
		TypeFull list = {.ty = TY_LIST, .more = sub_t.i};
		return intern_ty(list);
	    }
	case EXPR_SCALE:
	    {
		Type pitch_ty = intern_simple_ty(TY_PITCH);
		Type int_ty = intern_simple_ty(TY_INT);
		Type mode_ty = intern_simple_ty(TY_MODE);
		sub_t = sema_analy_expr(ctx, expr->data.scale.tonic, sec_idx);
		assert_type(sub_t, pitch_ty, expr->off);
		sub_t = sema_analy_expr(ctx, expr->data.scale.octave, sec_idx);
		assert_type(sub_t, int_ty, expr->off);
		sub_t = sema_analy_expr(ctx, expr->data.scale.mode, sec_idx);
		assert_type(sub_t, mode_ty, expr->off);
		return intern_simple_ty(TY_SCALE);
	    }
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
	case EXPR_PREFIX:
	{
	    assert(expr->data.prefix.op.type == '$');
	    sub_t = sema_analy_expr(ctx, expr->data.prefix.expr, sec_idx);
	    TypeFull sub_full = lookup_ty(sub_t);    
	    if (sub_full.ty != TY_LIST) {
		report(ctx->lexer, expr->off, "Can not spread non-list type: %s", type_to_str(sub_t));
		THROW_EXCEPT();
	    }
	    sub_full.ty = TY_SPREAD;
	    return intern_ty(sub_full);
	}
	case EXPR_IF:
	    {	
		ExprIdx cond_expr = expr->data.if_then_else.cond_expr;
		ExprIdx then_expr = expr->data.if_then_else.then_expr;
		ExprIdx else_expr = expr->data.if_then_else.else_expr;
		Type cond_t = sema_analy_expr(ctx, cond_expr, sec_idx);
		Type then_t = sema_analy_expr(ctx, then_expr, sec_idx);
		Type else_t = sema_analy_expr(ctx, else_expr, sec_idx);

		Type bool_ty = intern_simple_ty(TY_BOOL);
		assert_type(cond_t, bool_ty, ast_get(ctx->pgm, exprs, cond_expr).off);
		size_t then_off = ast_get(ctx->pgm, exprs, then_expr).off;
		if (ty_coerce(ctx, sec_idx, else_t, then_t, then_off).i == then_t.i) {
		    return then_t;
		} else if (ty_coerce(ctx, sec_idx, then_t, else_t, then_off).i == else_t.i) {
		    return else_t;
		}
		report(ctx->lexer, then_off, "two branches of an if expression must have the same type");
		THROW_EXCEPT();

	    }
	case EXPR_FOR:
	    {	
		Type int_ty = intern_simple_ty(TY_INT);
		ExprIdx lower_expr = expr->data.for_expr.lower_bound;	  	  
		ExprIdx upper_expr = expr->data.for_expr.upper_bound;
		SliceOf(AstIdx) body = expr->data.for_expr.body;

		Type lower_ty = sema_analy_expr(ctx, lower_expr, sec_idx);
		Type upper_ty = sema_analy_expr(ctx, upper_expr, sec_idx);
		assert_type(lower_ty, int_ty, ast_get(ctx->pgm, exprs, lower_expr).off);
		assert_type(upper_ty, int_ty, ast_get(ctx->pgm, exprs, upper_expr).off);
		sub_t = intern_simple_ty(TY_ANY);
		for (size_t i = 0; i < body.len; ++i)
		{
		    Type rest_t = sema_analy_expr(ctx, body.ptr[i], sec_idx);
		    if (sub_t.i == ty_coerce(ctx, sec_idx, rest_t, sub_t, expr->off).i) {
			// does nothing	
		    } else if (rest_t.i == ty_coerce(ctx, sec_idx, sub_t, rest_t, expr->off).i) {
			sub_t = rest_t;
		    } else {
			report(ctx->lexer, expr->off, "Type mistmatched in for, previously inferred as %s, %zuth is %s", type_to_str(sub_t), i, type_to_str(rest_t));
			THROW_EXCEPT();
		    }
		}
		TypeFull list = {.ty = TY_SPREAD, .more = sub_t.i};
		return intern_ty(list);
	    }
	default:
	    eprintf("tag %i\n", expr->tag);
	    assert(false && "unknown tag");
    }

    assert(false && "unreachable");
}
