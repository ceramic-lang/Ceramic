enum node_kind {
	node_kind_nil,
	node_kind_root,
	node_kind_proc,
	node_kind_local,
	node_kind_type,
	node_kind_initializer,
	node_kind_assign,
	node_kind_return,
	node_kind_block,
	node_kind_address,
	node_kind_deref,
	node_kind_name,
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
		[node_kind_local] = "local",
		[node_kind_type] = "type",
		[node_kind_initializer] = "initializer",
		[node_kind_assign] = "assign",
		[node_kind_return] = "return",
		[node_kind_block] = "block",
		[node_kind_address] = "address",
		[node_kind_deref] = "deref",
		[node_kind_name] = "name",
		[node_kind_number] = "number",
		[node_kind_add] = "add",
		[node_kind_sub] = "sub",
		[node_kind_mul] = "mul",
		[node_kind_div] = "div",
};

enum type_kind {
	type_kind_int,
	type_kind_pointer,
};

struct type {
	struct type *next;
	enum type_kind kind;
	struct type *inner;
};

struct local {
	struct local *next;
	char *name;
	struct type *type;
	size_t offset;
};

struct node {
	struct node *next;
	struct node *prev;
	struct node *kids;
	enum node_kind kind;
	char *name;
	uint64_t value;
	size_t line;

	struct local *local;
	size_t locals_size;
	struct type *return_type;
};

__attribute__((unused)) static void node_print(struct node *node);

static struct node *parse(char *s);
static void typecheck(struct node *root);

static void codegen(struct node *root, FILE *file);
