#ifndef COMPILER_H_
#define COMPILER_H_
#include "ast.h"
#include "sema.h"
#include "ds.h"
make_slice(Pitch);

// https://en.wikipedia.org/wiki/Mode_(music)

struct CpGen; // temp storage for intermediate objects generateed during compilation
typedef struct CpGen CpGen;
// returns a shallow copy. The next call to eval_chord invalidates the slice
// The pithches are absolute, from 0~128
SliceOf(Pitch) eval_chord(Context *ctx, ExprIdx idx);
ssize_t eval_int(Context *ctx, ExprIdx idx);
Scale eval_scale(Context *scale, ExprIdx idx);
SecConfig eval_config(Context *ctx, SecIdx idx);


#endif
