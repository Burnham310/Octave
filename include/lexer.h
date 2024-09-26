#ifndef LEXER_H_
#define LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>


typedef struct {
    ssize_t pitch;
    size_t dots;
} Note; // should prob move this some common.h
typedef struct
{
	size_t row;
	size_t col;
} Loc;
typedef enum
{
	TK_ERR = -2,
	TK_EOF = -1,
	TK_NULL = 0,
	TK_IDENT,
	TK_INT,
	TK_DOTS,

} TokenType;
typedef struct
{
	const char *ptr;
	size_t len;
} Slice;
typedef union
{
	Note note;
	ssize_t integer;
	Slice str;
} TokenData;
typedef struct
{
	TokenType type;
	TokenData data;
	size_t off;
} Token;
Loc off_to_loc(const char *src, const size_t src_len, const size_t off);
const char* ty_str(TokenType ty);
#define TOKEN_DEBUG(tk)                                               \
	do                                                                \
{                                                                 \
	switch (tk.type)                                              \
	{                                                             \
		case TK_ERR:                                                  \
									      printf("ERR");                                            \
		break;                                                    \
		case TK_EOF:                                                  \
									      printf("EOF");                                            \
		break;                                                    \
		case TK_NULL:                                                 \
									      printf("NULL");                                           \
		case TK_IDENT:                                                \
									      printf("ID %.*s", (int)tk.data.str.len, tk.data.str.ptr); \
		break;                                                    \
		case TK_INT:                                                  \
									      printf("INT_CONST %zi", tk.data.integer);                 \
		break;                                                    \
		case TK_DOTS: \
									      printf("TK_DOTS %zi", tk.data.integer); \
		break;                                                    \
		default:                                                      \
									      printf("%c", tk.type);                                    \
	}                                                             \
} while (0);
typedef struct
{
	const char *src;
	size_t src_len;
	size_t off;
	const char *path;
	Token peakbuf;
} Lexer;
typedef struct
{
	const char* src;
	size_t src_len;
	const char *path;
} LexerDummy;

LexerDummy lexer_dummy_init(const char* src, const size_t src_len, const char *path);
Lexer lexer_init(const char *src, size_t src_len, const char *path);
Token lexer_next(Lexer *self);
Token lexer_peek(Lexer *self);
#endif
