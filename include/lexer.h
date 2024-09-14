#ifndef LEXER_H_
#define LEXER_H_

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>


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


} TokenType;
typedef struct {
    const char* ptr;
    int len;
} Slice;
typedef union {
    ssize_t 	integer;
    Slice 	str;
} TokenData;
typedef struct {
    TokenType 	type;
    TokenData 	data;
    size_t	off;
} Token;
#define TOKEN_DEBUG(tk) do { \
    switch (tk.type) { \
	case TK_ERR: \
	    printf("ERR"); \
	    break; \
	case TK_EOF: \
	    printf("EOF"); \
	    break;	\
	case TK_NULL: \
	    printf("NULL"); \
	case TK_IDEN:	\
	    printf("ID %.*s", tk.data.str.len, tk.data.str.ptr);	\
	    break;	\
	case TK_INT:	\
	    printf("INT_CONST %zi", tk.data.integer); \
	    break; \
	default: \
	    printf("%c", tk.type); \
    } \
} while(0);
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
Token lexer_next(Lexer *self);
Token lexer_peek(Lexer *self);

void lexer_deinit(Lexer *lexer);
#endif
