typedef struct Span Span;
struct Span
{
	smm start;
	smm length;
};

function Span SpanUnion(Span a, Span b);

typedef enum ValueKind
{
	ValueKind_ConstantInt32,
	ValueKind_AddInt32,
	ValueKind__Count,
} ValueKind;

global read_only String value_kind_names[] = {S("ConstantInt32"), S("AddInt32")};

typedef enum ValueArgumentKind
{
	ValueArgumentKind_Value,
	ValueArgumentKind_Constant,
	ValueArgumentKind__Count,
} ValueArgumentKind;

global read_only String value_argument_kind_names[] = {S("value"), S("constant")};

typedef struct ValueSignature ValueSignature;
struct ValueSignature
{
	smm argument_count;
	ValueArgumentKind argument_kinds[2];
};

function ValueSignature ValueSignatureFromValueKind(ValueKind kind);
global read_only ValueSignature value_signatures[] = {
        {1, {ValueArgumentKind_Constant}},
        {2, {ValueArgumentKind_Value, ValueArgumentKind_Value}},
};

typedef struct Value Value;
typedef struct ValueArgument ValueArgument;
struct ValueArgument
{
	ValueArgument *next;
	Value *value;
	u64 constant;
	Span span;
};

struct Value
{
	Value *next;
	ValueArgument *first_argument;
	ValueArgument *last_argument;
	smm argument_count;
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
