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
