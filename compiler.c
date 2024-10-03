#include "compiler.h"
#include "assert.h"

// 1 <= degree <= 7
Pitch pitch_from_scale(Scale *scale, size_t degree) {
    assert(degree <= DIATONIC && degree >= 1 && "degree out of bound");
    assert(scale->tonic < DIATONIC && scale->tonic >= 0 && "tonic out of bound");
    size_t degree_shift = degree + scale->mode - 1;
    size_t base = scale->tonic + scale->octave * 12;
    if (degree_shift < DIATONIC) return base + BASE_MODE[degree_shift] - BASE_MODE[scale->tonic];
    else return base +  BASE_MODE[degree_shift % DIATONIC] - BASE_MODE[scale->tonic] + 12;
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


SecConfig eval_config(Context *ctx, SecIdx idx) {
    
}
