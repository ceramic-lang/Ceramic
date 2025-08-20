#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define countof(array) (sizeof(array) / sizeof((array)[0]))

_Noreturn static void unreachable(void) {
	__builtin_trap();
}

static void assert(bool b) {
	if (!b) unreachable();
}

enum token_kind {
	token_kind_name = 1,
	token_kind_number,

	token_kind_proc,
	token_kind_return,

	token_kind_plus,
	token_kind_hyphen,
	token_kind_asterisk,
	token_kind_slash,
	token_kind_semi,

	token_kind_lparen,
	token_kind_rparen,
	token_kind_lbrace,
	token_kind_rbrace,

	token_kind_eof,
	token_kind__last,
};

static char *const token_kind_strings[] = {
        [token_kind_name] = "name",
        [token_kind_number] = "number",

        [token_kind_proc] = "“proc”",
        [token_kind_return] = "“return”",

        [token_kind_plus] = "“+”",
        [token_kind_hyphen] = "“-”",
        [token_kind_asterisk] = "“*”",
        [token_kind_slash] = "“/”",
        [token_kind_semi] = "“;”",

        [token_kind_lparen] = "“(”",
        [token_kind_rparen] = "“)”",
        [token_kind_lbrace] = "“{”",
        [token_kind_rbrace] = "“}”",

        [token_kind_eof] = "end of file",
};

struct keyword {
	char *string;
	enum token_kind kind;
};

static const struct keyword keywords[] = {
        {"proc", token_kind_proc},
        {"return", token_kind_return},
};

struct token {
	enum token_kind kind;
	size_t line;
	char *string;
};

struct tokens {
	struct token *ptr;
	size_t count;
	size_t capacity;
};

static struct token *tokens_push(struct tokens *tokens) {
	assert(tokens->count <= tokens->capacity);
	if (tokens->count == tokens->capacity) {
		tokens->capacity = tokens->capacity == 0 ? 8 : 2 * tokens->capacity;
		tokens->ptr = realloc(tokens->ptr, sizeof(struct token) * tokens->capacity);
	}
	struct token *token = tokens->ptr + tokens->count;
	tokens->count++;
	return token;
}

static bool is_alpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_digit(char c) {
	return c >= '0' && c <= '9';
}

static struct tokens lex(char *s) {
	struct tokens tokens = {0};
	size_t line = 1;

