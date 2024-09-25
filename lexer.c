
#include "lexer.h"
#include "utils.h"
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

char next_char(Lexer *self);
Token match_single(Lexer *self);
Token match_multiple(Lexer *self);
Token match_num(Lexer *self);
Token match_ident(Lexer *self);
void skip_ws(Lexer *lexer);
static struct lexer_state
{
	int is_section_begin; // section begin flag
	int is_attribute_begin;
} LEXER_STATE = {
	.is_section_begin = 0,
	.is_attribute_begin = 0,
};

const char *ty_str(TokenType ty)
{

	switch (ty)
	{
	case TK_ERR:
		return "TK_ERR";
	case TK_NULL:
		return "TK_NULL";
	case TK_INT:
		return "TK_INT";
	case TK_IDENT:
		return "TK_IDENT";
	case TK_NOTE:
		return "TK_NOTE";
	case ';':
		return ";";
	case '=':
		return "=";
	case '.':
		return ".";
	case ',':
		return ",";
	case ':':
		return ":";
	default:
		return "TK_UNKNOWN";
	}
}
LexerDummy lexer_dummy_init(const char *src, const size_t src_len, const char *path)
{
	return (LexerDummy){.path = path, .src = src, .src_len = src_len};
}
Lexer lexer_init(const char *src, const size_t src_len, const char *path)
{
	Lexer lexer = {.src = src, .src_len = src_len, .off = 0, .path = path, .peakbuf = {0}};

	return lexer;
}
Loc off_to_loc(const char *src, const size_t src_len, const size_t off)
{
	Loc loc = {.row = 1, .col = 1};
	assert(off < src_len);
	for (size_t i = 0; i < off; ++i)
	{
		char c = src[i];
		if (c == '\n')
		{
			loc.row++;
		}
		else
		{
			loc.col++;
		}
	}
	return loc;
}

void rewind_char(Lexer *self)
{
	self->off--;
}

void skip_ws(Lexer *lexer)
{
	while (lexer->off < lexer->src_len)
	{
		char c = lexer->src[lexer->off];
		switch (c)
		{
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
char peek_char(Lexer *self)
{
	if (self->off >= self->src_len)
		return 0;
	return self->src[self->off];
}
// return 0 if no more character left
char next_char(Lexer *self)
{
	if (self->off >= self->src_len)
		return 0;
	return self->src[self->off++];
}

#define RETURN_TK(x) \
	do               \
	{                \
		tk.type = x; \
		return tk;   \
	} while (0)
Token match_single(Lexer *self)
{

	char c;
	Token tk = {.off = self->off};
	switch ((c = next_char(self)))
	{
	case 0:
		RETURN_TK(TK_NULL);
	case '[':
	case ']':
	case '.':
	case ',':
	case '|':
	case '=':
	case ':':
	case ';':
		tk.type = (unsigned char)c;
		return tk;
	default:
		rewind_char(self);
		RETURN_TK(TK_NULL);
	}
	// return tk;
}

Token match_num(Lexer *self)
{
	Token tk = {.off = self->off};

	char c = next_char(self);
	if (c == 0)
		RETURN_TK(TK_EOF);
	if (!isdigit(c))
	{
		rewind_char(self);
		RETURN_TK(TK_NULL);
	}
	while ((c = next_char(self)) != 0)
	{
		if (isdigit(c))
		{
			continue;
		}
		else if (isalpha(c))
		{
			report(self, self->off, "invalid character '%c' (%d) in integer", c, c);
			RETURN_TK(TK_ERR);
		}
		else
		{
			rewind_char(self);
			break;
		}
	}
	tk.type = TK_INT;
	tk.data.integer = strtol(self->src + tk.off, NULL, 10);
	if ((c = peek_char(self)) == '.')
	{
		int dots = 0;
		while ((c = next_char(self)) == '.')
		{
			dots += 1;
		}
		rewind_char(self);

		tk.type = TK_NOTE;
		tk.data.note.pitch = tk.data.integer;
		tk.data.note.dots = dots;
	}

	return tk;
}

// Token match_many(Lexer *self, const TokenType exp_token_type, const char *target)
// {
// 	Token tk = {.off = self->off};
// 	int len = strlen(target);
//
// 	for (int i = 0; i < len; ++i)
// 	{
// 		if (next_char(self) != target[i])
// 		{
// 			do
// 			{
// 				rewind_char(self);
// 				--i;
// 			} while (i > 0);
// 			RETURN_TK(TK_ERR);
// 		}
// 	}
//
// 	RETURN_TK(exp_token_type);
// }

Token match_ident(Lexer *self)
{
	Token tk = {.off = self->off};
	char c = next_char(self);
	if (c == 0)
		RETURN_TK(TK_EOF);
	if (!isalpha(c) && c != '_')
	{
		rewind_char(self);
		RETURN_TK(TK_NULL);
	}

	while ((c = next_char(self)) != 0)
	{
		if (isalpha(c) || c == '_')
		{
			continue;
		}
		else
		{
			rewind_char(self);
			break;
			// report(self, self->off, "invalid character '%c' (%d) in ident", c, c);
			// RETURN_TK(TK_ERR);
		}
	}

	tk.type = TK_IDENT;
	tk.data.str = (Slice){.ptr = self->src + tk.off, .len = self->off - tk.off};
	// const char* kw[] = {"key", "bpm"};
	// TokenType kw_type[] = { TK_KEY, TK_BPM, };
	// size_t kw_ct = sizeof(kw) / sizeof(kw[0]);
	// for (size_t i = 0; i < kw_ct; ++i) {
	//     if (strncmp(kw[i], tk.data.str.ptr, tk.data.str.len) == 0) {
	// 	tk.type = kw_type[i];
	// 	break;
	//     }
	// }
	return tk;
}
typedef Token (*match_fn)(Lexer *);
match_fn fns[] = {
	match_single,
	match_num,
	match_ident,
};
const size_t fns_len = sizeof(fns) / sizeof(match_fn);
Token lexer_next(Lexer *self)
{
	if (self->peakbuf.type > 0)
	{
		const Token tk = self->peakbuf;
		self->peakbuf.type = TK_NULL;
		return tk;
	}
	skip_ws(self);

	Token tk;
	for (int i = 0; i < fns_len; i++)
	{
		tk = fns[i](self);
		if (tk.type != 0)
			break;
	}
	return tk;
}

Token lexer_peek(Lexer *self)
{
	if (self->peakbuf.type > 0)
		return self->peakbuf;
	const Token tk = lexer_next(self);
	self->peakbuf = tk;
	return tk;
}
