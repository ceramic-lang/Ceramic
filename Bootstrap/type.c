static void add_local(struct node *proc, struct node *node) {
	struct local *local = calloc(1, sizeof(struct local));
	local->next = proc->local;
	local->name = node->name;
	local->offset = 8;
	if (proc->local) local->offset += proc->local->offset;
	proc->local = local;
	proc->locals_size += 8;
	node->local = local;
}

static void check_node(struct node *proc, struct node *node) {
	switch (node->kind) {
	case node_kind_local: {
		struct node *initializer = node_find(node, node_kind_initializer);
		if (initializer) check_node(proc, initializer);
		add_local(proc, node);
		break;
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
		break;
	}

	default:
		for (struct node *kid = node->kids->next; kid != node->kids; kid = kid->next) {
			check_node(proc, kid);
		}
		break;
	}
}

static void typecheck(struct node *root) {
	for (struct node *proc = root->kids->next; proc != root->kids; proc = proc->next) {
		assert(proc->kind == node_kind_proc);
		struct node *body = node_find(proc, node_kind_block);
		check_node(proc, body);
	}
}
