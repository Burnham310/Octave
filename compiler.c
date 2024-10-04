#include "compiler.h"
#include "assert.h"
#include "ds.h"
#include "sema.h"

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

make_arr(Pitch);
static ArrOf(Pitch) pitches_tmp = {0};
size_t eval_chord_recur(Context *ctx, ExprIdx idx);


SliceOf(Pitch) eval_chord(Context *ctx, ExprIdx idx) {

    eval_chord_recur(ctx, idx);
    SliceOf(Pitch) res = {.ptr = pitches_tmp.items, .len = pitches_tmp.size};
 
    pitches_tmp.size = 0;
    return res;
}
// returns the number of pitch added to the chord
size_t eval_chord_recur(Context *ctx, ExprIdx idx) {
    Type ty = ctx->types.ptr[idx];
    assert(ty == TY_INT || ty == TY_CHORD);  
    Expr *expr = &ast_get(ctx->pgm, exprs, idx);
    if (ty == TY_INT) {
	da_append(pitches_tmp, expr->data.num);
	return 1;
    }
    else {
	for (size_t i = 0; i < expr->data.chord_notes.len; ++i) {
	    eval_chord_recur(ctx, expr->data.chord_notes.ptr[i]);
	}
	return expr->data.chord_notes.len;
    }
     

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
