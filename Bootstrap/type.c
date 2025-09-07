static void type_list_push(struct type_node **first, struct type_node **last, struct type_node *node) {
	if (*first) {
		(*last)->next = node;
	} else {
		*first = node;
	}
	*last = node;
}

static struct type *interned_types_first;
static struct type *interned_types_last;

static struct type *type_intern(enum type_kind kind, struct type *inner, struct type_node *first) {
	struct type *result = 0;

	for (struct type *type = interned_types_first; type; type = type->next_interned) {
		if (type->kind == kind && type->inner == inner) {
			bool match = true;

			struct type_node *n1 = first;
			struct type_node *n2 = type->first;

			while (true) {
				bool n1_exists = n1;
				bool n2_exists = n2;
				if (n1_exists != n2_exists) {
					match = false;
					break;
				}

				if (!n1 || !n2) break;

				if (n1->type != n2->type) {
					match = false;
					break;
				}

				n1 = n1->next;
				n2 = n2->next;
			}

			if (match) {
				result = type;
				break;
			}
		}
	}

	if (!result) {
		result = calloc(1, sizeof(struct type));
		result->kind = kind;
		result->inner = inner;
		result->first = first;

		if (interned_types_first) {
			interned_types_last->next_interned = result;
		} else {
			interned_types_first = result;
		}
		interned_types_last = result;
	}

	return result;
}

static struct type *type_int(void) {
	return type_intern(type_kind_int, 0, 0);
}

static struct type *type_pointer(struct type *pointee) {
	return type_intern(type_kind_pointer, pointee, 0);
}

static struct type *type_proc(struct type_node *first_param, struct type *return_type) {
	return type_intern(type_kind_proc, return_type, first_param);
}

static struct type *type_from_expr(struct node *expr) {
	switch (expr->kind) {
	case node_kind_name:
		if (strcmp(expr->name, "int") == 0) return type_int();
		error(expr->line, "unknown type “%s”", expr->name);

	case node_kind_address:
		return type_pointer(type_from_expr(expr->first));

	case node_kind_proc_type: {
		struct node *params_expr = node_find(expr, node_kind_params);

		struct type_node *first_param = 0;
		struct type_node *last_param = 0;

		for (struct node *param_expr = params_expr->first; !node_is_nil(param_expr);
		        param_expr = param_expr->next) {
			struct type_node *param = calloc(1, sizeof(struct type_node));
			param->type = type_from_expr(param_expr);
			type_list_push(&first_param, &last_param, param);
		}

		struct node *return_type_expr = node_find(expr, node_kind_type)->first;
		return type_proc(first_param, type_from_expr(return_type_expr));
	}

	case node_kind_nil:
		return 0;

	default:
		error(expr->line, "cannot use non-type expression as type");
	}
}

struct buf {
	char *s;
	size_t capacity;
	size_t length;
};

static struct buf buf_make(void) {
	struct buf result = {0};
	result.capacity = 128;
	result.s = calloc(result.capacity, sizeof(char));
	return result;
}

__attribute__((format(printf, 2, 3))) static void buf_printf(struct buf *buf, char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	size_t remaining = buf->capacity - buf->length;
	int wanted = vsnprintf(buf->s + buf->length, remaining, fmt, ap);
	assert(wanted >= 0);
	buf->length += (size_t)wanted > remaining ? remaining : (size_t)wanted;
	va_end(ap);
}

static char *type_print(struct type *type) {
	if (!type) return "non-value";
	struct buf buf = buf_make();

	switch (type->kind) {
	case type_kind_int:
		buf_printf(&buf, "int");
		break;

	case type_kind_pointer:
		buf_printf(&buf, "*%s", type_print(type->inner));
		break;

	case type_kind_proc:
		buf_printf(&buf, "proc(");
		for (struct type_node *param = type->first; param; param = param->next) {
			buf_printf(&buf, "%s", type_print(param->type));
			if (param->next) buf_printf(&buf, ", ");
		}
		buf_printf(&buf, ")");
		if (type->inner) buf_printf(&buf, " %s", type_print(type->inner));
		break;
	}

	return buf.s;
}

static void expect_types_equal(struct type *expected, struct type *actual, size_t line) {
	if (expected == actual) return;
	error(line, "expected “%s” but found “%s”", type_print(expected), type_print(actual));
}

struct symbol {
	struct symbol *next;
	struct local *local;
	struct entity *entity;
};

struct scope {
	struct scope *up;
	struct symbol *first;
	struct symbol *last;
};

