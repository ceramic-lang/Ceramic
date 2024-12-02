typedef enum ValueKind
{
	ValueKind_ConstantInt32,
	ValueKind_AddInt32,
	ValueKind__Count,
} ValueKind;

global read_only String value_kind_names[] = {S("ConstantInt32"), S("AddInt32")};

typedef struct Value Value;
struct Value
{
	Value *next;
	Value *lhs;
	Value *rhs;
	u64 constant;
	ValueKind kind;
};

typedef struct Function Function;
struct Function
{
	Function *next;
	String name;
	Value *first_value;
	Value *last_value;
	smm value_count;
};

typedef struct IR IR;
struct IR
{
	Function *first_function;
	Function *last_function;
	smm function_count;
};

typedef struct Span Span;
struct Span
{
	smm start;
	smm length;
};

typedef enum DiagnosticSeverity
{
	DiagnosticSeverity_Error,
	DiagnosticSeverity__Count,
} DiagnosticSeverity;

typedef struct Diagnostic Diagnostic;
struct Diagnostic
{
	Diagnostic *next;
	DiagnosticSeverity severity;
	Span span;
	String message;
};

typedef struct DiagnosticList DiagnosticList;
struct DiagnosticList
{
	Diagnostic *first;
	Diagnostic *last;
	smm count;
};

function void DiagnosticListPush(DiagnosticList *list, Diagnostic *diagnostic);
function String DiagnosticSeverityPrint(DiagnosticSeverity severity);
function String DiagnosticListPrint(DiagnosticList *diagnostics);

function IR *IRParse(String string, DiagnosticList *diagnostics);
function String IRPrint(IR *ir);

function void IRTests(void);
