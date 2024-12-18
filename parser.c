#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "parser.h"
#include "lexer.h"
#include "assert.h"
#include "utils.h"
typedef struct
{
    int lbp;
    int rbp;
} BP;
SecIdx parse_section(Lexer *lexer, Gen *gen);
ExprIdx parse_section_expr(Lexer *lexer, Gen *gen);
LabelIdx parse_label(Lexer *lexer, Gen *gen, size_t note_size);
FormalIdx parse_formal(Lexer *lexer, Gen *gen);
ExprIdx parse_expr(Lexer *lexer, Gen *gen);
ExprIdx parse_expr_climb(Lexer *lexer, Gen *gen, int min_bp);
ExprIdx parse_list(Lexer *lexer, Gen *gen);
ExprIdx parse_scale(Lexer *lexer, Gen *gen);
ExprIdx parse_prefix(Lexer *lexer, Gen *gen);
ExprIdx parse_if(Lexer *lexer, Gen *gen);
ExprIdx parse_for(Lexer *lexer, Gen *gen);
ExprIdx parse_atomic_expr(Lexer *lexer, Gen *gen);
int prefix_bp(Token tk);
int postfix_bp(Token tk);
BP infix_bp(Token tk);

make_arr(Label);
void expr_debug(Pgm *pgm, SymbolTable sym_table, ExprIdx idx)
{
    Expr expr = pgm->exprs.ptr[idx];
    switch (expr.tag)
    {
    case EXPR_NUM:
        printf("%zi", expr.data.num);
        break;
    case EXPR_BOOL:
        printf("%s", expr.data.num ? "true" : "false");
        break;
    case EXPR_IDENT:
        printf("%s", symt_lookup(sym_table, expr.data.ident));
        break;
    case EXPR_NOTE:
        printf("NOTE ");
        expr_debug(pgm, sym_table, expr.data.note.expr);
        printf(".%zu", expr.data.note.dots);
        break;
    case EXPR_LIST:
        printf("[ ");
        for (size_t i = 0; i < expr.data.chord_notes.len; ++i)
        {
            expr_debug(pgm, sym_table, expr.data.chord_notes.ptr[i]);
            printf(" ");
        }
        printf("]");
        break;
    case EXPR_SEC:
        printf("Sec");
    case EXPR_SCALE:
    case EXPR_IF:
    case EXPR_FOR:
    case EXPR_VOID:
    // case EXPR_PREFIX:
    case EXPR_INFIX:
        assert(false && "unimplemented");
    }
}
// These functions & macros provdide 4 functionalities:
// peeking into the next token for a specific token type, and return ERR if no match, and the state of the lexer is NOT restored
// peeking into the next token for a specific token type, and return NULL if no match, and the peeked token is restored
// peeking into the next token for a specific token type, and execute custom code if no match
// if a TK_ERR is generated,
// try to match a token of type "type". If fails, the state of the lexer is unchanged. Otherwise, the lexer is incremented and the desired token is returned
#define assert_next(lexer, name, ty) \
    Token name = lexer_next(lexer);  \
    if (name.type != ty)             \
        THROW_EXCEPT();
#define assert_next_before(lexer, name, ty, before)                                                                    \
    Token name = lexer_next(lexer);                                                                                    \
    if (name.type != ty)                                                                                               \
    {                                                                                                                  \
        report(lexer, before.off, "Expect %s after %s, found %s", tk_str(ty), tk_str(before.type), tk_str(name.type)); \
        THROW_EXCEPT();                                                                                                \
    }
#define try_next(lexer, name, ty)   \
    Token name = lexer_peek(lexer); \
    do                              \
    {                               \
        if (name.type != ty)        \
            return PR_NULL;         \
        else                        \
            lexer_next(lexer);      \
    } while (0)

make_arr(Sec);
make_arr(Formal);
make_arr(Expr);
make_arr(AstIdx);
struct Gen
{
    ArrOf(AstIdx) toplevel;
    SecArr secs;
    FormalArr formals;
    ExprArr exprs;
    LabelArr labels;
    bool inside_sec;
};

