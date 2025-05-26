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
    for (ssize_t fi = 0; fi < (ssize_t)ast_len(ctx->pgm, toplevel); fi++)
    {
	eval_formal(ctx, ast_get(ctx->pgm, toplevel, fi), -1);
    }
    Val main = hmget(ctx->pgm_env, ctx->builtin_syms.main);
    TypeFull main_ty = lookup_ty(main.ty);
    SliceOf(Track) tracks = {0};
    Type sec_ty = intern_simple_ty(TY_SEC);
    if (main.ty.i == sec_ty.i)
    {
	tracks = (SliceOf(Track)) {.ptr = calloc(1, sizeof(Track)), .len = 1};
	tracks.ptr[0] = main.data.sec;
	return tracks;
    }
    else if (main_ty.ty == TY_LIST && main_ty.more.i == sec_ty.i)
    {
	size_t len = main.data.list.len;
	tracks = (SliceOf(Track)) {.ptr = calloc(len, sizeof(Track)), .len = len};
	for (size_t i = 0; i < len; ++i) {
	    tracks.ptr[i] = main.data.list.ptr[i].sec;
	}
    }
    return tracks;
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
    for (ssize_t fi = 0; fi < (ssize_t)ast_len(sec, vars); ++fi)
    {
	eval_formal(ctx, ast_get(sec, vars, fi), idx);
    }
    for (ssize_t fi = 0; fi < (ssize_t)ast_len(sec, config); ++fi)
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
    for (ssize_t ni = 0; ni < (ssize_t)ast_len(sec, note_exprs); ++ni)
    {
	ExprIdx expr = ast_get(sec, note_exprs, ni);
	ValData note = eval_expr(ctx, expr, idx);
	Type ty = ctx->types.ptr[expr];

	// note = va(ctx, &note, ty, 0);

	TypeFull body_full = lookup_ty(ty);
	if (body_full.ty == TY_SPREAD) {
	    // da_append_slice(track.notes, note.list);
	    size_t len = note.list.len;
	    for (size_t i = 0; i < len; ++i) {
		da_append(track.notes, note.list.ptr[i].note);
	    }
	} else {
	    da_append(track.notes, note.note);
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
    // eprintf("from %s, to %s\n", type_to_str(from_ty), type_to_str(to));
    (void)(ctx);
    if (from_ty.i == to.i)
	return *from;
    ValData res;
    TypeFull to_full = lookup_ty(to);
    TypeFull from_full = lookup_ty(from_ty);
    // TypeFull from_full = lookup_ty(from_ty);
    Type int_ty = intern_simple_ty(TY_INT);
    Type pitch_ty = intern_simple_ty(TY_PITCH);
    Type abspitch_ty = intern_simple_ty(TY_ABSPITCH);
    Type degree_ty = intern_simple_ty(TY_DEGREE);
    if (to_full.ty == TY_LIST && from_full.ty == TY_LIST) {
	size_t len = from->list.len;
	res.list = (SliceOf(ValData)) {.len = len, .ptr = calloc(len, sizeof(ValData))};
	for (size_t i = 0; i < len; ++i) {
	    res.list.ptr[i] = val_coerce(ctx, &from->list.ptr[i], from_full.more, to_full.more);
	}
	return res;
    }
    if (from_full.ty == TY_SPREAD) {
	size_t len = from->list.len;
	res.list = (SliceOf(ValData)) {.len = len, .ptr = calloc(len, sizeof(ValData))};
	for (size_t i = 0; i < len; ++i) {
	    res.list.ptr[i] = val_coerce(ctx, &from->list.ptr[i], from_full.more, to);
	}
	return res;
    }
    if (to_full.ty == TY_LIST && to_full.more.i == pitch_ty.i)
    {

	res.list = (SliceOf(ValData)){.ptr = calloc(sizeof(ValData), 1), .len = 1};
	if (from_ty.i == int_ty.i)
	{
	    res.list.ptr[0].pitch.is_abs = false;
	    res.list.ptr[0].pitch.data.deg = (Degree){.shift = 0, .degree = from->i};
	}
	else if (from_ty.i == degree_ty.i)
	{
	    res.list.ptr[0].pitch.is_abs = false;
	    res.list.ptr[0].pitch.data.deg = from->deg;
	}
	else if (from_ty.i == abspitch_ty.i)
	{
	    res.list.ptr[0].pitch.is_abs = true;
	    res.list.ptr[0].pitch.data.abs = from->i;
	}
	else if (from_ty.i == pitch_ty.i)
	{
	    res.list.ptr[0].pitch = from->pitch;
	} else {
	    assert(false && "unreachable");
	}
	return res;
    }
    else if (to.i == intern_simple_ty(TY_PITCH).i)
    {
	if (from_ty.i == int_ty.i)
	{
	    res.pitch.is_abs = false;
	    res.pitch.data.deg = (Degree){.degree = from->i, .shift = 0};
	}
	else if (from_ty.i == degree_ty.i)
	{
	    res.pitch.is_abs = false;
	    res.pitch.data.deg = from->deg;
	}
	else if (from_ty.i == abspitch_ty.i)
	{
	    res.pitch.is_abs = true;
	    res.pitch.data.abs = from->i;
	} else {
	    assert(false && "unreachable");
	}
	return res;
    }
    else if (to.i == degree_ty.i) {
	res.deg.degree = from->i;
	res.deg.shift = 0;
	return res;
    }
    res = *from;
    return res;
}
ValData shift_pitch(Type ty, ValData val, int shift) {
    // lhs_v = coerce(ctx, &v1, ctx->types.ptr[idx], TY_INT);
    TypeFull rhs_full = lookup_ty(ty);
    Type deg_ty = intern_simple_ty(TY_DEGREE);
    Type abspitch_ty = intern_simple_ty(TY_ABSPITCH);
    Type pitch_ty = intern_simple_ty(TY_PITCH);
    if (rhs_full.ty == TY_INT) {
	val.deg.degree = val.i;
	val.deg.shift = shift;
    } else if (val.i == deg_ty.i) {
	val.deg.shift += shift;
    } else if (val.i == abspitch_ty.i) {
	val.i += shift;
    } else if (val.i == pitch_ty.i) {
	if (val.pitch.is_abs) val.pitch.data.abs += shift;
	else val.pitch.data.deg.shift += shift;
    } else if (rhs_full.ty == TY_LIST) {
	size_t len = val.list.len;
	for (size_t i = 0; i < len; ++i) {
	    val.list.ptr[i] = shift_pitch(rhs_full.more, val.list.ptr[i], shift);
	}
    }
    return val;
}
ValData eval_expr(Context *ctx, ExprIdx idx, SecIdx sec_idx)
{
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    Type type = ctx->types.ptr[idx];
    ValData res;
    ptrdiff_t env_i;

    TypeFull chord_ty_full = {.ty = TY_LIST, .more = intern_simple_ty(TY_PITCH).i };
    Type chord_ty = intern_ty(chord_ty_full);
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
		sub_data = val_coerce(ctx, &sub_data, ctx->types.ptr[sub_expr], chord_ty);
		size_t len = sub_data.list.len;
		SliceOf(Pitch) chord = {.ptr = calloc(len, sizeof(Pitch)), .len = len};
		for (size_t i = 0; i < sub_data.list.len; ++i) {
		    chord.ptr[i] = sub_data.list.ptr[i].pitch;
		}
		res.note.dots = expr->data.note.dots;
		res.note.chord = chord;
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
		switch (op)
		{

		    case '\'':
			{
			    int shift = lhs_v.i;
			    // lhs_v = coerce(ctx, &v1, ctx->types.ptr[idx], TY_INT);
			    Type rhs_t = ctx->types.ptr[rhs];
			    return shift_pitch(rhs_t, rhs_v, shift);
			}
		    case '+':
			return (ValData){.i = lhs_v.i + rhs_v.i};
		    case '-':
			return (ValData){.i = lhs_v.i - rhs_v.i};
		    case '*':
			return (ValData){.i = lhs_v.i * rhs_v.i};
		    case '>':
			return (ValData){.i = lhs_v.i > rhs_v.i};
		    case '<':
			return (ValData){.i = lhs_v.i < rhs_v.i};
		    case TK_EQ:
			return (ValData){.i = lhs_v.i == rhs_v.i};
		    case TK_NEQ:
			return (ValData){.i = lhs_v.i != rhs_v.i};
		    case TK_LEQ:
			return (ValData){.i = lhs_v.i <= rhs_v.i};
		    case TK_GEQ:
			return (ValData){.i = lhs_v.i >= rhs_v.i};

		    default:
			assert(false && "unknown infix op");
			break;
		}
		case EXPR_PREFIX:
		{
		    assert(expr->data.prefix.op.type == '$');
		    res =  eval_expr(ctx, expr->data.prefix.expr, sec_idx);  
		    return res;
		}
		case EXPR_LIST:
		{
		    static ArrOf(ValData) list_tmp = {0};
		    TypeFull ty_full = lookup_ty(type);
		    Type sub_t = {.i = ty_full.more.i};
		    for (size_t i = 0; i < expr->data.chord_notes.len; ++i)
		    {
			ExprIdx sub_expr = expr->data.chord_notes.ptr[i];
			ValData sub_v = eval_expr(ctx, sub_expr, sec_idx);
			Type sub_el_t = ctx->types.ptr[sub_expr];
			TypeFull sub_el_full = lookup_ty(sub_el_t);
			sub_v = val_coerce(ctx, &sub_v, sub_el_t, sub_t);
			if (sub_el_full.ty == TY_SPREAD) {
			    da_append_slice(list_tmp, sub_v.list);
			} else {
			    da_append(list_tmp, sub_v);
			}
			// free(sub_v.chord.ptr);
			// printf("res.chord %zu\n", pitches_tmp.size);
		    }
		    da_move(list_tmp, res.list);
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
		    // TODO more efficient array since it is homogenous
		    ArrOf(ValData) val_arr = {0};
		    Type sub_t = {.i = lookup_ty(type).more.i};
		    for (ssize_t loop = start; loop < end; ++loop)
			for (size_t i = 0; i < body.len; ++i) {
			    ExprIdx body_expr = body.ptr[i];
			    Type body_t = ctx->types.ptr[body_expr];
			    ValData body_v = eval_expr(ctx, body_expr, sec_idx);
			    body_v = val_coerce(ctx, &body_v, body_t, sub_t);

			    TypeFull body_full = lookup_ty(body_t);
			    if (body_full.ty == TY_SPREAD) {
				da_append_slice(val_arr, body_v.list);
			    } else {
				da_append(val_arr, body_v);
			    }
			}
		    da_move(val_arr, res.list);
		    return res;
		}
		// case EXPR_PREFIX:
		default:
		eprintf("EXPR %i\n", expr->tag);
		assert(false && "unknown expr tag\n");

	    }
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
    assert(ctx->types.ptr[idx].i == intern_simple_ty(TY_SCALE).i);
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
    // eprintf("tonic %i\n", scale->tonic);
    // eprintf("degree %zu\n", degree);
    // eprintf("octave %i\n", scale->octave);
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
