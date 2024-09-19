#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "lexer.h"
#define try(exp)    \
    if ((exp) <= 0) \
    return exp
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

typedef struct
{
    AstIdx *items;
    size_t size;
    size_t cap;
} AstArr;

typedef struct
{
    Decl *items;
    size_t size;
    size_t cap;
} DeclArr;

typedef struct
{
    Sec *items;
    size_t size;
    size_t cap;
} SecArr;
typedef struct
{
    Note *items;
    size_t size;
    size_t cap;
} NoteArr;
struct Gen
{
    DeclArr decls;
    SecArr secs;
};

DeclIdx parse_decl(Lexer *lexer, Gen *gen)
{
    Token id = try_next(lexer, TK_IDENT);
    try(id.type);

    Token assign = assert_next(lexer, '='); // assert_next never returns TK_NULL
    try_before(assign, '=', id);

    SecIdx sec = parse_section(lexer, gen);
    if (sec <= 0)
    {
        report(lexer, assign.off, "Expect section after '='");
        return PR_ERR;
    };
    da_append(gen->decls, ((Decl){.name = id.data.str, .sec = sec}));
    return gen->decls.size - 1;
}
SecIdx parse_section(Lexer *lexer, Gen *gen)
{
    // TODO parse var decl skipped
    Token colon1 = try_next(lexer, ':');
    try(colon1.type);
    // TODO parse config skipped
    Token colon2 = assert_next(lexer, ':');
    try_before(colon2, ':', colon1);

    NoteArr notes = {0};
    Token note;
    AstIdx res;
    while ((note = try_next(lexer, TK_NOTE)).type > 0)
    {
        da_append(notes, note.data.note);
    }
    if (note.type < 0)
    {
        report(lexer, lexer->off, "Error while parsing notes");
        res = note.type;
        goto out;
    }
    Token colon3 = assert_next(lexer, ';');
    if (colon3.type < 0)
    {
        report(lexer, lexer->off, "Expect ';' at the end of section");
        res = colon3.type;
        goto out;
    }
    Sec sec = {0};
out:
    da_move(notes, sec.notes);
    da_append(gen->secs, sec);
    return gen->secs.size - 1;
err_out:
    da_free(notes);
    return res;
}

Pgm parse_ast(Lexer *lexer)
{
    Pgm pgm;
    DeclIdx decl;
    Gen gen = {0};
    da_append(gen.decls, (Decl){});
    da_append(gen.secs, (Sec){});
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

    return pgm.success = true, pgm;
err_out:
    da_free(gen.decls);
    da_free(gen.secs);
    // free gen
    pgm.success = false;
    return pgm;
}
void ast_deinit(Pgm *pgm)
{
    free(pgm->secs.ptr);
    free(pgm->decls.ptr);
}
