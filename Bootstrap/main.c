#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static void cm_assert(bool b) {
	if (!b) __builtin_trap();
}

enum token_kind {
	token_kind_name,
	token_kind_number,
};

static char *const token_kind_strings[] = {
        [token_kind_name] = "name",
        [token_kind_number] = "number",
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

static char *tokens_print(struct tokens tokens) {
	size_t capacity = 128 * 1024;
	char *result = malloc(capacity);
	char *s = result;
	size_t remaining = capacity;

	struct token *end = tokens.ptr + tokens.count;
	for (struct token *token = tokens.ptr; token != end; token++) {
		char *token_kind_string = token_kind_strings[token->kind];
		int return_value =
		        snprintf(s, remaining, "%s:%zu \"%s\"\n", token_kind_string, token->line, token->string);
		if (return_value < 0) break;

		size_t nprinted = (size_t)return_value;
		if (nprinted + 1 > remaining) break;
		remaining -= nprinted;
		s += nprinted;
	}

	return result;
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
		}

		size_t len = (size_t)(s - start);
		char *token_string = calloc(len + 1, 1);
		token.string = memcpy(token_string, start, len);
		token.line = line;

		cm_assert(count <= capacity);
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

int main(void) {
	static const size_t buffer_size = 1024;
	char buffer[buffer_size] = {0};
	size_t n = 0;

	while (n < buffer_size - 1) {
		char c = (char)getchar();
		if (c == EOF) break;
		buffer[n++] = c;
	}

	struct tokens tokens = lex(buffer);
	printf("%s", tokens_print(tokens));
}