// TOOD assumes never fails
LabelIdx parse_label(Lexer *lexer, Gen *gen, size_t note_ct)
{
    try_next(lexer, ldiamon, '@');
    Label label = {.note_pos = note_ct};

    assert_next_before(lexer, name, TK_IDENT, ldiamon);
    label.name = name.data.integer;
    assert_next_before(lexer, lbrac, '[', ldiamon);
    assert_next_before(lexer, volume, TK_IDENT, lbrac);
    assert(volume.data.integer == symt_intern(lexer->sym_table, "volume"));
    assert_next_before(lexer, eq, '=', volume);
    assert_next_before(lexer, num, TK_INT, eq);
    label.volume = num.data.integer;
    Token linear = lexer_peek(lexer);
    if (linear.type == TK_IDENT)
    {
        lexer_next(lexer);
        assert(linear.data.integer == symt_intern(lexer->sym_table, "linear"));
        label.linear = true;
    }
    assert_next_before(lexer, rbrac, ']', lbrac);
    da_append(gen->labels, label);
    return gen->labels.size - 1;
}
// parse a list of formal, seperated by comma i.e. <id1>=<exp1>, <id2>=<exp2>, ...
SliceOf(AstIdx) parse_formals(Lexer *lexer, Gen *gen)
{
    ArrOf(AstIdx) formals = {0};
    SliceOf(AstIdx) res = {0};
    // parse comma seperated list of formals
    FormalIdx formal = parse_formal(lexer, gen);
    if (formal == PR_NULL)
    {
        return res;
    }
    da_append(formals, formal);
    while (true)
    {
        Token comma = lexer_peek(lexer);
        if (comma.type != ',')
        {
            break;
        }
        lexer_next(lexer);
        if ((formal = parse_formal(lexer, gen)) == PR_NULL)
        {
            report(lexer, comma.off, "Expect formal after ','");
            THROW_EXCEPT();
        }
        da_append(formals, formal);
    }

    da_move(formals, res);
    return res;
}
ExprIdx parse_section_expr(Lexer *lexer, Gen *gen)
{
    SecIdx sec = parse_section(lexer, gen);
    if (sec == PR_NULL)
        return PR_NULL;
    Expr expr = {.off = gen->secs.items[sec].off, .tag = EXPR_SEC, .data.sec = sec};
    da_append(gen->exprs, expr);
    return gen->exprs.size - 1;
}
SecIdx parse_section(Lexer *lexer, Gen *gen)
{

    // TODO parse var decl skipped
    if (gen->inside_sec)
        return PR_NULL;
    gen->inside_sec = true;
    try_next(lexer, bar, '|');
    Sec sec = {0};
    sec.vars = parse_formals(lexer, gen);
    assert_next_before(lexer, colon1, ':', bar);
    // parse comma seperated list of formals
    sec.config = parse_formals(lexer, gen);
    assert_next_before(lexer, colon2, ':', colon1); // TODO more accurate loc
    ArrOf(AstIdx) notes = {0};
    ArrOf(AstIdx) labels = {0};
    ExprIdx note;
    sec.off = colon2.off;
    while ((note = parse_expr(lexer, gen)) >= PR_NULL)
    {
        if (note == PR_NULL)
        {
            // TODO assume never fail for now
            LabelIdx label = parse_label(lexer, gen, notes.size);
            if (label == PR_NULL)
            {
                break;
            }

            da_append(labels, label);
        }
        else
        {
            da_append(notes, note);
            sec.off = gen->exprs.items[note].off;
        }
    }

    assert_next_before(lexer, bar2, '|', colon2); // TODO: more accurate loc
    gen->inside_sec = false;
// out:
    da_move(notes, sec.note_exprs);
    da_move(labels, sec.labels);
    da_append(gen->secs, sec);
    return gen->secs.size - 1;
// err_out:
//     da_free(notes);
//     return res;
}
FormalIdx parse_formal(Lexer *lexer, Gen *gen)
{
    try_next(lexer, ident, TK_IDENT);

    assert_next_before(lexer, assign, '=', ident);
    ExprIdx expr = parse_expr(lexer, gen);
    if (expr == PR_NULL)
    {
        report(lexer, assign.off, "Expect expression after '='");
        THROW_EXCEPT();
    }

    Formal formal = {.expr = expr, .ident = ident.data.integer, .off = gen->exprs.items[expr].off};
    da_append(gen->formals, formal);
    return gen->formals.size - 1;
}

