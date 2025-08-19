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
	token_kind__last,
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

enum node_kind {
	node_kind_nil,
	node_kind_root,
	node_kind_number,
	node_kind_add,
	node_kind_sub,
	node_kind_mul,
	node_kind_div,
};

static char *const node_kind_strings[] = {
        [node_kind_nil] = "nil",
        [node_kind_root] = "root",
        [node_kind_number] = "number",
        [node_kind_add] = "add",
        [node_kind_sub] = "sub",
        [node_kind_mul] = "mul",
        [node_kind_div] = "div",
};

struct node {
	struct node *next;
	struct node *prev;
	struct node *children;
	enum node_kind kind;
	uint64_t value;
};

static struct node *node_create(enum node_kind kind) {
	struct node *node = calloc(1, sizeof(struct node));
	node->kind = kind;
	node->children = calloc(1, sizeof(struct node));
	node->children->next = node->children;
	node->children->prev = node->children;
	return node;
}

static void node_add_child(struct node *node, struct node *child) {
	child->prev = node->children->prev;
	child->next = node->children;
	child->prev->next = child;
	child->next->prev = child;
}

static void node_print_with_indentation(struct node *node, size_t indentation) {
	for (size_t i = 0; i < indentation; i++) fprintf(stderr, "\t");
	fprintf(stderr, "%s (%llu)\n", node_kind_strings[node->kind], node->value);
	for (struct node *child = node->children->next; child != node->children; child = child->next) {
		node_print_with_indentation(child, indentation + 1);
	}
}

__attribute__((unused)) static void node_print(struct node *node) {
	node_print_with_indentation(node, 0);
}

struct parser {
	struct token *token;
	size_t remaining_tokens;
	struct node *root;
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
	struct token *lhs_token = parser_expect(p, token_kind_number);
	struct node *lhs = node_create(node_kind_number);
	lhs->value = parse_number(lhs_token->string);

	static const enum node_kind node_kinds[token_kind__last] = {
	        [token_kind_plus] = node_kind_add,
	        [token_kind_hyphen] = node_kind_sub,
	        [token_kind_asterisk] = node_kind_mul,
	        [token_kind_slash] = node_kind_div,
	};

	enum node_kind expr_node_kind = node_kinds[parser_current(p)];
	if (expr_node_kind == 0) {
		printf("%zu: expected operator\n", p->token->line);
		exit(1);
	}
	parser_bump(p, parser_current(p));

	struct token *rhs_token = parser_expect(p, token_kind_number);
	struct node *rhs = node_create(node_kind_number);
	rhs->value = parse_number(rhs_token->string);

	struct node *expr = node_create(expr_node_kind);
	node_add_child(expr, lhs);
	node_add_child(expr, rhs);
	node_add_child(p->root, expr);
}

static struct node *parse(char *s) {
	struct tokens tokens = lex(s);
	struct parser p = {0};
	p.token = tokens.ptr;
	p.remaining_tokens = tokens.count;
	p.root = node_create(node_kind_root);
	parse_expr(&p);
	return p.root;
}

static void codegen(struct node *root, FILE *file) {
	fprintf(file, ".global _main\n");
	fprintf(file, "_main:\n");

	for (struct node *child = root->children->next; child != root->children; child = child->next) {
		struct node *lhs = child->children->next;
		struct node *rhs = child->children->next->next;
		fprintf(file, "\tmov x0, #%llu\n", lhs->value);
		fprintf(file, "\tmov x1, #%llu\n", rhs->value);

		switch (child->kind) {
		case node_kind_add:
			fprintf(file, "\tadd x0, x0, x1\n");
			break;

		case node_kind_sub:
			fprintf(file, "\tsub x0, x0, x1\n");
			break;
		case node_kind_mul:
			fprintf(file, "\tmul x0, x0, x1\n");
			break;

		case node_kind_div:
			fprintf(file, "\tsdiv x0, x0, x1\n");
			break;

		default:
			unreachable();
		}

		fprintf(file, "\tret\n");
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

	struct node *root = parse(buffer);
	codegen(root, stdout);
}