static struct node *current_root;
static struct entity *g_first_entity;
static struct scope *deepest_scope;

static char *symbol_name(struct symbol *symbol) {
	char *result = 0;
	if (symbol->local) {
		result = symbol->local->name;
	} else {
		result = symbol->entity->name;
	}
	return result;
}

static void scope_push(void) {
	struct scope *scope = calloc(1, sizeof(struct scope));
	scope->up = deepest_scope;
	deepest_scope = scope;
}

static void scope_pop(void) {
	deepest_scope = deepest_scope->up;
}

static struct symbol *scope_find(char *name) {
	struct symbol *result = 0;

	for (struct scope *scope = deepest_scope; scope; scope = scope->up) {
		for (struct symbol *symbol = scope->first; symbol; symbol = symbol->next) {
			if (strcmp(name, symbol_name(symbol)) == 0) {
				result = symbol;
				goto done;
			}
		}
	}

done:
	return result;
}

static void scope_add(struct symbol *symbol, size_t line) {
	char *name = symbol_name(symbol);
	if (scope_find(name)) error(line, "cannot redefine symbol “%s”", name);

	if (deepest_scope->first) {
		deepest_scope->last->next = symbol;
	} else {
		deepest_scope->first = symbol;
	}
	deepest_scope->last = symbol;
}

static void scope_add_local(struct local *local, size_t line) {
	struct symbol *symbol = calloc(1, sizeof(struct symbol));
	symbol->local = local;
	scope_add(symbol, line);
}

static void scope_add_entity(struct entity *entity, size_t line) {
	struct symbol *symbol = calloc(1, sizeof(struct symbol));
	symbol->entity = entity;
	scope_add(symbol, line);
}

static struct local *add_local(struct entity *proc, char *name, struct type *type) {
	assert(proc->kind == entity_kind_proc);
	struct local *local = calloc(1, sizeof(struct local));
	local->next = proc->first_local;
	local->name = name;
	local->type = type;
	local->offset = 8;
	if (proc->first_local) local->offset += proc->first_local->offset;
	proc->first_local = local;
	proc->locals_size += 8;
	return local;
}

static void check_node(struct entity *proc, struct node *node) {
	switch (node->kind) {
	case node_kind_number:
		node->type = type_int();
		break;

	case node_kind_local: {
		struct node *type_expr = node_find(node, node_kind_type)->first;
		struct node *initializer = node_find(node, node_kind_initializer);

		struct type *type = 0;
		if (node_is_nil(initializer)) {
			type = type_from_expr(type_expr);
		} else {
			struct node *initializer_expr = initializer->first;
			check_node(proc, initializer_expr);

			if (!initializer_expr->type) {
				error(initializer_expr->line,
				        "cannot initialize variable using expression without value");
			}

			if (node_is_nil(type_expr)) {
				type = initializer_expr->type;
			} else {
				type = type_from_expr(type_expr);
				expect_types_equal(type, initializer_expr->type, initializer->line);
			}
		}

		node->local = add_local(proc, node->name, type);
		scope_add_local(node->local, node->line);
		break;
	}

	case node_kind_name: {
		struct symbol *symbol = scope_find(node->name);
		if (!symbol) error(node->line, "unknown name “%s”", node->name);

		if (symbol->local) {
			node->local = symbol->local;
			node->type = node->local->type;
		} else {
			assert(symbol->entity->kind == entity_kind_proc);
			node->entity = symbol->entity;
			node->type = node->entity->type;
		}

		break;
	}

	case node_kind_assign: {
		struct node *lhs = node->first;
		struct node *rhs = lhs->next;
		check_node(proc, lhs);
		check_node(proc, rhs);
		expect_types_equal(lhs->type, rhs->type, rhs->line);
		break;
	}

	case node_kind_expr_stmt: {
		struct node *expr = node->first;
		check_node(proc, expr);
		if (expr->type) error(expr->line, "unused expression");
		break;
	}

	case node_kind_return: {
		struct node *return_value = node->first;
		if (node_is_nil(return_value)) {
			if (proc->type->inner) {
				error(node->line, "missing return value");
			}
		} else {
			if (!proc->type->inner) {
				error(node->line, "cannot return value from procedure with no return value");
			}
			check_node(proc, return_value);
			expect_types_equal(proc->type->inner, return_value->type, return_value->line);
		}
		break;
	}

	case node_kind_address: {
		struct node *operand = node->first;
		check_node(proc, operand);
		node->type = type_pointer(operand->type);
		break;
	}

	case node_kind_deref: {
		struct node *operand = node->first;
		check_node(proc, operand);
		if (operand->type->kind != type_kind_pointer) {
			error(node->line, "can’t dereference non-pointer type “%s”", type_print(operand->type));
		}
		node->type = operand->type->inner;
		break;
	}

	case node_kind_call: {
		struct node *callee = node->first;
		check_node(proc, callee);

		if (callee->type->kind != type_kind_proc) {
			error(node->line, "cannot call value of non-procedure type “%s”", type_print(callee->type));
		}

		struct node *args = node->first->next;
		size_t arg_count = node_kid_count(node) - 1;

		size_t param_count = 0;
		for (struct type_node *param = callee->type->first; param; param = param->next) {
			param_count++;
		}

		if (arg_count != param_count) {
			error(node->line, "expected %zu arguments but found %zu", param_count, arg_count);
		}

		struct node *arg = args;
		struct type_node *param = callee->type->first;
		while (!node_is_nil(arg) && param) {
			check_node(proc, arg);
			expect_types_equal(param->type, arg->type, arg->line);
			arg = arg->next;
			param = param->next;
		}

		node->type = callee->type->inner;
		break;
	}

	case node_kind_add:
	case node_kind_sub:
	case node_kind_mul:
	case node_kind_div: {
		struct node *lhs = node->first;
		struct node *rhs = lhs->next;
		check_node(proc, lhs);
		check_node(proc, rhs);
		expect_types_equal(type_int(), lhs->type, lhs->line);
		expect_types_equal(type_int(), rhs->type, rhs->line);
		node->type = type_int();
		break;
	}

	case node_kind_block:
		scope_push();
		for (struct node *kid = node->first; !node_is_nil(kid); kid = kid->next) {
			check_node(proc, kid);
		}
		scope_pop();
		break;

	case node_kind_nil:
	case node_kind_root:
	case node_kind_proc:
	case node_kind_proc_type:
	case node_kind_param:
	case node_kind_params:
	case node_kind_initializer:
	case node_kind_type:
	case node_kind__last:
		unreachable();
	}
}

