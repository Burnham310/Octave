#ifndef COMPILER_H_
#define COMPILER_H_
#include "ast.h"
#include "type.h"
#include "ds.h"
typedef int Pitch;
make_slice(Pitch);

// https://en.wikipedia.org/wiki/Mode_(music)
typedef enum {
    MODE_ION,
    MODE_DOR,
    MODE_PHRYG,
    MODE_LYD,
    MODE_MIXOLYD,
    MODE_AEOL,
    MODE_LOCR,
} Mode;
#define MODE_MAJ MODE_ION
#define MODE_MIN MODE_AEOL

#define PTCH_A 0
#define PTCH_B 2
#define PTCH_C 3
#define PTCH_D 5
#define PTCH_E 7
#define PTCH_F 8
#define PTCH_G 10
typedef struct {
    Pitch tonic;
    Mode mode;
    int octave;
} Scale;
typedef struct {
    Scale scale;
    size_t bpm;
    // Instr instr;
} SecConfig;
static const int BASE_MODE[] = {0, 2, 4, 5, 7, 9, 10};
static size_t DIATONIC = sizeof(BASE_MODE)/sizeof(BASE_MODE[0]);
// 1 <= degree <= 7
// returns absolute pitch
Pitch pitch_from_scale(Scale *scale, size_t degree);
struct CpGen; // temp storage for intermediate objects generateed during compilation
typedef struct CpGen CpGen;
// returns a shallow copy. The next call to eval_chord invalidates the slice
// The pithches are absolute, from 0~128
SliceOf(Pitch) eval_chord(Context *ctx, ExprIdx idx);
SecConfig eval_config(Context *ctx, SecIdx idx);


#endif
