#include "compiler.h"
#include "assert.h"
#include "ast.h"
#include "ds.h"
#include "lexer.h"
#include "sema.h"

make_arr(Pitch);
size_t eval_chord_recur(Context *ctx, ExprIdx idx, Scale *scale, SecIdx sec_idx);

SliceOf(Track) eval_pgm(Context *ctx)
{
	for (ssize_t fi = 0; fi < ast_len(ctx->pgm, toplevel); fi++)
	{
		eval_formal(ctx, ast_get(ctx->pgm, toplevel, fi), -1);
	}
	Val main = hmget(ctx->pgm_env, ctx->builtin_syms.main);
	if (main.ty == TY_SEC)
	{
		SliceOf(Track) res = {.ptr = calloc(1, sizeof(Track)), .len = 1};

		res.ptr[0] = main.data.sec;
		for (size_t i = 0; i < 10; ++i)
		{
		}
		return res;
	}
	else
	{
		return main.data.chorus;
	}
}
void eval_formal(Context *ctx, FormalIdx idx, SecIdx sec_idx)
{
	Formal *formal = &ast_get(ctx->pgm, formals, idx);
	ValEnv *env = get_curr_env(ctx, sec_idx);
	ValData v = eval_expr(ctx, formal->expr, sec_idx);
	hmgetp(*env, formal->ident)->value.data = v;
}

