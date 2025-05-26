#ifndef COMPILER_H_
#define COMPILER_H_
#include "ast.h"
#include "sema.h"
#include "ds.h"

// https://en.wikipedia.org/wiki/Mode_(music)
struct CpGen; // temp storage for intermediate objects generateed during compilation
typedef struct CpGen CpGen;
// 1 <= degree <= 7
// returns absolute pitch
AbsPitch abspitch_from_scale(Scale *scale, size_t degree);
AbsPitch resolve_pitch(Scale *scale, Pitch pitch);

Scale eval_scale(Context *scale, ExprIdx idx, SecIdx sec_idx);
SecConfig eval_config(Context *ctx, SecIdx idx);


SliceOf(Track) eval_pgm(Context* ctx);
void eval_formal(Context* ctx, FormalIdx idx, SecIdx sec_idx);
Track eval_section(Context* ctx, SecIdx idx);
ValData eval_expr(Context* ctx, ExprIdx idx, SecIdx sec_idx);


#endif
