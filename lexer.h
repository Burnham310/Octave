#ifndef LEXER_H_
#define LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
typedef struct {
    size_t	row;
    size_t 	col;
} Loc;
typedef enum {
    TK_ERR = -2,
    TK_EOF = -1,
    TK_NULL = 0,

    TK_IDEN,
    TK_INT,

    TK_EQ,
    TK_COLON,
    TK_LBRAC,
    TK_RRBRC,
    TK_DOT,
    TK_COMMA,

} TokenType;
typedef union {
    size_t 	integer;
    char* 	str;
} TokenData;
typedef struct {
    TokenType 	type;
    TokenData 	data;
    size_t	off;
} Token;

typedef struct {
    const char* src;
    size_t 	src_len;
    size_t	off;
    const char*	path;
    Token 	peekbuf;
} Lexer;
Lexer lexer_init(const char* src, size_t src_len, const char* path);
void skip_ws(Lexer *lexer); 
// return 0 if no more character left
char next_char(Lexer* self); 
Token match_single(Lexer *self);
Token match_multiple(Lexer *self);
Token match_num(Lexer *self);
Token match_iden(Lexer *self);

void lexer_deinit(Lexer *lexer);
#endif
