#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

_Noreturn static void unreachable(void) {
	__builtin_trap();
}

static void assert(bool b) {
	if (!b) unreachable();
}

enum token_kind {
	token_kind_name = 1,
	token_kind_number,
	token_kind_plus,
	token_kind_hyphen,
	token_kind_asterisk,
	token_kind_slash,
	token_kind_eof,
};

static char *const token_kind_strings[] = {
        [token_kind_name] = "name",
        [token_kind_number] = "number",
        [token_kind_plus] = "“+”",
        [token_kind_hyphen] = "“-”",
        [token_kind_asterisk] = "“*”",
        [token_kind_slash] = "“/”",
        [token_kind_eof] = "end of file",
};

struct token {
	enum token_kind kind;
	size_t line;
	char *string;
};

struct tokens {
	struct token *ptr;
	size_t count;
};

static bool is_alpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(char c) {
	return c >= '0' && c <= '9';
}

static struct tokens lex(char *s) {
	size_t count = 0;
	size_t capacity = 8;
	struct token *tokens = malloc(sizeof(struct token) * capacity);
	size_t line = 1;

	while (*s) {
		if (*s == ' ' || *s == '\t' || *s == '\n') {
			if (*s == '\n') line++;
			s++;
			continue;
		}

		struct token token = {0};
		char *start = s;

		if (is_alpha(*s) || *s == '_') {
			token.kind = token_kind_name;
			do {
				s++;
			} while (is_alpha(*s) || is_digit(*s) || *s == '_');
		} else if (is_digit(*s)) {
			token.kind = token_kind_number;
			do {
				s++;
			} while (is_digit(*s));
		} else {
			static const enum token_kind single_char_kinds[256] = {
			        ['+'] = token_kind_plus,
			        ['-'] = token_kind_hyphen,
			        ['*'] = token_kind_asterisk,
			        ['/'] = token_kind_slash,
			};

			token.kind = single_char_kinds[(size_t)*s];
			if (token.kind == 0) {
				printf("%zu: invalid token “%c”\n", line, *s);
				exit(1);
			}

			s++;
		}

		size_t len = (size_t)(s - start);
		char *token_string = calloc(len + 1, 1);
		token.string = memcpy(token_string, start, len);
		token.line = line;

		assert(count <= capacity);
		if (count == capacity) {
			capacity *= 2;
			tokens = realloc(tokens, sizeof(struct token) * capacity);
		}
		tokens[count++] = token;
	}

	struct tokens result = {0};
	result.ptr = tokens;
	result.count = count;
	return result;
}

struct expr {
	uint64_t lhs;
	uint64_t rhs;
	enum token_kind op;
};

struct parser {
	struct token *token;
	size_t remaining_tokens;
	struct expr expr;
};

static uint64_t parse_number(char *s) {
	uint64_t result = 0;
	for (; *s; s++) {
		if (!is_digit(*s)) break;
		result *= 10;
		result += (uint64_t)(*s - '0');
	}
	return result;
}

static enum token_kind parser_current(struct parser *p) {
	return p->token->kind;
}

static struct token *parser_bump(struct parser *p, enum token_kind kind) {
	assert(parser_current(p) == kind);
	struct token *token = p->token;
	p->remaining_tokens--;
	if (p->remaining_tokens == 0) {
		struct token *eof_token = calloc(1, sizeof(struct token));
		eof_token->kind = token_kind_eof;
		eof_token->line = token->line;
		p->token = eof_token;
	} else {
		p->token++;
	}
	return token;
}

static struct token *parser_expect(struct parser *p, enum token_kind kind) {
	assert(kind != token_kind_eof);
	if (parser_current(p) != kind) {
		printf("%zu: expected %s, found %s\n", p->token->line, token_kind_strings[kind],
		        token_kind_strings[parser_current(p)]);
		exit(1);
	}
	return parser_bump(p, kind);
}

static void parse_expr(struct parser *p) {
	struct token *lhs = parser_expect(p, token_kind_number);

	struct token *op = 0;
	switch (parser_current(p)) {
	case token_kind_plus:
	case token_kind_hyphen:
	case token_kind_asterisk:
	case token_kind_slash:
		op = parser_bump(p, parser_current(p));
		break;

	default:
		printf("%zu: expected operator\n", p->token->line);
		exit(1);
		break;
	}

	struct token *rhs = parser_expect(p, token_kind_number);

	p->expr.lhs = parse_number(lhs->string);
	p->expr.rhs = parse_number(rhs->string);
	p->expr.op = op->kind;
}

static struct expr parse(char *s) {
	struct tokens tokens = lex(s);
	struct parser p = {0};
	p.token = tokens.ptr;
	p.remaining_tokens = tokens.count;
	parse_expr(&p);
	return p.expr;
}

static uint64_t eval(struct expr expr) {
	switch (expr.op) {
	case token_kind_plus:
		return expr.lhs + expr.rhs;
	case token_kind_hyphen:
		return expr.lhs - expr.rhs;
	case token_kind_asterisk:
		return expr.lhs * expr.rhs;
	case token_kind_slash:
		return expr.lhs / expr.rhs;
	default:
		unreachable();
	}
}

int main(void) {
	static const size_t buffer_size = 1024;
	char buffer[buffer_size] = {0};
	size_t n = 0;

	while (n < buffer_size - 1) {
		char c = (char)getchar();
		if (c == EOF) break;
		buffer[n++] = c;
	}

	struct expr expr = parse(buffer);
	printf("%llu\n", eval(expr));
}
