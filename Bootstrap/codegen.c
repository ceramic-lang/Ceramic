static void codegen_node_address(struct entity *proc, struct node *node, FILE *file);

static void codegen_node(struct entity *proc, struct node *node, FILE *file) {
	switch (node->kind) {
	case node_kind_name:
		if (node->local) {
			fprintf(file, "\tldr x9, [x29, #-%zu]\n", node->local->offset);
		} else {
			fprintf(file, "\tadrp x9, _%s@PAGE\n", node->entity->name);
			fprintf(file, "\tadd x9, x9, _%s@PAGEOFF\n", node->entity->name);
		}
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

	case node_kind_call: {
		int arg_count = 0;
		for (struct node *arg = node->kids->next->next; !node_is_nil(arg); arg = arg->next) {
			codegen_node(proc, arg, file);
			fprintf(file, "\tstr x9, [sp, #-16]!\n");
			arg_count++;
		}

		for (int reg = arg_count - 1; reg >= 0; reg--) {
			fprintf(file, "\tldr x%d, [sp], #16\n", reg);
		}

		struct node *callee = node->kids->next;
		codegen_node(proc, callee, file);
		fprintf(file, "\tblr x9\n");
		break;
	}

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
		if (node_is_nil(initializer)) {
			fprintf(file, "\tmov x9, #0\n");
		} else {
			codegen_node(proc, initializer->kids->next, file);
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

	case node_kind_expr_stmt: {
		struct node *expr = node->kids->next;
		codegen_node(proc, expr, file);
		break;
	}

	case node_kind_return: {
		struct node *return_value = node->kids->next;
		if (!node_is_nil(return_value)) {
			codegen_node(proc, return_value, file);
			fprintf(file, "\tmov x0, x9\n");
		}
		fprintf(file, "\tb .L.%s.return\n", proc->name);
		break;
	}

	case node_kind_block:
		for (struct node *kid = node->kids->next; !node_is_nil(kid); kid = kid->next) {
			codegen_node(proc, kid, file);
		}
		break;

	case node_kind_nil:
	case node_kind_root:
	case node_kind_proc:
	case node_kind_proc_type:
	case node_kind_param:
	case node_kind_params:
	case node_kind_return_type:
	case node_kind_initializer:
	case node_kind_type:
	case node_kind__last:
		unreachable();
	}
}

static void codegen_node_address(struct entity *proc, struct node *node, FILE *file) {
	switch (node->kind) {
	case node_kind_name:
		if (node->local) {
			fprintf(file, "\tsub x9, x29, #%zu\n", node->local->offset);
		} else {
			error(node->line, "cannot take address of procedure");
		}
		break;

	case node_kind_deref:
		codegen_node(proc, node->kids->next, file);
		break;

	default:
		error(node->line, "expression doesn’t have an address");
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

static void codegen(struct entity *first_entity, FILE *file) {
	for (struct entity *proc = first_entity; proc; proc = proc->next) {
		assert(proc->kind == entity_kind_proc);
		fprintf(file, ".global _%s\n", proc->name);
		fprintf(file, ".align 2\n");
		fprintf(file, "_%s:\n", proc->name);

		size_t locals_size = round_up(proc->locals_size, 16);
		fprintf(file, "\tstp x29, x30, [sp, #-16]!\n");
		fprintf(file, "\tmov x29, sp\n");
		fprintf(file, "\tsub sp, sp, #%zu\n", locals_size);

		size_t reg = 0;
		for (struct param *param = proc->first_param; param; param = param->next) {
			fprintf(file, "\tstr x%zu, [x29, #-%zu]\n", reg, param->local->offset);
			reg++;
		}

		codegen_node(proc, proc->body, file);

		fprintf(file, ".L.%s.return:\n", proc->name);
		fprintf(file, "\tadd sp, sp, #%zu\n", locals_size);
		fprintf(file, "\tldp x29, x30, [sp], #16\n");
		fprintf(file, "\tret\n");
	}
}
