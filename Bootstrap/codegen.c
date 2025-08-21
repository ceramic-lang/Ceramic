static void codegen_node_address(struct node *proc, struct node *node, FILE *file);

static void codegen_node(struct node *proc, struct node *node, FILE *file) {
	switch (node->kind) {
	case node_kind_name:
		fprintf(file, "\tldr x9, [x29, #-%zu]\n", node->local->offset);
		break;

	case node_kind_number:
		fprintf(file, "\tmov x9, #%llu\n", node->value);
		break;

	case node_kind_address:
		codegen_node_address(proc, node->kids->next, file);
		break;

	case node_kind_deref:
		codegen_node(proc, node->kids->next, file);
		fprintf(file, "\tldr x9, [x9]\n");
		break;

	case node_kind_add:
	case node_kind_sub:
	case node_kind_mul:
	case node_kind_div:
		codegen_node(proc, node->kids->next, file);
		fprintf(file, "\tstr x9, [sp, #-16]!\n");
		codegen_node(proc, node->kids->next->next, file);
		fprintf(file, "\tldr x10, [sp], #16\n");
		switch (node->kind) {
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

	case node_kind_local: {
		struct node *initializer = node_find(node, node_kind_initializer);
		if (initializer) {
			codegen_node(proc, initializer->kids->next, file);
		} else {
			fprintf(file, "\tmov x9, #0\n");
		}
		fprintf(file, "\tstr x9, [x29, #-%zu]\n", node->local->offset);
		break;
	}

	case node_kind_assign: {
		struct node *lhs = node->kids->next;
		struct node *rhs = lhs->next;
		codegen_node_address(proc, lhs, file);
		fprintf(file, "\tstr x9, [sp, #-16]!\n");
		codegen_node(proc, rhs, file);
		fprintf(file, "\tldr x10, [sp], #16\n");
		fprintf(file, "\tstr x9, [x10]\n");
		break;
	}

	case node_kind_return:
		codegen_node(proc, node->kids->next, file);
		fprintf(file, "\tmov x0, x9\n");
		fprintf(file, "\tb .L.%s.return\n", proc->name);
		break;

	case node_kind_block:
		for (struct node *kid = node->kids->next; kid != node->kids; kid = kid->next) {
			codegen_node(proc, kid, file);
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

static void codegen_node_address(struct node *proc, struct node *node, FILE *file) {
	switch (node->kind) {
	case node_kind_name:
		fprintf(file, "\tsub x9, x29, #%zu\n", node->local->offset);
		break;

	case node_kind_deref:
		codegen_node(proc, node->kids->next, file);
		break;

	default:
		printf("%zu: expression doesn’t have an address\n", node->line);
		exit(1);
	}
}

static size_t round_up(size_t n, size_t m) {
	size_t remainder = n % m;
	if (remainder == 0) return n;
	size_t result = n + m - remainder;
	assert(result % m == 0);
	assert(result > n);
	return result;
}

static void codegen(struct node *root, FILE *file) {
	assert(root->kind == node_kind_root);

	for (struct node *proc = root->kids->next; proc != root->kids; proc = proc->next) {
		assert(proc->kind == node_kind_proc);
		fprintf(file, ".global _%s\n", proc->name);
		fprintf(file, ".align 2\n");
		fprintf(file, "_%s:\n", proc->name);

		size_t locals_size = round_up(proc->locals_size, 16);
		fprintf(file, "\tstp x29, x30, [sp, #-16]!\n");
		fprintf(file, "\tmov x29, sp\n");
		fprintf(file, "\tsub sp, sp, #%zu\n", locals_size);

		struct node *body = node_find(proc, node_kind_block);
		codegen_node(proc, body, file);

		fprintf(file, ".L.%s.return:\n", proc->name);
		fprintf(file, "\tadd sp, sp, #%zu\n", locals_size);
		fprintf(file, "\tldp x29, x30, [sp], #16\n");
		fprintf(file, "\tret\n");
	}
}