static const SecConfig default_config = {.bpm = 120, .scale = (Scale){.mode = MODE_MAJ, .tonic = PTCH_C, .octave = 5}, .instr = 2};
Track eval_section(Context *ctx, SecIdx idx)
{

	Sec *sec = &ast_get(ctx->pgm, secs, idx);
	for (ssize_t fi = 0; fi < ast_len(sec, vars); ++fi)
	{
		eval_formal(ctx, ast_get(sec, vars, fi), idx);
	}
	for (ssize_t fi = 0; fi < ast_len(sec, config); ++fi)
	{
		eval_formal(ctx, ast_get(sec, config, fi), idx);
	}
	Track track = {.labels = (SliceOf(Label)){.ptr = calloc(sizeof(Label), sec->labels.len), .len = sec->labels.len}};
	for (size_t ti = 0; ti < sec->labels.len; ++ti)
	{
		track.labels.ptr[ti] = ast_get(ctx->pgm, labels, sec->labels.ptr[ti]);
	}
	ptrdiff_t env_i;
	ValEnv *env = &ctx->sec_envs.ptr[idx];
	env_i = hmgeti(*env, ctx->builtin_syms.bpm);
	if (env_i < 0)
		hmput(*env, ctx->builtin_syms.bpm, (Val){.data.i = default_config.bpm});
	env_i = hmgeti(*env, ctx->builtin_syms.instrument);
	if (env_i < 0)
		hmput(*env, ctx->builtin_syms.instrument, (Val){.data.i = default_config.instr});
	env_i = hmgeti(*env, ctx->builtin_syms.scale);
	if (env_i < 0)
		hmput(*env, ctx->builtin_syms.scale, (Val){.data.scale = default_config.scale});
	track.notes = (ArrOf(Note)) {0};
	for (ssize_t ni = 0; ni < ast_len(sec, note_exprs); ++ni)
	{
		if (ctx->types.ptr[ni] == TY_VOID) continue;
		ExprIdx expr = ast_get(sec, note_exprs, ni);
		ValData note = eval_expr(ctx, expr, idx);
		Type ty = ctx->types.ptr[expr];
		if (TY_NOTE == ty) {
			da_append(track.notes, note.note);
		} else if (TY_FOR == ty) {
			da_append_slice(track.notes, note.notes);
		} else {
			assert(false && "unexpected type");
		}
	}
	track.config.bpm = hmget(*env, ctx->builtin_syms.bpm).data.i;
	track.config.instr = hmget(*env, ctx->builtin_syms.instrument).data.i;
	track.config.scale = hmget(*env, ctx->builtin_syms.scale).data.scale;
	return track;
}
// The type checking is already done so this function should always succeed
// TODO bound checks for int to degree
ValData val_coerce(Context *ctx, ValData *from, Type from_ty, Type to)
{
	if (from_ty == to)
		return *from;
	ValData res;
	if (to == TY_CHORD)
	{
		res.chord = (SliceOf(Pitch)){.ptr = calloc(sizeof(Pitch), 1), .len = 1};
		if (from_ty == TY_INT)
		{
			res.chord.ptr[0].is_abs = false;
			res.chord.ptr[0].data.deg = (Degree){.shift = 0, .degree = from->i};
		}
		else if (from_ty == TY_DEGREE)
		{
			res.chord.ptr[0].is_abs = false;
			res.chord.ptr[0].data.deg = from->deg;
		}
		else if (from_ty == TY_ABSPITCH)
		{
			res.chord.ptr[0].is_abs = true;
			res.chord.ptr[0].data.abs = from->i;
		}
		else if (from_ty == TY_PITCH)
		{
			res.chord.ptr[0] = from->pitch;
		}
		return res;
	}
	else if (to == TY_PITCH)
	{
		if (from_ty == TY_INT)
		{
			res.pitch.is_abs = false;
			res.pitch.data.deg = (Degree){.degree = from->i, .shift = 0};
		}
		else if (from_ty == TY_DEGREE)
		{
			res.pitch.is_abs = false;
			res.pitch.data.deg = from->deg;
		}
		else if (from_ty == TY_ABSPITCH)
		{
			res.chord.ptr[0].is_abs = true;
			res.chord.ptr[0].data.abs = from->i;
		}
		return res;
	}
	else if (to == TY_CHORUS)
	{
		res.chorus = (SliceOf(Track)){.ptr = calloc(sizeof(Track), 1), .len = 1};
		res.chorus.ptr[0] = from->sec;
		return res;
	}
	assert(false && "unexpected coercion");
}
ValData eval_expr(Context *ctx, ExprIdx idx, SecIdx sec_idx)
{
	Expr *expr = &ast_get(ctx->pgm, exprs, idx);
	ValEnv *env = get_curr_env(ctx, sec_idx);
	Scale *scale = NULL;
	ValData res;
	ptrdiff_t env_i;
	switch (expr->tag)
	{
	case EXPR_NUM:
		return (ValData){.i = expr->data.num};
	case EXPR_BOOL:
		return (ValData){.i = expr->data.num};
	case EXPR_VOID:
		return (ValData) {0};
	case EXPR_IDENT:
		env_i = -1;
		if (sec_idx >= 0)
		{
			env_i = hmgeti(ctx->sec_envs.ptr[sec_idx], expr->data.ident);
			if (env_i >= 0)
				return ctx->sec_envs.ptr[sec_idx][env_i].value.data;
		}
		res = hmget(ctx->pgm_env, expr->data.ident).data;
		return res;
	case EXPR_SCALE:
		return (ValData){.scale = eval_scale(ctx, idx, sec_idx)};
	case EXPR_NOTE:
	{
		ExprIdx sub_expr = expr->data.note.expr;
		ValData sub_data = eval_expr(ctx, sub_expr, sec_idx);
		sub_data = val_coerce(ctx, &sub_data, ctx->types.ptr[sub_expr], TY_CHORD);
		res.note.dots = expr->data.note.dots;
		res.note.chord = sub_data.chord;
		return res;
	}
	case EXPR_SEC:
		res = (ValData){.sec = eval_section(ctx, expr->data.sec)};
		return res;
	case EXPR_INFIX:
	{
		ExprIdx lhs = expr->data.infix.lhs;
		ExprIdx rhs = expr->data.infix.rhs;
		ValData lhs_v = eval_expr(ctx, lhs, sec_idx);
		ValData rhs_v = eval_expr(ctx, rhs, sec_idx);
		TokenType op = expr->data.infix.op.type;
		if (op == '&')
		{
			lhs_v = val_coerce(ctx, &lhs_v, ctx->types.ptr[lhs], TY_CHORUS);
			rhs_v = val_coerce(ctx, &rhs_v, ctx->types.ptr[rhs], TY_CHORUS);
			size_t lhs_len = lhs_v.chorus.len;
			size_t rhs_len = rhs_v.chorus.len;
			size_t res_len = lhs_len + rhs_len;
			res = (ValData){.chorus = (SliceOf(Track)){.ptr = calloc(sizeof(Track), res_len), .len = res_len}};
			memcpy(res.chorus.ptr, lhs_v.chorus.ptr, lhs_len * sizeof(Track));
			memcpy(res.chorus.ptr + lhs_len, rhs_v.chorus.ptr, rhs_len * sizeof(Track));
			return res;
		}
		else if (op == '\'')
		{
			int shift = lhs_v.i;
			// lhs_v = coerce(ctx, &v1, ctx->types.ptr[idx], TY_INT);
			rhs_v = val_coerce(ctx, &rhs_v, ctx->types.ptr[rhs], TY_CHORD);
			res.chord = rhs_v.chord;
			for (size_t ci = 0; ci < res.chord.len; ++ci)
			{
				if (res.chord.ptr[ci].is_abs)
				{
					res.chord.ptr[ci].data.abs += shift;
				}
				else
				{
					res.chord.ptr[ci].data.deg.shift += shift;
				}
			}
			return res;
		}
		else if (op == '+')
		{
			return (ValData){.i = lhs_v.i + rhs_v.i};
		}
		else if (op == '-')
		{
			return (ValData){.i = lhs_v.i - rhs_v.i};
		}
		else if (op == '*')
		{
			return (ValData){.i = lhs_v.i * rhs_v.i};
		}
		else if (op == TK_EQ)
		{
			return (ValData){.i = lhs_v.i == rhs_v.i};
		}
		assert(false && "unknown infix op");
	}
	case EXPR_CHORD:
	{

		static ArrOf(Pitch) pitches_tmp = {0};
		for (size_t i = 0; i < expr->data.chord_notes.len; ++i)
		{
			ExprIdx sub_expr = expr->data.chord_notes.ptr[i];
			ValData sub_v = eval_expr(ctx, sub_expr, sec_idx);
			sub_v = val_coerce(ctx, &sub_v, ctx->types.ptr[sub_expr], TY_CHORD);
			da_append_slice(pitches_tmp, sub_v.chord);
			// free(sub_v.chord.ptr);
			// printf("res.chord %zu\n", pitches_tmp.size);
		}
		da_move(pitches_tmp, res.chord);
		// printf("here\n");
		//
		return res;
	}
	case EXPR_IF:
	{
		ExprIdx cond_expr = expr->data.if_then_else.cond_expr;
		ExprIdx then_expr = expr->data.if_then_else.then_expr;
		ExprIdx else_expr = expr->data.if_then_else.else_expr;	
		bool cond = eval_expr(ctx, cond_expr, sec_idx).i;
		ExprIdx branch = cond ? then_expr : else_expr;
		return eval_expr(ctx, branch, sec_idx);
	}
	case EXPR_FOR:
	{
		ExprIdx lower_expr = expr->data.for_expr.lower_bound;	  	  
		ExprIdx upper_expr = expr->data.for_expr.upper_bound;
		SliceOf(AstIdx) body = expr->data.for_expr.body;

		ValData lower_v = eval_expr(ctx, lower_expr, sec_idx);
		ValData upper_v = eval_expr(ctx, upper_expr, sec_idx);
		ssize_t start = lower_v.i;
		ssize_t end = upper_v.i + expr->data.for_expr.is_leq;
		assert(start <= end);
		ArrOf(Note) note_arr = {0};
			
		for (size_t loop = start; loop < end; ++loop)
			for (size_t i = 0; i < body.len; ++i) {
				ExprIdx body_expr = body.ptr[i];
				Type body_t = ctx->types.ptr[body_expr];
				ValData body_v = eval_expr(ctx, body_expr, sec_idx);
				if (TY_NOTE == body_t) {
					da_append(note_arr, body_v.note);
				} else if (TY_FOR == body_t) {
					da_append_slice(note_arr, body_v.notes);
				} else {
					assert(false && "unexpected type");	
				}
			}
		da_move(note_arr, res.notes)
		return res;
	}
	// case EXPR_PREFIX:
	default:
		eprintf("EXPR %i\n", expr->tag);
		assert(false && "unknown expr tag\n");
	}
}

