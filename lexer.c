
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
Token match_dots(Lexer *self);
void skip_ws(Lexer *lexer);
static struct lexer_state
{
	int is_section_begin; // section begin flag
	int is_attribute_begin;
} LEXER_STATE = {
	.is_section_begin = 0,
	.is_attribute_begin = 0,
};

const char *tk_str(TokenType ty)
{

	switch (ty)
	{
	case TK_NULL:
		return "TK_NULL";
	case TK_INT:
		return "TK_INT";
	case TK_IDENT:
		return "TK_IDENT";
	case TK_DOTS:
		return "TK_DOTS";
	case TK_QUAL:
		return "TK_QUAL";
	case ';':
		return ";";
	case '=':
		return "=";
	case ',':
		return ",";
	case ':':
		return ":";
	case '[':
		return "[";
	case ']':
		return "]";
	case '/':
		return "/";
	case '<':
		return "<";
	case '>':
		return ">";
	default:
		return "TK_UNKNOWN";
	}
}
LexerDummy lexer_dummy_init(const char *src, const size_t src_len, const char *path)
{
	return (LexerDummy){.path = path, .src = src, .src_len = src_len};
}
Lexer lexer_init(char *src, const size_t src_len, const char *path)
{
	Lexer lexer = {.src = src, .src_len = src_len, .off = 0, .path = path, .peakbuf = {0}, .sym_table = NULL};
	sh_new_arena(lexer.sym_table);

	return lexer;
}
void lexer_deinit(Lexer *self)
{
	shfree(self->sym_table);
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
			loc.col = 1;
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

void skip_comment(Lexer *lexer)
{
	if (lexer->off + 1 >= lexer->src_len)
		return;

	if (next_char(lexer) == '/' && peek_char(lexer) == '/')
	{
		while (lexer->off < lexer->src_len)
		{
			if (next_char(lexer) == '\n') {
				skip_ws(lexer);
				skip_comment(lexer);
				break;
			}
		}
	}
	else
		rewind_char(lexer);
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
	case ',':
	case '|':
	case '=':
	case ':':
	case ';':
	case '/':
	case '<':
	case '>':
		tk.type = (unsigned char)c;
		return tk;
	default:
		rewind_char(self);
		RETURN_TK(TK_NULL);
	}
	// return tk;
}
Token match_qualifier(Lexer *self)
{
	Token tk = {0};
	char c = peek_char(self);
	size_t start = self->off;
	bool maybe_ident = true;
	if (c != 'o' && c != 's' && c != '#' && c != 'b')
	{
		RETURN_TK(TK_NULL);
	}
	tk.type = TK_QUAL;
	while ((c = next_char(self)) != 0)
	{
		switch (c)
		{
		case 'o':
			tk.data.qualifier.octave += 1;
			break;
		case 's':
			tk.data.qualifier.suboctave += 1;
			break;
		case '#':
			tk.data.qualifier.sharps += 1;
			maybe_ident = false;
			break;
		case 'b':
			tk.data.qualifier.flats += 1;
			break;
		case '\'':
			tk.off = self->off;
			return tk;
		default:
			if (maybe_ident)
			{
				self->off = start;
				return match_ident(self);
			}
			report(self, self->off, "Invalid character %c in sequence of pitch qualifiers", c);
			THROW_EXCEPT();
		}
	}
	report(self, self->off, "Unterminated sequence of pitch qualifiers (expects ')");
	THROW_EXCEPT();
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
			THROW_EXCEPT();
		}
		else
		{
			rewind_char(self);
			break;
		}
	}
	tk.type = TK_INT;
	tk.data.integer = strtol(self->src + tk.off, NULL, 10);

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
// 			THROW_EXCEPT();
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
			// THROW_EXCEPT();
		}
	}

	tk.type = TK_IDENT;
	c = self->src[self->off];
	// a hack because stb sh wants NULL terminated string
	self->src[self->off] = '\0';
	shput(self->sym_table, self->src + tk.off, '\0');
	ptrdiff_t i = shgeti(self->sym_table, self->src + tk.off);
	tk.data.integer = i;
	self->src[self->off] = c; // restore the char
	return tk;
}
Token match_dots(Lexer *self)
{
	Token tk;
	char c = peek_char(self);
	if (c != '.')
	{
		RETURN_TK(TK_NULL);
	}
	next_char(self);
	size_t ct = 1;
	while ((c = next_char(self)) != 0)
	{
		if (c == '.')
		{
			ct += 1;
		}
		else if (isalnum(c))
		{
			report(self, self->off, "Invalid character '%c' in sequence of dots", c);
			THROW_EXCEPT();
		}
		else
		{
			rewind_char(self);
			break;
		}
	}
	tk.type = TK_DOTS;
	tk.data.integer = ct;
	tk.off = self->off;
	return tk;
}
typedef Token (*match_fn)(Lexer *);
match_fn fns[] = {
	match_dots,
	match_qualifier,
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
	skip_comment(self);

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
