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

	default:
		error(expr->line, "cannot use non-type expression as type");
	}
}

static char *type_print(struct type *type) {
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

static void check_node(struct node *proc, struct node *node) {
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
			if (node_is_nil(type_expr)) {
				type = initializer_expr->type;
			} else {
				type = type_from_expr(type_expr);
				expect_types_equal(type, initializer_expr->type, initializer->line);
			}
		}

		struct local *local = calloc(1, sizeof(struct local));
		local->next = proc->local;
		local->name = node->name;
		local->type = type;
		local->offset = 8;
		if (proc->local) local->offset += proc->local->offset;

		proc->local = local;
		proc->locals_size += 8;

		node->local = local;
		break;
	}

	case node_kind_name:
		for (struct local *local = proc->local; local; local = local->next) {
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

	case node_kind_return: {
		struct node *return_value = node->kids->next;
		check_node(proc, return_value);
		expect_types_equal(proc->type, return_value->type, return_value->line);
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
		char *name = node_find(node, node_kind_name)->name;
		struct node *match = 0;

		for (struct node *candidate = current_root->kids->next; !node_is_nil(candidate);
		        candidate = candidate->next) {
			if (strcmp(candidate->name, name) == 0) {
				match = candidate;
				break;
			}
		}

		if (!match) {
			error(node->line, "unknown procedure “%s”", name);
		}

		node->type = match->type;
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
	case node_kind_initializer:
	case node_kind_type:
	case node_kind__last:
		unreachable();
	}
}

static void typecheck(struct node *root) {
	current_root = root;

	for (struct node *proc = root->kids->next; !node_is_nil(proc); proc = proc->next) {
		assert(proc->kind == node_kind_proc);
		proc->type = type_from_expr(node_find(proc, node_kind_type)->kids->next);
	}

	for (struct node *proc = root->kids->next; !node_is_nil(proc); proc = proc->next) {
		assert(proc->kind == node_kind_proc);
		struct node *body = node_find(proc, node_kind_block);
		check_node(proc, body);
	}
}