	while (*s) {
		if (*s == ' ' || *s == '\t' || *s == '\n') {
			if (*s == '\n') {
				if (tokens.count > 0) {
					struct token last_token = tokens.ptr[tokens.count - 1];

					static const uint64_t last_expression_token_kinds = (1 << token_kind_name) |
					                                                    (1 << token_kind_number) |
					                                                    (1 << token_kind_rparen);
					if (last_expression_token_kinds & (1 << last_token.kind)) {
						struct token auto_semi_token = last_token;
						auto_semi_token.kind = token_kind_semi;
						auto_semi_token.string = ";";
						*tokens_push(&tokens) = auto_semi_token;
					}
				}

				line++;
			}

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
			        [';'] = token_kind_semi,
			        ['('] = token_kind_lparen,
			        [')'] = token_kind_rparen,
			        ['{'] = token_kind_lbrace,
			        ['}'] = token_kind_rbrace,
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

		if (token.kind == token_kind_name) {
			for (size_t i = 0; i < countof(keywords); i++) {
				struct keyword keyword = keywords[i];
				if (strcmp(token.string, keyword.string) == 0) {
					token.kind = keyword.kind;
					break;
				}
			}
		}

		*tokens_push(&tokens) = token;
	}

	return tokens;
}

enum node_kind {
	node_kind_nil,
	node_kind_root,
	node_kind_proc,
	node_kind_return,
	node_kind_block,
	node_kind_number,
	node_kind_add,
	node_kind_sub,
	node_kind_mul,
	node_kind_div,
	node_kind__last,
};

static char *const node_kind_strings[] = {
        [node_kind_nil] = "nil",
        [node_kind_root] = "root",
        [node_kind_proc] = "proc",
        [node_kind_return] = "return",
        [node_kind_block] = "block",
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
	char *name;
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

static struct node *node_find(struct node *node, enum node_kind kind) {
	struct node *result = 0;
	for (struct node *child = node->children->next; child != node->children; child = child->next) {
		if (child->kind == kind) {
			result = child;
			break;
		}
	}
	return result;
}

static void node_print_with_indentation(struct node *node, size_t indentation) {
	for (size_t i = 0; i < indentation; i++) fprintf(stderr, "  ");

	fprintf(stderr, "%s", node_kind_strings[node->kind]);
	if (node->name) fprintf(stderr, " (%s)", node->name);
	fprintf(stderr, " (%llu)\n", node->value);

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

static bool parser_at(struct parser *p, enum token_kind kind) {
	return parser_current(p) == kind;
}

static struct token *parser_bump(struct parser *p, enum token_kind kind) {
	assert(parser_at(p, kind));
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
	if (!parser_at(p, kind)) {
		printf("%zu: expected %s, found %s\n", p->token->line, token_kind_strings[kind],
		        token_kind_strings[parser_current(p)]);
		exit(1);
	}
	return parser_bump(p, kind);
}

static struct node *parse_expr(struct parser *p);

static struct node *parse_lhs(struct parser *p) {
	switch (parser_current(p)) {
	case token_kind_number: {
		struct token *lhs_token = parser_bump(p, token_kind_number);
		struct node *lhs = node_create(node_kind_number);
		lhs->value = parse_number(lhs_token->string);
		return lhs;
	}

	case token_kind_lparen: {
		parser_bump(p, token_kind_lparen);
		struct node *expr = parse_expr(p);
		parser_expect(p, token_kind_rparen);
		return expr;
	}

	default:
		printf("%zu: expected expression\n", p->token->line);
		exit(1);
	}
}

static bool right_binds_tighter(enum node_kind left, enum node_kind right) {
	static const int binding_powers[node_kind__last] = {
	        [node_kind_add] = 1,
	        [node_kind_sub] = 1,
	        [node_kind_mul] = 2,
	        [node_kind_div] = 2,
	};
	return binding_powers[right] > binding_powers[left];
}

static struct node *parse_expr_rec(struct parser *p, enum node_kind left_node_kind) {
	struct node *lhs = parse_lhs(p);

	while (true) {
		static const enum node_kind node_kinds[token_kind__last] = {
		        [token_kind_plus] = node_kind_add,
		        [token_kind_hyphen] = node_kind_sub,
		        [token_kind_asterisk] = node_kind_mul,
		        [token_kind_slash] = node_kind_div,
		};

		enum node_kind right_node_kind = node_kinds[parser_current(p)];
		if (!right_binds_tighter(left_node_kind, right_node_kind)) break;
		parser_bump(p, parser_current(p));
		struct node *rhs = parse_expr_rec(p, right_node_kind);

		struct node *new_lhs = node_create(right_node_kind);
		node_add_child(new_lhs, lhs);
		node_add_child(new_lhs, rhs);
		lhs = new_lhs;
	}

	return lhs;
}

static struct node *parse_expr(struct parser *p) {
	return parse_expr_rec(p, node_kind_nil);
}

static struct node *parse_stmt(struct parser *p);

static struct node *parse_block(struct parser *p) {
	parser_bump(p, token_kind_lbrace);
	struct node *block = node_create(node_kind_block);
	while (!parser_at(p, token_kind_rbrace)) {
		struct node *stmt = parse_stmt(p);
		node_add_child(block, stmt);
	}
	parser_expect(p, token_kind_rbrace);
	return block;
}

static struct node *parse_stmt(struct parser *p) {
	switch (parser_current(p)) {
	case token_kind_return:
		parser_bump(p, token_kind_return);
		struct node *value = parse_expr(p);
		parser_expect(p, token_kind_semi);
		struct node *stmt = node_create(node_kind_return);
		node_add_child(stmt, value);
		return stmt;

	case token_kind_lbrace:
		return parse_block(p);

	default:
		printf("%zu: expected statement\n", p->token->line);
		exit(1);
	}
}

static struct node *parse_proc(struct parser *p) {
	parser_bump(p, token_kind_proc);
	struct token *name_token = parser_expect(p, token_kind_name);
	struct node *proc = node_create(node_kind_proc);
	proc->name = name_token->string;

	parser_expect(p, token_kind_lparen);
	parser_expect(p, token_kind_rparen);
	if (!parser_at(p, token_kind_lbrace)) {
		printf("%zu: expected procedure body\n", p->token->line);
		exit(1);
	}
	struct node *block = parse_block(p);
	node_add_child(proc, block);

	return proc;
}

static void parse_source_file(struct parser *p) {
	while (!parser_at(p, token_kind_eof)) {
		if (!parser_at(p, token_kind_proc)) {
			printf("%zu: expected procedure\n", p->token->line);
			exit(1);
		}
		struct node *proc = parse_proc(p);
		node_add_child(p->root, proc);
	}
}

static struct node *parse(char *s) {
	struct tokens tokens = lex(s);
	struct parser p = {0};
	p.token = tokens.ptr;
	p.remaining_tokens = tokens.count;
	p.root = node_create(node_kind_root);
	parse_source_file(&p);
	return p.root;
}

static void codegen_expr(struct node *expr, FILE *file) {
	switch (expr->kind) {
	case node_kind_number:
		fprintf(file, "\tmov x9, #%llu\n", expr->value);
		break;

	case node_kind_add:
	case node_kind_sub:
	case node_kind_mul:
	case node_kind_div:
		codegen_expr(expr->children->next, file);
		fprintf(file, "\tstr x9, [sp, #-16]!\n");
		codegen_expr(expr->children->next->next, file);
		fprintf(file, "\tldr x10, [sp], #16\n");
		switch (expr->kind) {
		case node_kind_add:
			fprintf(file, "\tadd x9, x10, x9\n");
			break;
		case node_kind_sub:
			fprintf(file, "\tsub x9, x10, x9\n");
			break;
		case node_kind_mul:
			fprintf(file, "\tmul x9, x10, x9\n");
			break;
		case node_kind_div:
			fprintf(file, "\tsdiv x9, x10, x9\n");
			break;
		default:
			unreachable();
		}
		break;

	default:
		unreachable();
	}
}

static void codegen_stmt(struct node *stmt, FILE *file) {
	switch (stmt->kind) {
	case node_kind_return:
		codegen_expr(stmt->children->next, file);
		fprintf(file, "\tmov x0, x9\n");
		fprintf(file, "\tret\n");
		break;

	case node_kind_block:
		for (struct node *child = stmt->children->next; child != stmt->children; child = child->next) {
			codegen_stmt(child, file);
		}
		break;

	default:
		unreachable();
	}
}

static void codegen(struct node *root, FILE *file) {
	assert(root->kind == node_kind_root);

	for (struct node *proc = root->children->next; proc != root->children; proc = proc->next) {
		assert(proc->kind == node_kind_proc);
		fprintf(file, ".global _%s\n", proc->name);
		fprintf(file, ".align 2\n");
		fprintf(file, "_%s:\n", proc->name);
		codegen_stmt(node_find(proc, node_kind_block), file);
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
