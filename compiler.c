#include "compiler.h"
#include "assert.h"
#include "ds.h"
#include "sema.h"



make_arr(Pitch);
static ArrOf(Pitch) pitches_tmp = {0};
size_t eval_chord_recur(Context *ctx, ExprIdx idx, Scale *scale);


SliceOf(Pitch) eval_chord(Context *ctx, ExprIdx idx, Scale *scale) {

    eval_chord_recur(ctx, idx, scale);
    SliceOf(Pitch) res = {.ptr = pitches_tmp.items, .len = pitches_tmp.size};
 
    pitches_tmp.size = 0;
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

static const SecConfig default_config = {.bpm = 120, .scale = (Scale) {.mode = MODE_MAJ, .tonic = PTCH_C, .octave = 5}};
SecConfig eval_config(Context *ctx, SecIdx idx) {
    SecConfig config = default_config;
    Sec *sec = &ast_get(ctx->pgm, secs, idx);
    for (size_t i = 0; i < ast_len(sec, config); ++i) {
	Formal *formal = &ast_get(ctx->pgm, formals, ast_get(sec, config, i));
	if (formal->ident == ctx->builtin_syms.scale) {
	    config.scale = eval_scale(ctx, formal->expr);
	} else if (formal->ident == ctx->builtin_syms.bpm) {
	   config.bpm = eval_int(ctx, formal->expr); 
	} else assert(false && "unreachable");
    }
    return config;
}
ssize_t eval_int(Context *ctx, ExprIdx idx) {
    Type ty = ctx->types.ptr[idx];
    assert(ty == TY_INT);
    return ast_get(ctx->pgm, exprs, idx).data.num;
}
// TODO memory
Mode eval_mode(Context *ctx, ExprIdx idx) {
    Symbol mode = ast_get(ctx->pgm, exprs, idx).data.ident;
    if (mode == ctx->builtin_syms.maj) return MODE_ION;
    if (mode == ctx->builtin_syms.min) return MODE_AEOL;

    for (size_t i = 0; i < DIATONIC; ++i) {
	if (mode == ctx->builtin_syms.modes[i]) return i;
    }
    assert(false && "unreachable");
}
Pitch eval_pitch(Context *ctx, ExprIdx idx) {
    Symbol pitch = ast_get(ctx->pgm, exprs, idx).data.ident;
       for (size_t i = 0; i < DIATONIC; ++i) {
	if (pitch == ctx->builtin_syms.pitches[i]) return BASE_MODE[i];
    }
    assert(false && "unreachable");

}
Scale eval_scale(Context *ctx, ExprIdx idx) {
    assert(ctx->types.ptr[idx] == TY_SCALE);
    Scale scale = {0};
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    scale.tonic = eval_pitch(ctx, expr->data.scale.tonic);
    // TODO check for bounds
    scale.octave = eval_int(ctx, expr->data.scale.octave);
    scale.mode = eval_mode(ctx, expr->data.scale.mode);
    return scale; 
}