// SliceOf(Pitch) eval_chord(Context *ctx, ExprIdx idx, Scale *scale, SecIdx sec_idx) {
//
//     eval_chord_recur(ctx, idx, scale, sec_idx);
//     SliceOf(Pitch) res = {0};
//     da_move(pitches_tmp, res);
//     return res;
// }
// // returns the number of pitch added to the chord
// size_t eval_chord_recur(Context *ctx, ExprIdx idx, Scale *scale, SecIdx sec_idx) {
//     Type ty = ctx->types.ptr[idx];
//     assert(ty == TY_INT || ty == TY_CHORD || ty == TY_PITCH);
//     Expr *expr = &ast_get(ctx->pgm, exprs, idx);
//     ExprTag tag = expr->tag;
//     if (tag == EXPR_NUM) {
// 	da_append(pitches_tmp, pitch_from_scale(scale, expr->data.num));
// 	return 1;
//     }
//     else if (tag == EXPR_CHORD) {
// 	size_t total = 0;
// 	for (size_t i = 0; i < expr->data.chord_notes.len; ++i) {
// 	    total += eval_chord_recur(ctx, expr->data.chord_notes.ptr[i], scale, sec_idx);
// 	}
// 	return total;
//     } else if (tag == EXPR_PREFIX) {
// 	assert(expr->data.prefix.op.type == TK_QUAL);
// 	size_t count = eval_chord_recur(ctx, expr->data.prefix.expr, scale, sec_idx);
// 	Token qual = expr->data.prefix.op;
// 	ssize_t shift =
// 	    - 12 * qual.data.qualifier.suboctave
// 	    + 12 * qual.data.qualifier.octave
// 	    - qual.data.qualifier.flats
// 	    + qual.data.qualifier.sharps;
// 	for (size_t i = pitches_tmp.size - count; i < pitches_tmp.size; ++i) {
// 	    pitches_tmp.items[i] += shift;
// 	}
// 	return count;
//
//     } else if (tag == EXPR_INFIX) {
// 	assert(expr->data.infix.op.type == '\'');
// 	size_t count = eval_chord_recur(ctx, expr->data.infix.rhs, scale, sec_idx);
// 	ssize_t shift = eval_expr(ctx, expr->data.infix.lhs, sec_idx).i;
// 	for (size_t i = pitches_tmp.size - count; i < pitches_tmp.size; ++i) {
// 	    pitches_tmp.items[i] += shift;
// 	}
// 	return count;
//
//     }
//     eprintf("EXPR %i\n", expr->tag);
//     assert(false && "unreachable");
//
//
// }