static struct entity *typecheck(struct node *root) {
	current_root = root;

	g_first_entity = 0;
	struct entity *last_entity = 0;

	for (struct node *node = root->first; !node_is_nil(node); node = node->next) {
		assert(node->kind == node_kind_proc);

		struct entity *entity = calloc(1, sizeof(struct entity));
		entity->kind = entity_kind_proc;
		entity->name = node->name;
		entity->node = node;

		struct param *first_param = 0;
		struct param *last_param = 0;
		struct type_node *first_param_type_node = 0;
		struct type_node *last_param_type_node = 0;

		struct node *params = node_find(node, node_kind_params);
		for (struct node *kid = params->first; !node_is_nil(kid); kid = kid->next) {
			struct param *param = calloc(1, sizeof(struct param));
			param->name = kid->name;
			param->node = kid;
			param->type = type_from_expr(kid->first);
			param->local = add_local(entity, param->name, param->type);

			if (first_param) {
				last_param->next = param;
			} else {
				first_param = param;
			}
			last_param = param;

			struct type_node *type_node = calloc(1, sizeof(struct type_node));
			type_node->type = param->type;
			type_list_push(&first_param_type_node, &last_param_type_node, type_node);

			entity->param_count++;
		}

		entity->first_param = first_param;

		struct type *return_type = 0;
		struct node *return_type_expr = node_find(node, node_kind_type);
		if (return_type_expr) {
			return_type = type_from_expr(return_type_expr->first);
		}
		entity->type = type_proc(first_param_type_node, return_type);

		entity->body = node_find(node, node_kind_block);

		if (g_first_entity) {
			last_entity->next = entity;
		} else {
			g_first_entity = entity;
		}
		last_entity = entity;
	}

	deepest_scope = 0;
	scope_push();
	for (struct entity *entity = g_first_entity; entity; entity = entity->next) {
		assert(entity->kind == entity_kind_proc);
		scope_add_entity(entity, entity->node->line);
	}

	for (struct entity *entity = g_first_entity; entity; entity = entity->next) {
		assert(entity->kind == entity_kind_proc);
		scope_push();
		for (struct param *param = entity->first_param; param; param = param->next) {
			scope_add_local(param->local, param->node->line);
		}
		check_node(entity, entity->body);
		scope_pop();
	}

	return g_first_entity;
}
