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
		printf("%zu: unknown type “%s”\n", expr->line, expr->name);
		exit(1);

	case node_kind_address:
		return type_pointer(type_from_expr(expr->kids->next));

	default:
		printf("%zu: cannot use non-type expression as type\n", expr->line);
		exit(1);
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
	printf("%zu: expected “%s” but found “%s”\n", line, type_print(expected), type_print(actual));
	exit(1);
}

static struct node *current_root;

static struct type *check_node(struct node *proc, struct node *node) {
	switch (node->kind) {
	case node_kind_number:
		return type_int();

	case node_kind_local: {
		struct node *type_expr = node_find(node, node_kind_type)->kids->next;
		struct type *type = type_from_expr(type_expr);

		struct node *initializer = node_find(node, node_kind_initializer);
		if (!node_is_nil(initializer)) {
			struct type *initializer_type = check_node(proc, initializer->kids->next);
			expect_types_equal(type, initializer_type, initializer->line);
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
		return local->type;
	}

	case node_kind_name: {
		struct local *match = 0;
		for (struct local *local = proc->local; local; local = local->next) {
			if (strcmp(local->name, node->name) == 0) {
				match = local;
			}
		}

		if (!match) {
			printf("%zu: unknown variable “%s”\n", node->line, node->name);
			exit(1);
		}

		node->local = match;
		return match->type;
	}

	case node_kind_assign: {
		struct node *lhs = node->kids->next;
		struct node *rhs = lhs->next;
		struct type *lhs_type = check_node(proc, lhs);
		struct type *rhs_type = check_node(proc, rhs);
		expect_types_equal(lhs_type, rhs_type, rhs->line);
		return 0;
	}

	case node_kind_return: {
		struct node *return_value = node->kids->next;
		struct type *return_value_type = check_node(proc, return_value);
		expect_types_equal(proc->return_type, return_value_type, return_value->line);
		return 0;
	}

	case node_kind_address: {
		struct type *pointee = check_node(proc, node->kids->next);
		return type_pointer(pointee);
	}

	case node_kind_deref: {
		struct type *type = check_node(proc, node->kids->next);
		if (type->kind != type_kind_pointer) {
			printf("%zu: can’t dereference non-pointer type “%s”\n", node->line, type_print(type));
			exit(1);
		}
		return type->inner;
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
			printf("%zu: unknown procedure “%s”\n", node->line, name);
			exit(1);
		}

		return match->return_type;
	}

	case node_kind_add:
	case node_kind_sub:
	case node_kind_mul:
	case node_kind_div: {
		struct node *lhs = node->kids->next;
		struct node *rhs = lhs->next;
		expect_types_equal(type_int(), check_node(proc, lhs), lhs->line);
		expect_types_equal(type_int(), check_node(proc, rhs), rhs->line);
		return type_int();
	}

	case node_kind_block:
		for (struct node *kid = node->kids->next; !node_is_nil(kid); kid = kid->next) {
			check_node(proc, kid);
		}
		return 0;

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
		proc->return_type = type_from_expr(node_find(proc, node_kind_type)->kids->next);
	}

	for (struct node *proc = root->kids->next; !node_is_nil(proc); proc = proc->next) {
		assert(proc->kind == node_kind_proc);
		struct node *body = node_find(proc, node_kind_block);
		check_node(proc, body);
	}
}
