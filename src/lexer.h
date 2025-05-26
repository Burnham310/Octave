#ifndef LEXER_H_
#define LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include "ds.h"
#include "stb_ds.h"


typedef struct
{
	size_t row;
	size_t col;
} Loc;
typedef enum
{
	TK_EOF = -1,
	TK_NULL = 0,
	TK_IDENT,
	TK_INT,
	TK_DOTS,
	TK_QUAL,
	TK_TRUE,
	TK_FALSE,
	TK_EQ,
	TK_NEQ,
	TK_GEQ,
	TK_LEQ,
	TK_IF,
	TK_THEN,
	TK_ELSE,
	TK_FOR,
	TK_LOOP,
	TK_END,
	TK_VOID,
} TokenType;
typedef struct
{
    const char *key;
    TokenType value;
} SymbolEntry;
typedef SymbolEntry *SymbolTable;
typedef size_t Symbol;
#define symt_put_keyword_tk(sym_table, s, tk) (shput(sym_table, s, tk))
#define symt_get_keyword_tk(sym_table, s) (sym_table[s].tk)
#define symt_intern(sym_table, s) (shput(sym_table, s, TK_IDENT), shgeti(sym_table, s))
#define symt_lookup(sym_table, s) sym_table[s].key
typedef unsigned char u8;
typedef union
{
	struct
	{
		u8 sharps;
		u8 flats;
		u8 octave;
		u8 suboctave;
	} qualifier;
	ssize_t integer;
} TokenData;
typedef struct
{
	TokenType type;
	TokenData data;
	size_t off;
} Token;
Loc off_to_loc(const char *src, const size_t src_len, const size_t off);
const char *tk_str(TokenType ty);
typedef struct
{
	char *src;
	size_t src_len;
	size_t off;
	const char *path;
	Token peakbuf;
	SymbolTable sym_table;

} Lexer;
typedef struct
{
	const char *src;
	size_t src_len;
	const char *path;
} LexerDummy;

LexerDummy lexer_dummy_init(const char *src, const size_t src_len, const char *path);
Lexer lexer_init(char *src, size_t src_len, const char *path);
void lexer_deinit(Lexer *self);
Token lexer_next(Lexer *self);
Token lexer_peek(Lexer *self);
#endif
