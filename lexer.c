
#include "lexer.h"
#include <assert.h>
Lexer lexer_init(const char* src, size_t src_len, const char* path) {
    Lexer lexer = {.src = src, .src_len = src_len, .off = 0, .path = path, .peekbuf = TK_NULL};

    return lexer;
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
    self->off++;
    return self->src[self->off - 1];
}
#define RETURN_TK(x) do {tk.type = x; return tk; } while (0)
Token match_single(Lexer *self) {

    char c;
    Token tk = {.off = self->off};
    switch ((c = next_char(self))) {
	case 0:
	    RETURN_TK(TK_NULL);
	case '=':
	    RETURN_TK(TK_EQ);
	case '[':
	    RETURN_TK(TK_LBRAC);
	case ']':
	    RETURN_TK(TK_RRBRC);
	case '.':
	    RETURN_TK(TK_DOT);
	case ',':
	    RETURN_TK(TK_COMMA);
	default:
	    RETURN_TK(TK_NULL);
	
    }
    return tk;

}
Token match_multiple(Lexer *self) {
   assert(0 && "unimplemented"); 
}	
Token match_num(Lexer *self) {
   assert(0 && "unimplemented"); 
}
Token match_iden(Lexer *self) {
   assert(0 && "unimplemented"); 
}
void lexer_deinit(Lexer *lexer) {
   assert(0 && "unimplemented"); 


}