// ssize_t eval_int(Context *ctx, ExprIdx idx) {
//     Type ty = ctx->types.ptr[idx];
//     assert(ty == TY_INT);
//     return ast_get(ctx->pgm, exprs, idx).data.num;
// }
// TODO memory
// Mode eval_mode(Context *ctx, ExprIdx idx) {
//     Symbol mode = ast_get(ctx->pgm, exprs, idx).data.ident;
//     if (mode == ctx->builtin_syms.maj) return MODE_ION;
//     if (mode == ctx->builtin_syms.min) return MODE_AEOL;
//
//     for (size_t i = 0; i < DIATONIC; ++i) {
// 	if (mode == ctx->builtin_syms.modes[i]) return i;
//     }
//     assert(false && "unreachable");
// }
// Pitch eval_pitch(Context *ctx, ExprIdx idx) {
//     Symbol pitch = ast_get(ctx->pgm, exprs, idx).data.ident;
//        for (size_t i = 0; i < DIATONIC; ++i) {
// 	if (pitch == ctx->builtin_syms.pitches[i]) return BASE_MODE[i];
//     }
//     assert(false && "unreachable");
//
// }
Scale eval_scale(Context *ctx, ExprIdx idx, SecIdx sec_idx)
{
	assert(ctx->types.ptr[idx] == TY_SCALE);
	Scale scale = {0};
	Expr *expr = &ast_get(ctx->pgm, exprs, idx);
	scale.tonic = eval_expr(ctx, expr->data.scale.tonic, sec_idx).i;
	// TODO check for bounds
	scale.octave = eval_expr(ctx, expr->data.scale.octave, sec_idx).i;
	scale.mode = eval_expr(ctx, expr->data.scale.mode, sec_idx).i;
	return scale;
}

AbsPitch abspitch_from_scale(Scale *scale, size_t degree)
{
	// eprintf("degree %zu\n", degree);
	// eprintf("tonic %zu\n", scale->tonic);
	assert(degree <= DIATONIC && degree >= 1 && "degree out of bound");
	size_t base = scale->tonic + scale->octave * 12;
	// degree is 1-based
	size_t degree_shift = degree + scale->mode - 1;
	// Step is how much we should walk from the tonic
	size_t step = degree_shift < DIATONIC
					  ? BASE_MODE[degree_shift] - BASE_MODE[scale->mode]
					  : 12 + BASE_MODE[degree_shift % DIATONIC] - BASE_MODE[scale->mode];
	return base + step;
}

AbsPitch resolve_pitch(Scale *scale, Pitch pitch)
{
	if (pitch.is_abs)
		return pitch.data.abs;
	return abspitch_from_scale(scale, pitch.data.deg.degree) + pitch.data.deg.shift;
}
