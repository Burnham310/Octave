#ifndef LEXER_H_
#define LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>

#define eprintf(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define report(lexer, off, fmt, ...)                                                     \
	do                                                                                   \
	{                                                                                    \
		Loc __loc = off_to_loc(lexer->src, lexer->src_len, off);                         \
		eprintf("%s:%zu:%zu " fmt "\n", lexer->path, __loc.row, __loc.col, ##__VA_ARGS__); \
	} while (0)


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
	TK_NOTE,

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
		case TK_NOTE: \
									      printf("NOTE_CONST %zi.%zu", tk.data.note.pitch, tk.data.note.dots); \
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
Lexer lexer_init(const char *src, size_t src_len, const char *path);
void skip_ws(Lexer *lexer);
// return 0 if no more character left

char next_char(Lexer *self);
Token match_single(Lexer *self);
Token match_multiple(Lexer *self);
Token match_num(Lexer *self);
Token match_ident(Lexer *self);
Token lexer_next(Lexer *self);
Token lexer_peek(Lexer *self);

void lexer_uninit(Lexer *lexer);
#endif