ExprIdx parse_list(Lexer *lexer, Gen *gen)
{
    try_next(lexer, lbrac, '[');
    Expr expr;
    ExprIdx sub_expr;
    ArrOf(AstIdx) sub_exprs = {0};
    while ((sub_expr = parse_expr(lexer, gen)) > PR_NULL)
    {
        da_append(sub_exprs, sub_expr);
    }
    assert_next_before(lexer, rbrac, ']', lbrac) // TODO: more accurate loc
        expr.tag = EXPR_LIST;
    expr.off = rbrac.off;
    da_move(sub_exprs, expr.data.chord_notes);
    da_append(gen->exprs, expr);
    return gen->exprs.size - 1;
}
typedef ExprIdx (*ParseFn)(Lexer *, Gen *);
ExprIdx parse_scale(Lexer *lexer, Gen *gen)
{
    try_next(lexer, lslash, '/');
    ExprIdx tonic = parse_expr(lexer, gen);
    if (tonic == PR_NULL)
    {
        report(lexer, lslash.off, "Expect expression for tonic in scale expression");
        THROW_EXCEPT();
    }
    ExprIdx octave = parse_expr(lexer, gen);
    if (octave == PR_NULL)
    {
        report(lexer, gen->exprs.items[tonic].off, "Expect expression for octave in scale expression");
        THROW_EXCEPT();
    }
    ExprIdx mode = parse_expr(lexer, gen);
    if (mode == PR_NULL)
    {
        report(lexer, gen->exprs.items[octave].off, "Expect expression for mode in scale expression");
        THROW_EXCEPT();
    }
    try_next(lexer, rslash, '/');
    if (rslash.type == PR_NULL)
    {
        report(lexer, gen->exprs.items[mode].off, "Unclosed scale expression");
        THROW_EXCEPT();
    }
    Expr expr = {.off = lslash.off, .tag = EXPR_SCALE};
    expr.data.scale.tonic = tonic;
    expr.data.scale.octave = octave;
    expr.data.scale.mode = mode;
    da_append(gen->exprs, expr);
    return gen->exprs.size - 1;
}
// currently only have one kind of prefix operator
// we desugar it to the infix opeator e.g. #'1 => 1'1
ExprIdx parse_if(Lexer *lexer, Gen *gen)
{
    try_next(lexer, if_tk, TK_IF);
    ExprIdx cond_expr = parse_expr(lexer, gen);
    if (cond_expr == PR_NULL)
    {
        report(lexer, if_tk.off, "Expect conditional expression after `if`");
        THROW_EXCEPT();
    }
    assert_next_before(lexer, then, TK_THEN, if_tk);
    ExprIdx then_expr = parse_expr(lexer, gen);
    if (then_expr == PR_NULL)
    {
        report(lexer, then.off, "Expect expression after `then`");
        THROW_EXCEPT();
    }
    assert_next_before(lexer, else_tk, TK_ELSE, then);
    ExprIdx else_expr = parse_expr(lexer, gen);
    if (else_expr == PR_NULL)
    {
        report(lexer, then.off, "Expect expression after `else`");
        THROW_EXCEPT();
    }
    Expr expr = {.off = if_tk.off, .tag = EXPR_IF, .data.if_then_else = {.cond_expr = cond_expr, .then_expr = then_expr, .else_expr = else_expr}};
    da_append(gen->exprs, expr);
    return gen->exprs.size - 1;
}
ExprIdx parse_for(Lexer *lexer, Gen *gen)
{
    try_next(lexer, for_tk, TK_FOR);
    ExprIdx lower_expr = parse_expr(lexer, gen);
    if (lower_expr == PR_NULL)
    {
        report(lexer, for_tk.off, "Expect lower bound expression after `for`");
        THROW_EXCEPT();
    }
    // parse bound
    assert_next_before(lexer, to_tk, '~', for_tk);
    Token bound = lexer_peek(lexer);
    bool is_leq;
    if (bound.type == '<')
    {
        is_leq = false;
    }
    else if (bound.type == TK_LEQ)
    {
        is_leq = true;
    }
    else
    {
        report(lexer, to_tk.off, "Expect either `<` or `<=` after `~`");
        THROW_EXCEPT();
    }
    lexer_next(lexer);
    ExprIdx upper_bound = parse_expr(lexer, gen);
    if (upper_bound == PR_NULL)
    {
        report(lexer, bound.off, "Expect upper bound expression after bound");
        THROW_EXCEPT();
    }

    assert_next_before(lexer, loop, TK_LOOP, bound);
    // parse body
    ArrOf(AstIdx) body_arr = {0};
    ExprIdx body;
    while ((body = parse_expr(lexer, gen)) != PR_NULL)
    {
        da_append(body_arr, body);
    }
    assert_next_before(lexer, end, TK_END, loop);
    SliceOf(AstIdx) body_slice = {0};
    da_move(body_arr, body_slice);

    Expr expr = {.off = for_tk.off, .tag = EXPR_FOR, .data.for_expr = {.is_leq = is_leq, .lower_bound = lower_expr, .upper_bound = upper_bound, .body = body_slice}};
    da_append(gen->exprs, expr);
    return gen->exprs.size - 1;
}
ExprIdx parse_prefix(Lexer *lexer, Gen *gen)
{
    Token prefix = lexer_peek(lexer);
    int lbp = prefix_bp(prefix);
    if (lbp < 0) return PR_NULL;
    lexer_next(lexer);
    ExprIdx sub_expr = parse_expr_climb(lexer, gen, lbp);
    if (sub_expr == PR_NULL)
    {
        report(lexer, prefix.off, "Expect expression after prefix operator");
        THROW_EXCEPT();
    }
    Expr expr;
    if (prefix.type == TK_QUAL) {
	Token qual = prefix;
	ssize_t shift =
	    -12 * qual.data.qualifier.suboctave + 12 * qual.data.qualifier.octave - qual.data.qualifier.flats + qual.data.qualifier.sharps;
	Expr lhs = {.off = qual.off, .tag = EXPR_NUM, .data.num = shift};
	da_append(gen->exprs, lhs);
	ExprIdx lhs_idx = gen->exprs.size - 1;
	expr = (Expr) {.off = qual.off, .tag = EXPR_INFIX, .data.infix = {.op = '\'', .lhs = lhs_idx, .rhs = sub_expr}};
    } else {

	expr = (Expr){.off = prefix.off, .tag = EXPR_PREFIX, .data.prefix = {.expr = sub_expr, .op = prefix}};
    }
    da_append(gen->exprs, expr);
    return gen->exprs.size - 1;

}
int prefix_bp(Token tk)
{
    switch (tk.type)
    {
    case TK_QUAL:
        return 20;
    case '$':
	return 10;
    default:
        return -1;
    }
}
int postfix_bp(Token tk)
{
    switch (tk.type)
    {
    case TK_DOTS:
        return 10;
    default:
        return -1;
    }
}

