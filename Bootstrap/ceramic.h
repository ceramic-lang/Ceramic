enum node_kind {
	node_kind_nil,
	node_kind_root,
	node_kind_proc,
	node_kind_proc_type,
	node_kind_param,
	node_kind_params,
	node_kind_local,
	node_kind_type,
	node_kind_initializer,
	node_kind_assign,
	node_kind_expr_stmt,
	node_kind_return,
	node_kind_block,
	node_kind_address,
	node_kind_deref,
	node_kind_call,
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
        [node_kind_proc_type] = "proc_type",
        [node_kind_param] = "param",
        [node_kind_params] = "params",
        [node_kind_local] = "local",
        [node_kind_type] = "type",
        [node_kind_initializer] = "initializer",
        [node_kind_assign] = "assign",
        [node_kind_expr_stmt] = "expr_stmt",
        [node_kind_return] = "return",
        [node_kind_block] = "block",
        [node_kind_address] = "address",
        [node_kind_deref] = "deref",
        [node_kind_call] = "call",
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
	type_kind_proc,
};

struct type_node {
	struct type_node *next;
	struct type *type;
};

struct type {
	struct type *next_interned;
	enum type_kind kind;
	struct type *inner;
	struct type_node *first;
};

struct local {
	struct local *next;
	char *name;
	struct type *type;
	size_t offset;
};

struct node {
	struct node *next;
	struct node *first;
	struct node *last;
	enum node_kind kind;
	char *name;
	uint64_t value;
	size_t line;

	struct type *type;
	struct local *local;
	struct entity *entity;
};

static const struct node node_nil = {
        .next = (struct node *)&node_nil,
        .first = (struct node *)&node_nil,
        .last = (struct node *)&node_nil,
};

static bool node_is_nil(struct node *node);
static struct node *node_find(struct node *node, enum node_kind kind);
static size_t node_kid_count(struct node *node);
__attribute__((unused)) static void node_print(struct node *node);

static struct node *parse(char *s);

struct param {
	struct param *next;
	char *name;
	struct type *type;
	struct local *local;
};

enum entity_kind {
	entity_kind_proc,
};

struct entity {
	struct entity *next;
	enum entity_kind kind;
	char *name;
	struct node *node;

	struct param *first_param;
	size_t param_count;
	struct type *type;
	struct node *body;
	struct local *first_local;
	size_t locals_size;
};

static void type_list_push(struct type_node **first, struct type_node **last, struct type_node *node);
static struct entity *typecheck(struct node *root);

static void codegen(struct entity *first_entity, FILE *file);
