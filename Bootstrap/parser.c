enum token_kind {
	token_kind_name = 1,
	token_kind_number,

	token_kind_proc,
	token_kind_return,

	token_kind_plus,
	token_kind_hyphen,
	token_kind_asterisk,
	token_kind_slash,
	token_kind_caret,
	token_kind_equal,
	token_kind_colon,
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
        [token_kind_caret] = "“^”",
        [token_kind_equal] = "“=”",
        [token_kind_colon] = "“:”",
        [token_kind_semi] = "“;”",

        [token_kind_lparen] = "“(”",
        [token_kind_rparen] = "“)”",
        [token_kind_lbrace] = "“{”",
        [token_kind_rbrace] = "“}”",

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
	size_t capacity;
};

struct keyword {
	char *string;
	enum token_kind kind;
};

static const struct keyword keywords[] = {
        {"proc", token_kind_proc},
        {"return", token_kind_return},
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

					static const uint64_t last_expression_token_kinds =
					        (1 << token_kind_name) | (1 << token_kind_number) |
					        (1 << token_kind_rparen) | (1 << token_kind_caret);
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
			        ['^'] = token_kind_caret,
			        ['='] = token_kind_equal,
			        [':'] = token_kind_colon,
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

static struct node *node_create(enum node_kind kind, struct token *token) {
	struct node *node = calloc(1, sizeof(struct node));
	node->kind = kind;
	node->line = token ? token->line : 1;
	node->kids = calloc(1, sizeof(struct node));
	node->kids->next = node->kids;
	node->kids->prev = node->kids;
	return node;
}

static void node_add_kid(struct node *node, struct node *kid) {
	kid->prev = node->kids->prev;
	kid->next = node->kids;
	kid->prev->next = kid;
	kid->next->prev = kid;
}

static struct node *node_find(struct node *node, enum node_kind kind) {
	struct node *result = 0;
	for (struct node *kid = node->kids->next; kid != node->kids; kid = kid->next) {
		if (kid->kind == kind) {
			result = kid;
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

	for (struct node *kid = node->kids->next; kid != node->kids; kid = kid->next) {
		node_print_with_indentation(kid, indentation + 1);
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

static enum token_kind parser_peek(struct parser *p) {
	if (p->remaining_tokens == 0) return token_kind_eof;
	return (p->token + 1)->kind;
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
	struct node *result = 0;

	switch (parser_current(p)) {
	case token_kind_number: {
		struct token *token = parser_bump(p, token_kind_number);
		result = node_create(node_kind_number, token);
		result->value = parse_number(token->string);
		break;
	}

	case token_kind_name: {
		struct token *token = parser_bump(p, token_kind_name);
		result = node_create(node_kind_name, token);
		result->name = token->string;
		break;
	}

	case token_kind_asterisk: {
		struct token *token = parser_bump(p, token_kind_asterisk);
		struct node *pointee = parse_lhs(p);
		result = node_create(node_kind_address, token);
		node_add_kid(result, pointee);
		break;
	}

	case token_kind_lparen: {
		parser_bump(p, token_kind_lparen);
		result = parse_expr(p);
		parser_expect(p, token_kind_rparen);
		break;
	}

	default:
		printf("%zu: expected expression\n", p->token->line);
		exit(1);
	}

	if (parser_at(p, token_kind_caret)) {
		struct token *token = parser_bump(p, token_kind_caret);
		struct node *deref = node_create(node_kind_deref, token);
		node_add_kid(deref, result);
		result = deref;
	}

	return result;
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
		struct token *operator_token = parser_bump(p, parser_current(p));
		struct node *rhs = parse_expr_rec(p, right_node_kind);

		struct node *new_lhs = node_create(right_node_kind, operator_token);
		node_add_kid(new_lhs, lhs);
		node_add_kid(new_lhs, rhs);
		lhs = new_lhs;
	}

	return lhs;
}

static struct node *parse_expr(struct parser *p) {
	return parse_expr_rec(p, node_kind_nil);
}

static struct node *parse_stmt(struct parser *p);

static struct node *parse_block(struct parser *p) {
	struct token *lbrace_token = parser_bump(p, token_kind_lbrace);
	struct node *block = node_create(node_kind_block, lbrace_token);
	while (!parser_at(p, token_kind_rbrace)) {
		struct node *stmt = parse_stmt(p);
		node_add_kid(block, stmt);
	}
	parser_expect(p, token_kind_rbrace);
	return block;
}

static struct node *parse_stmt(struct parser *p) {
	switch (parser_current(p)) {
	case token_kind_return: {
		struct token *token = parser_bump(p, token_kind_return);
		struct node *value = parse_expr(p);
		parser_expect(p, token_kind_semi);
		struct node *stmt = node_create(node_kind_return, token);
		node_add_kid(stmt, value);
		return stmt;
	}

	case token_kind_lbrace:
		return parse_block(p);

	default: {
		if (parser_at(p, token_kind_name) && parser_peek(p) == token_kind_colon) {
			struct token *name = parser_bump(p, token_kind_name);
			struct node *local = node_create(node_kind_local, name);
			local->name = name->string;

			struct token *colon = parser_bump(p, token_kind_colon);
			struct node *type_expr = parse_expr(p);
			struct node *type = node_create(node_kind_type, colon);
			node_add_kid(type, type_expr);
			node_add_kid(local, type);

			if (!parser_at(p, token_kind_semi)) {
				struct token *equal = parser_expect(p, token_kind_equal);
				struct node *initializer_expr = parse_expr(p);
				struct node *initializer = node_create(node_kind_initializer, equal);
				node_add_kid(initializer, initializer_expr);
				node_add_kid(local, initializer);
			}

			parser_expect(p, token_kind_semi);
			return local;
		}

		struct node *lhs = parse_expr(p);
		struct token *equal = parser_expect(p, token_kind_equal);
		struct node *rhs = parse_expr(p);
		parser_expect(p, token_kind_semi);

		struct node *assign = node_create(node_kind_assign, equal);
		node_add_kid(assign, lhs);
		node_add_kid(assign, rhs);
		return assign;
	}
	}
}

static struct node *parse_proc(struct parser *p) {
	struct token *proc_token = parser_bump(p, token_kind_proc);
	struct token *name_token = parser_expect(p, token_kind_name);
	struct node *proc = node_create(node_kind_proc, proc_token);
	proc->name = name_token->string;

	parser_expect(p, token_kind_lparen);
	parser_expect(p, token_kind_rparen);

	if (!parser_at(p, token_kind_lbrace)) {
		struct node *return_type = parse_expr(p);
		node_add_kid(proc, return_type);
	}

	if (!parser_at(p, token_kind_lbrace)) {
		printf("%zu: expected procedure body\n", p->token->line);
		exit(1);
	}
	struct node *block = parse_block(p);
	node_add_kid(proc, block);

	return proc;
}

static void parse_source_file(struct parser *p) {
	while (!parser_at(p, token_kind_eof)) {
		if (!parser_at(p, token_kind_proc)) {
			printf("%zu: expected procedure\n", p->token->line);
			exit(1);
		}
		struct node *proc = parse_proc(p);
		node_add_kid(p->root, proc);
	}
}

static struct node *parse(char *s) {
	struct tokens tokens = lex(s);
	struct parser p = {0};
	p.token = tokens.ptr;
	p.remaining_tokens = tokens.count;
	p.root = node_create(node_kind_root, p.token);
	parse_source_file(&p);
	return p.root;
}
