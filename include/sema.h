#ifndef TYPE_H_
#define TYPE_H_
#include "ast.h"
#include "utils.h"

typedef int Pitch;
#define MODE_SYMS \
    X(ION) \
    X(DOR) \
    X(PHRYG) \
    X(LYD) \
    X(MIXOLYD) \
    X(AEOL) \
    X(LOCR)
#define X(x) MODE_##x,
typedef enum {
    MODE_SYMS
} Mode;
#undef X
#define MODE_MAJ MODE_ION
#define MODE_MIN MODE_AEOL
// midi starts at C
#define PTCH_C 0
#define PTCH_D 2
#define PTCH_E 4
#define PTCH_F 5
#define PTCH_G 7
#define PTCH_A 9
#define PTCH_B 11

// evaluted scale
typedef struct {
    Pitch tonic;
    Mode mode;
    int octave;
} Scale;
typedef struct {
    Scale scale;
    size_t bpm;
    int instr;
} SecConfig;

#define DIATONIC 7
static const int BASE_MODE[DIATONIC] = {0, 2, 4, 5, 7, 9, 11};
// 1 <= degree <= 7
// returns absolute pitch
Pitch pitch_from_scale(Scale *scale, size_t degree);
// x macro: https://en.wikipedia.org/wiki/X_macro
// TY_PITCH refers to abosulte pitches e.g. A C G#
// TY_DEGREE refers to relative position in a scale, 1 2 3 4 5 6 7
// TY_INT is implicitly coerced to TY_DEGREE if it is the direct child of the notes part of a section, or a chord.
#define TYPE_LISTX \
    X(TY_ERR) \
    X(TY_VOID) \
    X(TY_INT) \
    X(TY_PITCH) \
    X(TY_CHORD) \
    X(TY_NOTE) \
    X(TY_SCALE) \
    X(TY_MODE) \
    X(TY_SEC) \
    X(TY_CHORUS)
#define X(x) x,
typedef enum {
    TYPE_LISTX
} Type;
#undef X

// Each type corresponds to one thing in ValData
// INT -> int i
// PITCH -> int i
// DEGREE -> int i
// CHORD -> SliceOf(Pitch)
// NOTE -> Note
// SCALE -> Scale
// MODE -> Mode
// Sec -> SliceOf(Note)

const char *type_to_str(Type ty);


make_slice(Pitch);
typedef struct {
    SliceOf(Pitch) chord;
    int dots;
} Note;
make_slice(Note);
typedef struct {
    SliceOf(Note) notes;
    SecConfig config;
    SliceOf(Label) labels;
} Track;
make_slice(Track);
typedef union {
    int i; 
    SliceOf(Pitch) chord;
    Note note;
    Scale scale;
    Track sec;
    SliceOf(Track) chorus;
} ValData;
typedef struct {
    ValData data;
    Type ty;
} Val;


typedef struct {
    Symbol key;
    Type value; // could also holds a value in the future?
} TypeEntry;
typedef struct {
    Symbol key;
    Val value;
} ValEntry;
typedef TypeEntry * TypeEnv;
typedef ValEntry * ValEnv; 

make_slice(Type);
make_slice(TypeEnv);
make_slice(ValEnv);
#define CONFIG_SYMS \
    X(scale) \
    X(bpm) \
    X(instrument)
#define BUILTIN_SYMS \
    X(main)
#define PITCH_SYMS \
    X(C) \
    X(D) \
    X(E) \
    X(F) \
    X(G) \
    X(A) \
    X(B)
#define X(x) #x,
static const char* CONST_PITCH[DIATONIC] = { PITCH_SYMS };
static const char* CONST_MODE[DIATONIC] = { MODE_SYMS };
#undef X
typedef struct {
    Pgm* pgm;
    SecIdx main;
    Lexer* lexer;
    SliceOf(Type) types; // len == pgm->exprs.len
    ValEnv pgm_env; // top level env
    SliceOf(ValEnv) sec_envs; // one for each section, len == pgm->sections.len
    TypeEnv config_builtin;
    bool success;
    SymbolTable sym_table;
#define X(x) Symbol x;
    struct {
	CONFIG_SYMS
	BUILTIN_SYMS
	Symbol maj;
	Symbol min;
	Symbol pitches[DIATONIC];
	Symbol modes[DIATONIC];
    } builtin_syms;
#undef X
} Context;
ValEnv* get_curr_env(Context *ctx, SecIdx idx);
void context_deinit(Context *ctx);
void sema_analy(Pgm* pgm, Lexer* lexer, Context *ctx /* res arg */);
bool sema_analy_pgm(Context *ctx);
bool sema_analy_formal(Context *ctx, FormalIdx idx, bool builtin, SecIdx sec_idx);
Type sema_analy_expr(Context *ctx, ExprIdx idx, SecIdx sec_idx);
Type sema_analy_sec(Context *ctx, SecIdx idx);
#endif
