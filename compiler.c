#include "compiler.h"
#include "assert.h"
#include "ast.h"
#include "ds.h"
#include "sema.h"



make_arr(Pitch);
static ArrOf(Pitch) pitches_tmp = {0};
size_t eval_chord_recur(Context *ctx, ExprIdx idx, Scale *scale);

SliceOf(Track) eval_pgm(Context* ctx) {
    for (ssize_t fi = 0; fi < ast_len(ctx->pgm, toplevel); fi++) {
	eval_formal(ctx, ast_get(ctx->pgm, toplevel, fi), -1);
    }
    Track main =  hmget(ctx->pgm_env, ctx->builtin_syms.main).data.sec;
    SliceOf(Track) res = {.ptr = calloc(1, sizeof(Track)), .len = 1 }; 
    	
    res.ptr[0] = main;
    for (size_t i = 0; i < 10; ++i) {
    }
    return res;
}
void eval_formal(Context* ctx, FormalIdx idx, SecIdx sec_idx) {
    Formal* formal = &ast_get(ctx->pgm, formals, idx);
    ValEnv *env = get_curr_env(ctx, sec_idx);
    ValData v = eval_expr(ctx, formal->expr, sec_idx);
    hmgetp(*env, formal->ident)->value.data = v;
}
make_arr(Note);


static const SecConfig default_config = {.bpm = 120, .scale = (Scale) {.mode = MODE_MAJ, .tonic = PTCH_C, .octave = 5}, .instr = 2};
Track eval_section(Context* ctx, SecIdx idx) {

    Sec *sec = &ast_get(ctx->pgm, secs, idx);
    for (ssize_t fi = 0; fi < ast_len(sec, vars); ++fi) {
	eval_formal(ctx, ast_get(sec, vars, fi), idx);
    }
    for (ssize_t fi = 0; fi < ast_len(sec, config); ++fi) {
	eval_formal(ctx, ast_get(sec, config, fi), idx);
    }
    Track track = {};
    ptrdiff_t env_i;
    ValEnv *env = &ctx->sec_envs.ptr[idx];
    env_i = hmgeti(*env, ctx->builtin_syms.bpm);
    if (env_i < 0) hmput(*env, ctx->builtin_syms.bpm, (Val) {.data.i = default_config.bpm});
    env_i = hmgeti(*env, ctx->builtin_syms.instrument);
    if (env_i < 0) hmput(*env, ctx->builtin_syms.instrument, (Val) {.data.i = default_config.instr});
    env_i = hmgeti(*env, ctx->builtin_syms.scale);
    if (env_i < 0) hmput(*env, ctx->builtin_syms.scale, (Val) {.data.scale = default_config.scale});
    
    track.notes = (SliceOf(Note)) {.ptr = calloc(sizeof(Note), ast_len(sec, note_exprs)), .len = ast_len(sec, note_exprs) }; // TODO hacked
    for (ssize_t ni = 0; ni < ast_len(sec, note_exprs); ++ni) {
	ValData note = eval_expr(ctx, ast_get(sec, note_exprs, ni), idx);
	track.notes.ptr[ni] = note.note;
    }
    track.config.bpm = hmget(*env, ctx->builtin_syms.bpm).data.i;
    track.config.instr = hmget(*env, ctx->builtin_syms.instrument).data.i;
    track.config.scale = hmget(*env, ctx->builtin_syms.scale).data.scale;
    return track;
}
ValData eval_expr(Context* ctx, ExprIdx idx, SecIdx sec_idx) {
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    ValEnv *env = get_curr_env(ctx, sec_idx);
    Scale *scale = NULL;
    ValData v;
    ptrdiff_t env_i;
    switch (expr->tag) {
	case EXPR_NUM: return (ValData) {.i = expr->data.num };

	case EXPR_IDENT:
	    env_i = -1;
	    if (sec_idx > 0) {
		env_i = hmgeti(ctx->sec_envs.ptr[sec_idx], expr->data.ident);
		if (env_i >= 0) return ctx->sec_envs.ptr[sec_idx][env_i].value.data;
	    } 
	    return hmget(ctx->pgm_env, expr->data.ident).data;
	case EXPR_SCALE: return (ValData) { .scale = eval_scale(ctx, idx, sec_idx) };
	case EXPR_NOTE: 
	    scale = &hmgetp(*env, ctx->builtin_syms.scale)->value.data.scale; // scale must be defined in the current scope
	    return (ValData) { .note = (Note) { .chord = eval_chord(ctx, expr->data.note.expr, scale), .dots = expr->data.note.dots } };
	case EXPR_SEC:
	    v = (ValData) {.sec = eval_section(ctx, expr->data.sec) };
	    return v;
	case EXPR_PREFIX:
	case EXPR_CHORD:
	    assert(false && "unimplemented, currently hacked");

    }

}



SliceOf(Pitch) eval_chord(Context *ctx, ExprIdx idx, Scale *scale) {

    eval_chord_recur(ctx, idx, scale);
    SliceOf(Pitch) res = {0};
    da_move(pitches_tmp, res);
    return res;
}
// returns the number of pitch added to the chord
size_t eval_chord_recur(Context *ctx, ExprIdx idx, Scale *scale) {
    Type ty = ctx->types.ptr[idx];
    assert(ty == TY_INT || ty == TY_CHORD || ty == TY_PITCH);  
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    ExprTag tag = expr->tag;
    if (tag == EXPR_NUM) {
	da_append(pitches_tmp, pitch_from_scale(scale, expr->data.num));
	return 1;
    }
    else if (tag == EXPR_CHORD) {
	size_t total = 0;
	for (size_t i = 0; i < expr->data.chord_notes.len; ++i) {
	    total += eval_chord_recur(ctx, expr->data.chord_notes.ptr[i], scale);
	}
	return total;
    } else if (tag == EXPR_PREFIX) {
	size_t count = eval_chord_recur(ctx, expr->data.prefix.expr, scale);
	Token qual = expr->data.prefix.op;
	ssize_t shift = 
	    - 12 * qual.data.qualifier.suboctave 
	    + 12 * qual.data.qualifier.octave 
	    - qual.data.qualifier.flats 
	    + qual.data.qualifier.sharps;
	for (size_t i = pitches_tmp.size - count; i < pitches_tmp.size; ++i) {
	    pitches_tmp.items[i] += shift;
	}
	return count;

    }

    assert(false && "unreachable");
     

}


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
Scale eval_scale(Context *ctx, ExprIdx idx, SecIdx sec_idx) {
    assert(ctx->types.ptr[idx] == TY_SCALE);
    Scale scale = {0};
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    scale.tonic = eval_expr(ctx, expr->data.scale.tonic, sec_idx).i;
    // TODO check for bounds
    scale.octave = eval_expr(ctx, expr->data.scale.octave, sec_idx).i;
    scale.mode = eval_expr(ctx, expr->data.scale.mode, sec_idx).i;
    return scale; 
}