BP infix_bp(Token tk)
{
    switch (tk.type)
    {
    case '&':
        return (BP){.lbp = 5, .rbp = 5};
    case '\'':
        return (BP){.lbp = 19, .rbp = 20};
    case '+':
        return (BP){.lbp = 5, .rbp = 4};
    case '-':
        return (BP){.lbp = 5, .rbp = 4};
    case '*':
        return (BP){.lbp = 7, .rbp = 6};
    case TK_EQ:
        return (BP){.lbp = 3, .rbp = 3};
    case TK_NEQ:
        return (BP){.lbp = 3, .rbp = 3};
    case TK_GEQ:
        return (BP){.lbp = 3, .rbp = 3};
    case TK_LEQ:
        return (BP){.lbp = 3, .rbp = 3};
    case '>':
        return (BP){.lbp = 3, .rbp = 3};
    case '<':
        return (BP){.lbp = 3, .rbp = 3};

    default:
        return (BP){.lbp = -1, .rbp = -1};
    }
}
ExprIdx parse_expr_climb(Lexer *lexer, Gen *gen, int min_bp)
{
    // the order of this matter; atomic expr must be tried last.
    static const ParseFn prefix_parsers[] = {parse_prefix, parse_if, parse_for, parse_scale, parse_list, parse_section_expr, parse_atomic_expr};
    static const size_t prefix_parsers_len = sizeof(prefix_parsers) / sizeof(prefix_parsers[0]);
    ExprIdx lhs;
    for (size_t i = 0; i < prefix_parsers_len; ++i)
    {
        lhs = prefix_parsers[i](lexer, gen);
        if (lhs > PR_NULL)
            break;
    }
    if (lhs == PR_NULL)
        return PR_NULL;
    // postfix parser
    Token op;
    while ((op = lexer_peek(lexer)).type > TK_NULL)
    {
        int lbp = postfix_bp(op);
        if (lbp > 0)
        {
            if (lbp < min_bp)
                break;
            lexer_next(lexer);
            Expr expr;
            expr.tag = EXPR_NOTE;
            expr.off = op.off;
            expr.data.note.dots = op.data.integer;
            expr.data.note.expr = lhs;
            da_append(gen->exprs, expr);
            lhs = gen->exprs.size - 1;
        }
        else
        {
            BP bp = infix_bp(op);
            lbp = bp.lbp;
            int rbp = bp.rbp;
            if (lbp < min_bp)
                break;
            lexer_next(lexer);
            ExprIdx rhs = parse_expr_climb(lexer, gen, rbp);
            Expr expr = {.tag = EXPR_INFIX, .off = op.off};
	    expr.data.infix.lhs = lhs;
            expr.data.infix.rhs = rhs;
            expr.data.infix.op = op;
            da_append(gen->exprs, expr);
            lhs = gen->exprs.size - 1;
        }
    }
    return lhs;
}
ExprIdx parse_expr(Lexer *lexer, Gen *gen)
{
    return parse_expr_climb(lexer, gen, 0);
}
ExprIdx parse_atomic_expr(Lexer *lexer, Gen *gen)
{
    Token tk = lexer_peek(lexer);
    Expr expr = {0};
    // used in '['
    switch ((int)tk.type)
    {
    case TK_IDENT:
        expr.tag = EXPR_IDENT;
        expr.data.ident = tk.data.integer;
        lexer_next(lexer);
        expr.off = tk.off;
        break;
    case TK_INT:
        expr.tag = EXPR_NUM;
        expr.data.num = tk.data.integer;
        lexer_next(lexer);
        expr.off = tk.off;
        break;
    case TK_FALSE:
        expr.tag = EXPR_BOOL;
        expr.data.num = 0;
        lexer_next(lexer);
        expr.off = tk.off;
        break;

    case TK_TRUE:
        expr.tag = EXPR_BOOL;
        expr.data.num = 1;
        lexer_next(lexer);
        expr.off = tk.off;
        break;
    case TK_VOID:
        expr.tag = EXPR_VOID;
        lexer_next(lexer);
        expr.off = tk.off;
        break;
    case '(':
        lexer_next(lexer);
        ExprIdx tmp = parse_expr(lexer, gen);
        if ((int)lexer_next(lexer).type != ')')
            THROW_EXCEPT();
        return tmp;
    default:
        return PR_NULL;
    }

    da_append(gen->exprs, expr);
    return gen->exprs.size - 1;
}

