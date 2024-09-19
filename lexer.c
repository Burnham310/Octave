
#include "lexer.h"
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#define eprintf(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define report(lexer, off, fmt, ...)                                                     \
	do                                                                                   \
	{                                                                                    \
		Loc __loc = off_to_loc(lexer->src, lexer->src_len, off);                         \
		eprintf("%s:%zu:%zu " fmt "\n", lexer->path, __loc.row, __loc.col, __VA_ARGS__); \
	} while (0)

static struct lexer_state
{
	int is_section_begin; // section begin flag
	int is_attribute_begin;
} LEXER_STATE = {
	.is_section_begin = 0,
	.is_attribute_begin = 0,
};

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
		LEXER_STATE.is_section_begin = !LEXER_STATE.is_section_begin;
	case '=':
	case ':':
		LEXER_STATE.is_attribute_begin = !LEXER_STATE.is_attribute_begin;
	case ';':
		tk.type = (unsigned char)c;
		return tk;
	default:
		rewind_char(self);
		RETURN_TK(TK_NULL);
	}
	// return tk;
}
Token match_multiple(Lexer *self)
{
	assert(0 && "unimplemented");
}
Token match_num(Lexer *self)
{
	Token tk = {.off = self->off};
	
	char c = next_char(self);
	if (c == 0) RETURN_TK(TK_EOF);
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
			break;
		}
	}
	tk.type = TK_INT;

	tk.data.integer = strtol(self->src + tk.off, NULL, 10);
	return tk;
}

Token match_many(Lexer *self, const TokenType exp_token_type, const char *target)
{
	Token tk = {.off = self->off};
	int len = strlen(target);

	for (int i = 0; i < len; ++i)
	{
		if (next_char(self) != target[i])
		{
			do
			{
				rewind_char(self);
				--i;
			} while (i > 0);
			RETURN_TK(TK_ERR);
		}
	}

	RETURN_TK(exp_token_type);
}


Token match_ident(Lexer *self)
{
	Token tk = {.off = self->off};
	char c = next_char(self);
	if (c == 0) RETURN_TK(TK_EOF);
	if (!isalpha(c) && c != '_')
	{
		rewind_char(self);
		RETURN_TK(TK_NULL);
	}

	while ((c = next_char(self)) != 0)
	{
		if (c == ' ' || c == '\t' || c == '\n' || c == '=' || c == ',')
		{
			rewind_char(self);
			break;
		}
		else if (isalpha(c) || c == '_')
		{
			continue;
		}
		else
		{
			// TODO report error
			report(self, self->off, "invalid character '%c' (%d) in ident", c, c);
			RETURN_TK(TK_ERR);
		}
	}

	tk.type = TK_IDENT;
	tk.data.str = (Slice){.ptr = self->src + tk.off, .len = self->off - tk.off};
	return tk;
}
void lexer_uninit(Lexer *lexer)
{
	return;
}

Token lexer_next(Lexer *self)
{
	if (self->peakbuf.type > 0)
	{
		const Token tk = self->peakbuf;
		self->peakbuf.type = TK_NULL;
		return tk;
	}
	skip_ws(self);

	Token tk = match_single(self);
	if (tk.type != 0)
		return tk;


	tk = match_num(self);
	if (tk.type != 0)
		return tk;

	tk = match_ident(self);
	return tk;
}

Token lexer_peak(Lexer *self)
{
	if (self->peakbuf.type > 0)
		return self->peakbuf;
	const Token tk = lexer_next(self);
	self->peakbuf = tk;
	return tk;
}
