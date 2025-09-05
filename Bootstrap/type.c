static struct type *first_type;

static struct type *type_int(void) {
	for (struct type *type = first_type; type; type = type->next) {
		if (type->kind == type_kind_int) return type;
	}

	struct type *type = calloc(1, sizeof(struct type));
	type->kind = type_kind_int;
	type->next = first_type;
	first_type = type;
	return type;
}

static struct type *type_pointer(struct type *pointee) {
	for (struct type *type = first_type; type; type = type->next) {
		if (type->kind == type_kind_pointer && type->inner == pointee) return type;
	}

	struct type *type = calloc(1, sizeof(struct type));
	type->kind = type_kind_pointer;
	type->inner = pointee;
	type->next = first_type;
	first_type = type;
	return type;
}

static struct type *type_from_expr(struct node *expr) {
	switch (expr->kind) {
	case node_kind_name:
		if (strcmp(expr->name, "int") == 0) return type_int();
		error(expr->line, "unknown type “%s”", expr->name);

	case node_kind_address:
		return type_pointer(type_from_expr(expr->kids->next));

	case node_kind_nil:
		return 0;

	default:
		error(expr->line, "cannot use non-type expression as type");
	}
}

static char *type_print(struct type *type) {
	if (!type) return "non-value";

	char *result = 0;
	switch (type->kind) {
	case type_kind_int:
		result = "int";
		break;
	case type_kind_pointer:
		asprintf(&result, "*%s", type_print(type->inner));
		break;
	}
	return result;
}

static void expect_types_equal(struct type *expected, struct type *actual, size_t line) {
	if (expected == actual) return;
	error(line, "expected “%s” but found “%s”", type_print(expected), type_print(actual));
}

static struct node *current_root;
static struct entity *g_first_entity;

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
		struct node *type_expr = node_find(node, node_kind_type)->kids->next;
		struct node *initializer = node_find(node, node_kind_initializer);

		struct type *type = 0;
		if (node_is_nil(initializer)) {
			type = type_from_expr(type_expr);
		} else {
			struct node *initializer_expr = initializer->kids->next;
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
		break;
	}

	case node_kind_name:
		for (struct local *local = proc->first_local; local; local = local->next) {
			if (strcmp(local->name, node->name) == 0) {
				node->local = local;
				break;
			}
		}

		if (!node->local) {
			error(node->line, "unknown variable “%s”", node->name);
		}

		node->type = node->local->type;
		break;

	case node_kind_assign: {
		struct node *lhs = node->kids->next;
		struct node *rhs = lhs->next;
		check_node(proc, lhs);
		check_node(proc, rhs);
		expect_types_equal(lhs->type, rhs->type, rhs->line);
		break;
	}

	case node_kind_expr_stmt: {
		struct node *expr = node->kids->next;
		check_node(proc, expr);
		if (expr->type) error(expr->line, "unused expression");
		break;
	}

	case node_kind_return: {
		struct node *return_value = node->kids->next;
		if (node_is_nil(return_value)) {
			if (proc->return_type) {
				error(node->line, "missing return value");
			}
		} else {
			if (!proc->return_type) {
				error(node->line, "cannot return value from procedure with no return value");
			}
			check_node(proc, return_value);
			expect_types_equal(proc->return_type, return_value->type, return_value->line);
		}
		break;
	}

	case node_kind_address: {
		struct node *operand = node->kids->next;
		check_node(proc, operand);
		node->type = type_pointer(operand->type);
		break;
	}

	case node_kind_deref: {
		struct node *operand = node->kids->next;
		check_node(proc, operand);
		if (operand->type->kind != type_kind_pointer) {
			error(node->line, "can’t dereference non-pointer type “%s”", type_print(operand->type));
		}
		node->type = operand->type->inner;
		break;
	}

	case node_kind_call: {
		char *name = node->kids->next->name;
		struct entity *match = 0;

		for (struct entity *candidate = g_first_entity; candidate; candidate = candidate->next) {
			if (candidate->kind == entity_kind_proc && strcmp(candidate->name, name) == 0) {
				match = candidate;
				break;
			}
		}

		if (!match) {
			error(node->line, "unknown procedure “%s”", name);
		}

		size_t arg_count = node_kid_count(node) - 1;
		size_t param_count = match->param_count;
		if (arg_count != param_count) {
			error(node->line, "expected %zu arguments but found %zu", param_count, arg_count);
		}

		struct node *arg = node->kids->next->next;
		struct param *param = match->first_param;
		while (!node_is_nil(arg) && param) {
			check_node(proc, arg);
			expect_types_equal(param->type, arg->type, arg->line);
			arg = arg->next;
			param = param->next;
		}

		node->type = match->return_type;
		break;
	}

	case node_kind_add:
	case node_kind_sub:
	case node_kind_mul:
	case node_kind_div: {
		struct node *lhs = node->kids->next;
		struct node *rhs = lhs->next;
		check_node(proc, lhs);
		check_node(proc, rhs);
		expect_types_equal(type_int(), lhs->type, lhs->line);
		expect_types_equal(type_int(), rhs->type, rhs->line);
		node->type = type_int();
		break;
	}

	case node_kind_block:
		for (struct node *kid = node->kids->next; !node_is_nil(kid); kid = kid->next) {
			check_node(proc, kid);
		}
		break;

	case node_kind_nil:
	case node_kind_root:
	case node_kind_proc:
	case node_kind_param:
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

	for (struct node *node = root->kids->next; !node_is_nil(node); node = node->next) {
		assert(node->kind == node_kind_proc);

		struct entity *entity = calloc(1, sizeof(struct entity));
		entity->kind = entity_kind_proc;
		entity->name = node->name;
		entity->node = node;

		struct param *first_param = 0;
		struct param *last_param = 0;

		for (struct node *kid = node->kids->next; !node_is_nil(kid); kid = kid->next) {
			if (kid->kind == node_kind_param) {
				struct param *param = calloc(1, sizeof(struct param));
				param->name = kid->name;
				param->type = type_from_expr(kid->kids->next);
				param->local = add_local(entity, param->name, param->type);

				if (first_param) {
					last_param->next = param;
				} else {
					first_param = param;
				}
				last_param = param;
				entity->param_count++;
			}
		}

		entity->first_param = first_param;
		entity->body = node_find(node, node_kind_block);

		struct node *return_type = node_find(node, node_kind_type);
		if (return_type) {
			entity->return_type = type_from_expr(return_type->kids->next);
		}

		if (g_first_entity) {
			last_entity->next = entity;
		} else {
			g_first_entity = entity;
		}
		last_entity = entity;
	}

	for (struct entity *entity = g_first_entity; entity; entity = entity->next) {
		assert(entity->kind == entity_kind_proc);
		check_node(entity, entity->body);
	}

	return g_first_entity;
}
