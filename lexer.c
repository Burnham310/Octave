
#include "lexer.h"
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#define eprintf(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define report(lexer, off, fmt, ...) do {Loc __loc = off_to_loc(lexer->src, lexer->src_len, off); eprintf("%s:%zu:%zu "fmt"\n", lexer->path, __loc.row, __loc.col, __VA_ARGS__); } while (0)
Lexer lexer_init(const char* src, size_t src_len, const char* path) {
    Lexer lexer = {.src = src, .src_len = src_len, .off = 0, .path = path, .peekbuf = TK_NULL};

    return lexer;
}
Loc off_to_loc(const char* src, size_t src_len, size_t off) {
    Loc loc = {.row = 1, .col = 1};
    assert(off < src_len);
    for (size_t i = 0; i < off; ++i) {
	char c = src[i];
	if (c == '\n') {
	    loc.row++;
	} else {
	    loc.col++;
	}
    }
    return loc;
    
}
void rewind_char(Lexer* self) {
    self->off--;
}
void skip_ws(Lexer *lexer) {
    while (lexer->off < lexer->src_len) {
	char c = lexer->src[lexer->off];
	switch (c) {
	    case '\n':
	    case ' ':
	    case '\t':
		break;
	    default:
		return;
	}
	lexer->off++;
    }
}
// return 0 if no more character left
char next_char(Lexer* self) {
    if (self->off >= self->src_len) return 0;
    return self->src[self->off++];
}

#define RETURN_TK(x) do {tk.type = x; return tk; } while (0)
Token match_single(Lexer *self) {

    char c;
    Token tk = {.off = self->off};
    switch ((c = next_char(self))) {
	case 0:
	    RETURN_TK(TK_NULL);
	case '=':
	case '[':
	case ']':
	case '.':
	case ',':
	case ':':
	case ';':
	    tk.type = c;
	    return tk;
	default:
	    rewind_char(self);
	    RETURN_TK(TK_NULL);
	
    }
    return tk;

}
Token match_multiple(Lexer *self) {
   assert(0 && "unimplemented"); 
}	
Token match_num(Lexer *self) {
    Token tk = {.off = self->off};
    char c = next_char(self);
    if (!isdigit(c)) {
	rewind_char(self);
	RETURN_TK(TK_NULL);
    }
    while ((c = next_char(self)) != 0) {
	if (c == ' ' || c == '\t' || c == '\n') {
	    rewind_char(self);
	    break;
	} else if (isdigit(c)) {
	    continue;
	} else {
	    // TODO report error
	    report(self, self->off, "invalid character '%c' (%d) in interger", c, c);
	    RETURN_TK(TK_ERR);
	}
    }
    tk.type = TK_INT;

    tk.data.integer = strtol(self->src + tk.off, NULL, 10);
    return tk;

}
Token match_iden(Lexer *self) {
    Token tk = {.off = self->off};
    char c = next_char(self);
    if (!isalpha(c) && c != '_') {
	rewind_char(self);
	RETURN_TK(TK_NULL);
    }

    while ((c = next_char(self)) != 0) {
	if (c == ' ' || c == '\t' || c == '\n') {
	    rewind_char(self);
	    break;
	} else if (isalpha(c) || c == '_') {
	    continue;
	} else {
	    // TODO report error
	    report(self, self->off, "invalid character '%c' (%d) in interger", c, c);
	    RETURN_TK(TK_ERR);
	}
    }

    tk.type = TK_IDEN;
    tk.data.str = (Slice) {.ptr = self->src + tk.off, .len = self->off - tk.off};
    return tk;

}
void lexer_deinit(Lexer *lexer) {
   assert(0 && "unimplemented"); 


}

Token lexer_next(Lexer *self) {
    skip_ws(self);
    Token tk;
    return
	(tk = match_single(self)).type 	> 0 ? tk :
	(tk = match_num(self)).type 	> 0 ? tk : 
	(tk = match_iden(self)).type 	> 0 ? tk : tk;
}

Token lexer_peek(Lexer *self) {
   assert(0 && "unimplemented"); 
}

