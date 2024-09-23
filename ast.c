#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "lexer.h"
#include "utils.h"
DeclIdx parse_decl(Lexer* lexer, Gen* gen);
SecIdx parse_section(Lexer* lexer, Gen* gen);
FormalIdx parse_formal(Lexer* lexer, Gen* gen);
ExprIdx parse_expr(Lexer* lexer, Gen* gen);
#define try(exp)    \
    if ((exp).type <= 0) \
    return (exp).type
#define try_before(tk, expect, before)                                                                                       \
    do                                                                                                                       \
    {                                                                                                                        \
        if ((tk).type <= 0)                                                                                                  \
        {                                                                                                                    \
            report(lexer, before.off, "Expect %s after %s, found %s", ty_str(expect), ty_str(before.type), ty_str(tk.type)); \
            return (tk).type;                                                                                                \
        }                                                                                                                    \
    } while (0)
// try to match a token of type "type". If fails, the state of the lexer is unchanged. Otherwise, the lexer is incremented and the desired token is returned
Token try_next(Lexer *lexer, TokenType type)
{
    Token tk = lexer_peek(lexer);
    if (tk.type == type)
    {
        lexer_next(lexer);
        return tk;
    }
    tk.type = TK_NULL;
    return tk;
}

// try to match a token of type "type". If the next token does not match, an error is returned.
Token assert_next(Lexer *lexer, TokenType type)
{
    Token tk = lexer_next(lexer);
    if (tk.type == type)
    {
        return tk;
    }
    tk.type = TK_ERR;
    return tk;
}
make_arr(Decl);
make_arr(Sec);
make_arr(Note);
make_arr(Formal);
make_arr(Expr);
make_arr(AstIdx);
struct Gen
{
    DeclArr decls;
    SecArr secs;
    FormalArr formals;
    ExprArr exprs;
};

DeclIdx parse_decl(Lexer *lexer, Gen *gen)
{
    Token id = try_next(lexer, TK_IDENT);
    try(id);

    Token assign = assert_next(lexer, '='); // assert_next never returns TK_NULL
    try_before(assign, '=', id);

    SecIdx sec = parse_section(lexer, gen);
    if (sec <= 0)
    {
        report(lexer, assign.off, "Expect section after '='");
        return PR_ERR;
    };
    Token colon = assert_next(lexer, ';');
    if (colon.type < 0)
    {
        report(lexer, gen->secs.items[sec].off, "Expect ';' at the end of a declaration");
	// TODO free sec
	return PR_ERR;

    }
    da_append(gen->decls, ((Decl){.name = id.data.str, .sec = sec, .off = colon.off}));
    return gen->decls.size - 1;
}
SecIdx parse_section(Lexer *lexer, Gen *gen)
{
    // TODO parse var decl skipped
    Token colon1 = try_next(lexer, ':');
    Sec sec = {0};
    try(colon1);
    // TODO parse config skipped
    ArrOf(AstIdx) formals = {0};

    FormalIdx formal = parse_formal(lexer, gen);
    if (formal < 0) {
	return PR_ERR;
    } else if (formal > 0) {
	da_append(formals, formal);
	while (true) {
	    Token comma = try_next(lexer, ',');
	    if (comma.type < 0) {
		return PR_ERR;
	    } else if (comma.type == 0) {
		break;
	    }
	    if ((formal = parse_formal(lexer, gen)) <= 0) {
		report(lexer, comma.off, "Expect formal after ','");
		return PR_ERR;
	    }
	    da_append(formals, formal);	
	}
	if (formal < 0) {
	    // TODO free formals
	    return PR_ERR;
	}
	da_move(formals, sec.config);
    }

    Token colon2 = assert_next(lexer, ':');
    try_before(colon2, ':', colon1);

    NoteArr notes = {0};
    Token note;
    AstIdx res;
    sec.off = colon2.off;
    while ((note = try_next(lexer, TK_NOTE)).type > 0)
    {
        da_append(notes, note.data.note);
	sec.off = note.off;
    }
    if (note.type < 0)
    {
        report(lexer, sec.off, "Error while parsing notes");
        res = note.type;
        goto out;
    }
    
out:
    da_move(notes, sec.notes);
    da_append(gen->secs, sec);
    return gen->secs.size - 1;
err_out:
    da_free(notes);
    return res;
}
FormalIdx parse_formal(Lexer* lexer, Gen* gen) {
    Token ident = try_next(lexer, TK_IDENT);
    try(ident);
    Token assign = assert_next(lexer, '=');
    try_before(assign, '=', ident);
    ExprIdx expr = parse_expr(lexer, gen);
    if (expr <= 0) {
	report(lexer, assign.off, "Expect expression after '='");
	return PR_ERR;
    }
    

    Formal formal = {.expr = expr, .ident = ident.data.str, .off = gen->exprs.items[expr].off};
    da_append(gen->formals, formal);
    return gen->formals.size - 1;
}

ExprIdx parse_expr(Lexer* lexer, Gen* gen) {
    Token tk = lexer_peek(lexer);
    Expr expr = {0};
    switch (tk.type) {
	case TK_IDENT:
	    expr.tag = EXPR_IDENT;
	    expr.data.ident = tk.data.str;
	    break;
	case TK_INT:
	    expr.tag = EXPR_NUM;
	    expr.data.num = tk.data.integer;
	    break;
	default:
	    return PR_NULL;

    }
    lexer_next(lexer);
    expr.off = tk.off;
    da_append(gen->exprs, expr);
    return gen->exprs.size - 1;
}

Pgm parse_ast(Lexer *lexer)
{
    Pgm pgm;
    DeclIdx decl;
    Gen gen = {0};
    da_append(gen.decls, (Decl){});
    da_append(gen.secs, (Sec){});
    da_append(gen.formals, (Formal){});
    da_append(gen.exprs, (Expr){});
    bool find_main = false;
    while ((decl = parse_decl(lexer, &gen)) > 0)
    {
        Slice name = gen.decls.items[decl].name;
        if (strncmp(name.ptr, "main", name.len) == 0)
        {
            find_main = true;
            pgm.main = decl;
        }
    }
    if (decl < 0)
    {
        report(lexer, lexer->off, "Expect Decl at toplevel");
        goto err_out;
    }
    if (!find_main)
    {
        eprintf("\"main\" is undefined\n");
        goto err_out;
    }

    da_move(gen.decls, pgm.decls);
    da_move(gen.secs, pgm.secs);
    da_move(gen.formals, pgm.formals);
    da_move(gen.exprs, pgm.exprs);

    return pgm.success = true, pgm;
err_out:
    da_free(gen.decls);
    da_free(gen.secs);
    da_free(gen.formals);
    da_free(gen.exprs);
    // free gen
    pgm.success = false;
    return pgm;
}
void ast_deinit(Pgm *pgm)
{
    free(pgm->secs.ptr);
    free(pgm->decls.ptr);
}