Pgm parse_ast(Lexer *lexer)
{
    Pgm pgm;
    FormalIdx toplevel;
    Gen gen = {0};
    //  da_append(gen.decls, (Decl){});
    //  da_append(gen.secs, (Sec){});
    //  da_append(gen.formals, (Formal){});
    //  da_append(gen.exprs, (Expr){});
    //  da_append(gen.labels, (Label){});
    while ((toplevel = parse_formal(lexer, &gen)) != PR_NULL)
    {
        da_append(gen.toplevel, toplevel);
    }

    da_move(gen.toplevel, pgm.toplevel);
    da_move(gen.secs, pgm.secs);
    da_move(gen.formals, pgm.formals);
    da_move(gen.exprs, pgm.exprs);
    da_move(gen.labels, pgm.labels);

    return pgm.success = true, pgm;
// err_out:
//     da_free(gen.toplevel);
//     da_free(gen.secs);
//     da_free(gen.formals);
//     da_free(gen.exprs);
//     da_free(gen.labels);
//     // free gen
//     pgm.success = false;
//     return pgm;
}
void ast_get_expr(Pgm *pgm, AstIdx i);
void ast_get_decl(Pgm *pgm, AstIdx i);
void ast_get_sec(Pgm *pgm, AstIdx i);
void ast_get_form(Pgm *pgm, AstIdx i);
void ast_deinit(Pgm *pgm)
{
    free(pgm->secs.ptr);
    free(pgm->toplevel.ptr);
    free(pgm->formals.ptr);
    free(pgm->exprs.ptr);
}
