function Span
SpanUnion(Span a, Span b)
{
	Span result = {0};
	smm a_end = a.start + a.length;
	smm b_end = b.start + b.length;
	result.start = a.start < b.start ? a.start : b.start;
	smm result_end = a_end > b_end ? a_end : b_end;
	result.length = result_end - result.start;
	return result;
}

function ValueSignature
ValueSignatureFromValueKind(ValueKind kind)
{
	ValueSignature result = {0};

	switch (kind)
	{
		case ValueKind_ConstantInt32:
		{
			result.argument_count = 1;
			result.argument_kinds[0] = ValueArgumentKind_Constant;
			break;
		}

		default:
		{
			result.argument_count = 2;
			result.argument_kinds[0] = ValueArgumentKind_Value;
			result.argument_kinds[1] = ValueArgumentKind_Value;
		}
	}

	return result;
}

typedef enum IRTokenKind
{
	IRTokenKind_Eof,
	IRTokenKind_Error,
	IRTokenKind_FuncKeyword,
	IRTokenKind_Identifier,
	IRTokenKind_Number,
	IRTokenKind_Equal,
	IRTokenKind_Comma,
	IRTokenKind_LBrace,
	IRTokenKind_RBrace,
	IRTokenKind__Count,
} IRTokenKind;

typedef struct IRKeyword IRKeyword;
struct IRKeyword
{
	String string;
	IRTokenKind kind;
};

global read_only IRKeyword ir_keywords[] = {
        {S("func"), IRTokenKind_FuncKeyword},
};

typedef struct IRToken IRToken;
struct IRToken
{
	IRToken *next;
	Span span;
	IRTokenKind kind;
};

typedef struct IRTokenList IRTokenList;
struct IRTokenList
{
	IRToken *first;
	IRToken *last;
	smm count;
};

function String
IRTokenKindName(IRTokenKind kind)
{
	local_persist read_only String names[] = {S("end of file"), S("unrecognized token"), S("“func”"), S("identifier"),
	        S("number"), S("“=”"), S("“,”"), S("“{”"), S("“}”")};

	Assert(ArrayCount(names) == IRTokenKind__Count);

	String result = S("<unknown token kind>");
	if (kind >= IRTokenKind_Eof && kind < IRTokenKind__Count)
	{
		result = names[kind];
	}
	return result;
}

function void
DiagnosticListPush(DiagnosticList *list, Diagnostic *diagnostic)
{
	if (list->first == 0)
	{
		list->first = diagnostic;
	}
	else
	{
		list->last->next = diagnostic;
	}
	list->last = diagnostic;
	list->count++;
}

function String
DiagnosticSeverityPrint(DiagnosticSeverity severity)
{
	switch (severity)
	{
		case DiagnosticSeverity_Error: return S("error");
		default: return S("<unknown diagnostic severity>");
	}
}

function String
DiagnosticListPrint(DiagnosticList *diagnostics)
{
	StringList list = {0};

	for (Diagnostic *diagnostic = diagnostics->first; diagnostic != 0; diagnostic = diagnostic->next)
	{
		StringListPushF(&list, "%.*s at %td..%td: %.*s\n", SF(DiagnosticSeverityPrint(diagnostic->severity)),
		        diagnostic->span.start, diagnostic->span.start + diagnostic->span.length, SF(diagnostic->message));
	}

	return StringListJoin(list);
}

function void
IRTokenListPush(IRTokenList *list, IRToken *token)
{
	if (list->first == 0)
	{
		list->first = token;
	}
	else
	{
		list->last->next = token;
	}
	list->last = token;
	list->count++;
}

function IRTokenList
IRTokenize(String string, DiagnosticList *diagnostics)
{
	IRTokenList result = {0};
	smm cursor = 0;

	for (; cursor < string.length;)
	{
		if (IsWhitespace(string.data[cursor]))
		{
			for (; cursor < string.length && IsWhitespace(string.data[cursor]);)
			{
				cursor++;
			}
			continue;
		}

		if (IsAlphabetic(string.data[cursor]))
		{
			IRToken *token = calloc(1, (umm)size_of(IRToken));
			token->kind = IRTokenKind_Identifier;
			token->span.start = cursor;
			for (; cursor < string.length && IsAlphanumeric(string.data[cursor]);)
			{
				cursor++;
			}
			token->span.length = cursor - token->span.start;

			String token_string = StringSlice(string, token->span.start, token->span.length);
			for (smm keyword_index = 0; keyword_index < ArrayCount(ir_keywords); keyword_index++)
			{
				IRKeyword *keyword = ir_keywords + keyword_index;
				if (StringEqual(token_string, keyword->string))
				{
					token->kind = keyword->kind;
					break;
				}
			}

			IRTokenListPush(&result, token);
			continue;
		}

		if (IsDigit(string.data[cursor]))
		{
			IRToken *token = calloc(1, (umm)size_of(IRToken));
			token->kind = IRTokenKind_Number;
			token->span.start = cursor;
			for (; cursor < string.length && IsDigit(string.data[cursor]);)
			{
				cursor++;
			}
			token->span.length = cursor - token->span.start;
			IRTokenListPush(&result, token);
			continue;
		}

		if (string.data[cursor] == '=')
		{
			IRToken *token = calloc(1, (umm)size_of(IRToken));
			token->kind = IRTokenKind_Equal;
			token->span.start = cursor;
			token->span.length = 1;
			cursor++;
			IRTokenListPush(&result, token);
			continue;
		}

		if (string.data[cursor] == ',')
		{
			IRToken *token = calloc(1, (umm)size_of(IRToken));
			token->kind = IRTokenKind_Comma;
			token->span.start = cursor;
			token->span.length = 1;
			cursor++;
			IRTokenListPush(&result, token);
			continue;
		}

		if (string.data[cursor] == '{')
		{
			IRToken *token = calloc(1, (umm)size_of(IRToken));
			token->kind = IRTokenKind_LBrace;
			token->span.start = cursor;
			token->span.length = 1;
			cursor++;
			IRTokenListPush(&result, token);
			continue;
		}

		if (string.data[cursor] == '}')
		{
			IRToken *token = calloc(1, (umm)size_of(IRToken));
			token->kind = IRTokenKind_RBrace;
			token->span.start = cursor;
			token->span.length = 1;
			cursor++;
			IRTokenListPush(&result, token);
			continue;
		}

		Diagnostic *diagnostic = calloc(1, (umm)size_of(Diagnostic));
		diagnostic->severity = DiagnosticSeverity_Error;
		diagnostic->message = S("unrecognized token");
		diagnostic->span.start = cursor;
		cursor++;
		diagnostic->span.length = cursor - diagnostic->span.start;
		DiagnosticListPush(diagnostics, diagnostic);
	}

	return result;
}

typedef struct ValueNameNode ValueNameNode;
struct ValueNameNode
{
	ValueNameNode *next;
	String name;
	Value *value;
};

typedef struct ValueNameList ValueNameList;
struct ValueNameList
{
	ValueNameNode *first;
	ValueNameNode *last;
};

typedef struct IRParser IRParser;
struct IRParser
{
	String string;
	IRToken *current;
	IRToken *previous;
	smm check_count;
	DiagnosticList *diagnostics;

	ValueNameList value_names;
};

function IRTokenKind
IRParserCurrent(IRParser *parser)
{
	IRTokenKind result = IRTokenKind_Eof;
	parser->check_count++;
	Assert(parser->check_count < 128);
	if (parser->current != 0)
	{
		result = parser->current->kind;
	}
	return result;
}

function Span
IRParserCurrentSpan(IRParser *parser)
{
	Span result = {0};
	if (parser->current == 0)
	{
		result.start = parser->string.length;
	}
	else
	{
		result = parser->current->span;
	}
	return result;
}

function b32
IRParserAt(IRParser *parser, IRTokenKind kind)
{
	return IRParserCurrent(parser) == kind;
}

function b32
IRParserAtEof(IRParser *parser)
{
	return IRParserAt(parser, IRTokenKind_Eof);
}

function String
IRParserBumpAny(IRParser *parser)
{
	String result = {0};
	if (!IRParserAtEof(parser))
	{
		parser->check_count = 0;
		Span span = parser->current->span;
		parser->previous = parser->current;
		parser->current = parser->current->next;
		result = StringSlice(parser->string, span.start, span.length);
	}
	return result;
}

function String
IRParserBump(IRParser *parser, IRTokenKind kind)
{
	Assert(IRParserAt(parser, kind));
	return IRParserBumpAny(parser);
}

function b32
IRParserEat(IRParser *parser, IRTokenKind kind, String *string)
{
	b32 result = IRParserAt(parser, kind);

	if (string != 0)
	{
		MemoryZeroStruct(string);
	}

	if (result)
	{
		String s = IRParserBump(parser, kind);
		if (string != 0)
		{
			*string = s;
		}
	}

	return result;
}

function void
IRParserErrorWithSpan(IRParser *parser, Span span, String message)
{
	Diagnostic *diagnostic = calloc(1, (umm)size_of(Diagnostic));
	diagnostic->severity = DiagnosticSeverity_Error;
	diagnostic->span = span;
	diagnostic->message = message;
	DiagnosticListPush(parser->diagnostics, diagnostic);
}

function void
IRParserError(IRParser *parser, String message)
{
	Span span = IRParserCurrentSpan(parser);
	IRParserErrorWithSpan(parser, span, message);
}

function void
IRParserErrorPrevious(IRParser *parser, String message)
{
	Span span = parser->previous->span;
	IRParserErrorWithSpan(parser, span, message);
}

function String
IRParserExpect(IRParser *parser, IRTokenKind kind)
{
	String result = {0};
	if (!IRParserEat(parser, kind, &result))
	{
		if (IRParserAtEof(parser))
		{
			IRParserError(parser, PushStringF("missing %.*s", SF(IRTokenKindName(kind))));
		}
		else
		{
			IRParserError(parser, PushStringF("expected %.*s but found %.*s", SF(IRTokenKindName(kind)),
			                              SF(IRTokenKindName(IRParserCurrent(parser)))));
		}
		IRParserBumpAny(parser);
	}
	return result;
}

function Value *
IRParserResolveValue(IRParser *parser, String name)
{
	Value *result = 0;

	for (ValueNameNode *node = parser->value_names.first; node != 0; node = node->next)
	{
		if (StringEqual(node->name, name))
		{
			result = node->value;
			break;
		}
	}

	if (name.data != 0 && result == 0)
	{
		IRParserErrorPrevious(parser, PushStringF("unknown value “%.*s”", SF(name)));
	}

	return result;
}

function IR *
IRParse(String string, DiagnosticList *diagnostics)
{
	IR *result = calloc(1, (umm)size_of(IR));
	IRTokenList tokens = IRTokenize(string, diagnostics);
	IRParser parser = {0};
	parser.current = tokens.first;
	parser.diagnostics = diagnostics;
	parser.string = string;

	for (; !IRParserAtEof(&parser);)
	{
		if (IRParserAt(&parser, IRTokenKind_FuncKeyword))
		{
			IRParserBump(&parser, IRTokenKind_FuncKeyword);

			String function_name = IRParserExpect(&parser, IRTokenKind_Identifier);
			if (function_name.data == 0)
			{
				continue;
			}

			Function *func = calloc(1, (umm)size_of(Function));
			func->name = function_name;

			if (result->first_function == 0)
			{
				result->first_function = func;
			}
			else
			{
				result->last_function->next = func;
			}
			result->last_function = func;
			result->function_count++;

			IRParserExpect(&parser, IRTokenKind_LBrace);

			MemoryZeroStruct(&parser.value_names);

			for (; !IRParserAtEof(&parser) && !IRParserAt(&parser, IRTokenKind_RBrace);)
			{
				String value_name = IRParserExpect(&parser, IRTokenKind_Identifier);
				if (value_name.data == 0)
				{
					continue;
				}

				IRParserExpect(&parser, IRTokenKind_Equal);

				String value_kind_name = IRParserExpect(&parser, IRTokenKind_Identifier);
				ValueKind value_kind = (ValueKind)-1;
				for (ValueKind kind = 0; kind < ValueKind__Count; kind++)
				{
					if (StringEqual(value_kind_names[kind], value_kind_name))
					{
						value_kind = kind;
						break;
					}
				}

				if (value_kind_name.data != 0 && value_kind == (ValueKind)-1)
				{
					IRParserErrorPrevious(&parser, PushStringF("unknown value kind “%.*s”", SF(value_kind_name)));
				}

				ValueArgument *first_argument = 0;
				ValueArgument *last_argument = 0;
				smm argument_count = 0;
				b32 failed_to_resolve_any = 0;

				Span argument_start_span = IRParserCurrentSpan(&parser);

				for (;;)
				{
					if (IRParserAtEof(&parser) || (first_argument != 0 && !IRParserEat(&parser, IRTokenKind_Comma, 0)))
					{
						break;
					}

					ValueArgument *argument = 0;
					switch (IRParserCurrent(&parser))
					{
						case IRTokenKind_Identifier:
						{
							argument = calloc(1, (umm)size_of(ValueArgument));
							argument->span = IRParserCurrentSpan(&parser);
							String name = IRParserBump(&parser, IRTokenKind_Identifier);
							argument->value = IRParserResolveValue(&parser, name);
							failed_to_resolve_any |= argument->value == 0;
							break;
						}

						case IRTokenKind_Number:
						{
							argument = calloc(1, (umm)size_of(ValueArgument));
							argument->span = IRParserCurrentSpan(&parser);
							String constant = IRParserBump(&parser, IRTokenKind_Number);
							Assert(U64FromString(constant, &argument->constant));
							break;
						}

						default:
						{
							IRParserError(&parser, S("expected value name or constant"));
							IRParserBumpAny(&parser);
							break;
						}
					}

					if (argument != 0)
					{
						if (first_argument == 0)
						{
							first_argument = argument;
						}
						else
						{
							last_argument->next = argument;
						}
						last_argument = argument;
						argument_count++;
					}
				}

				if (value_kind == (ValueKind)-1 || failed_to_resolve_any)
				{
					continue;
				}

				Span argument_end_span = parser.previous->span;
				Span value_span = SpanUnion(argument_start_span, argument_end_span);

				ValueSignature value_signature = ValueSignatureFromValueKind(value_kind);
				if (argument_count != value_signature.argument_count)
				{
					IRParserErrorWithSpan(&parser, value_span,
					        PushStringF("expected %td arguments to value but found %td", value_signature.argument_count,
					                argument_count));
					continue;
				}

				smm argument_index = -1;
				b32 any_mismatched_types = 0;
				for (ValueArgument *argument = first_argument; argument != 0; argument = argument->next)
				{
					argument_index++;

					ValueArgumentKind actual_argument_kind = ValueArgumentKind_Value;
					if (argument->value == 0)
					{
						actual_argument_kind = ValueArgumentKind_Constant;
					}

					Assert(argument_index < ArrayCount(value_signature.argument_kinds));
					ValueArgumentKind expected_argument_kind = value_signature.argument_kinds[argument_index];

					if (actual_argument_kind != expected_argument_kind)
					{
						Assert(actual_argument_kind < ValueArgumentKind__Count);
						Assert(expected_argument_kind < ValueArgumentKind__Count);
						String actual_kind_name = value_argument_kind_names[actual_argument_kind];
						String expected_kind_name = value_argument_kind_names[expected_argument_kind];
						String message = PushStringF(
						        "expected %.*s but found %.*s", SF(expected_kind_name), SF(actual_kind_name));
						IRParserErrorWithSpan(&parser, argument->span, message);
						any_mismatched_types = 1;
					}
				}

				if (any_mismatched_types)
				{
					continue;
				}

				Value *value = calloc(1, (umm)size_of(Value));
				value->kind = value_kind;
				value->first_argument = first_argument;
				value->last_argument = last_argument;
				value->argument_count = argument_count;
				if (func->first_value == 0)
				{
					func->first_value = value;
				}
				else
				{
					func->last_value->next = value;
				}
				func->last_value = value;
				func->value_count++;

				ValueNameNode *node = calloc(1, (umm)size_of(ValueNameNode));
				node->name = value_name;
				node->value = value;
				if (parser.value_names.first == 0)
				{
					parser.value_names.first = node;
				}
				else
				{
					parser.value_names.last->next = node;
				}
				parser.value_names.last = node;
			}

			IRParserExpect(&parser, IRTokenKind_RBrace);

			continue;
		}

		IRParserError(&parser, S("expected function"));
		IRParserBumpAny(&parser);
	}

	return result;
}

function String
ValueName(Value *value, Value **indexed_values, smm value_count, smm *next_value_index)
{
	String result = S("v<null>");
	if (value != 0)
	{
		smm index = -1;
		for (smm i = 0; i < value_count; i++)
		{
			if (indexed_values[i] == value)
			{
				index = i;
				break;
			}
		}
		if (index == -1)
		{
			index = *next_value_index;
			Assert(index < value_count);
			indexed_values[index] = value;
			(*next_value_index)++;
		}
		result = PushStringF("v%td", index);
	}
	return result;
}

function String
IRPrint(IR *ir)
{
	StringList list = {0};

	for (Function *func = ir->first_function; func != 0; func = func->next)
	{
		Value **indexed_values = calloc((umm)func->value_count, size_of(Value *));
		smm next_value_index = 0;
		StringListPushF(&list, "func %.*s {\n", SF(func->name));

		for (Value *value = func->first_value; value != 0; value = value->next)
		{
			StringListPush(&list, S("\t"));

			String value_name = ValueName(value, indexed_values, func->value_count, &next_value_index);
			StringListPushF(&list, "%.*s = ", SF(value_name));

			Assert(value->kind >= 0);
			Assert(value->kind < ValueKind__Count);
			StringListPushF(&list, "%.*s ", SF(value_kind_names[value->kind]));

			for (ValueArgument *argument = value->first_argument; argument != 0; argument = argument->next)
			{
				if (argument->value == 0)
				{
					StringListPushF(&list, "%llu", argument->constant);
				}
				else
				{
					String name = ValueName(argument->value, indexed_values, func->value_count, &next_value_index);
					StringListPushF(&list, "%.*s", SF(name));
				}
				if (argument->next != 0)
				{
					StringListPush(&list, S(", "));
				}
			}

			StringListPush(&list, S("\n"));
		}

		StringListPush(&list, S("}\n"));
	}

	return StringListJoin(list);
}

typedef struct ValueMapNode ValueMapNode;
struct ValueMapNode
{
	ValueMapNode *next;
	Value *value;
	Value *new;
};

typedef struct ValueMap ValueMap;
struct ValueMap
{
	ValueMapNode *first;
	ValueMapNode *last;
};

function ValueMapNode *
ValueMapGet(ValueMap *map, Value *value)
{
	Assert(value != 0);
	ValueMapNode *result = 0;
	for (ValueMapNode *node = map->first; node != 0; node = node->next)
	{
		if (node->value == value)
		{
			result = node;
			break;
		}
	}
	return result;
}

function ValueMapNode *
ValueMapInsert(ValueMap *map, Value *value)
{
	Assert(value != 0);
	ValueMapNode *result = ValueMapGet(map, value);
	if (result == 0)
	{
		result = calloc(1, (umm)size_of(ValueMapNode));
		result->value = value;
		if (map->first == 0)
		{
			map->first = result;
		}
		else
		{
			map->last->next = result;
		}
		map->last = result;
	}
	return result;
}

function IR *
DeadCodeElimination(IR *ir)
{
	IR *result = calloc(1, (umm)size_of(IR));

	for (Function *func = ir->first_function; func != 0; func = func->next)
	{
		Function *new_func = calloc(1, (umm)size_of(Function));
		new_func->name = func->name;

		if (result->first_function == 0)
		{
			result->first_function = new_func;
		}
		else
		{
			result->last_function->next = new_func;
		}
		result->last_function = new_func;
		result->function_count++;

		ValueMap used_values = {0};
		for (Value *value = func->first_value; value != 0; value = value->next)
		{
			for (ValueArgument *argument = value->first_argument; argument != 0; argument = argument->next)
			{
				if (argument->value != 0)
				{
					ValueMapInsert(&used_values, argument->value);
				}
			}
		}
		for (Value *value = func->first_value; value != 0; value = value->next)
		{
			ValueMapNode *map_node = ValueMapGet(&used_values, value);
			if (map_node != 0)
			{
				Value *new_value = calloc(1, (umm)size_of(Value));
				new_value->kind = value->kind;

				for (ValueArgument *argument = value->first_argument; argument != 0; argument = argument->next)
				{
					ValueArgument *new_argument = calloc(1, (umm)size_of(ValueArgument));
					if (argument->value == 0)
					{
						new_argument->constant = argument->constant;
					}
					else
					{
						new_argument->value = ValueMapGet(&used_values, argument->value)->new;
						Assert(new_argument->value != 0);
					}

					if (new_value->first_argument == 0)
					{
						new_value->first_argument = new_argument;
					}
					else
					{
						new_value->last_argument->next = new_argument;
					}
					new_value->last_argument = new_argument;
					new_value->argument_count++;
				}

				map_node->new = new_value;

				if (new_func->first_value == 0)
				{
					new_func->first_value = new_value;
				}
				else
				{
					new_func->last_value->next = new_value;
				}
				new_func->last_value = new_value;
				new_func->value_count++;
			}
		}
	}

	return result;
}

typedef IR *(*IRTransformer)(IR *ir);

function void
IRTest(String category, String subcategory, IRTransformer transformer)
{
	DirectoryIterator iterator = {0};
	DirectoryIterate(&iterator, PushStringF("%.*s/%.*s", SF(category), SF(subcategory)));

	for (DirectoryEntry entry = {0}; DirectoryIteratorNext(&iterator, &entry); MemoryZeroStruct(&entry))
	{
		String contents = ReadFile(entry);
		String source = {0};
		String expected_output = {0};
		Assert(StringCut(contents, &source, S("===\n"), &expected_output));

		DiagnosticList diagnostics = {0};
		IR *ir = IRParse(source, &diagnostics);
		if (transformer != 0)
		{
			ir = transformer(ir);
		}

		String actual_output = PushStringF("%.*s%.*s", SF(IRPrint(ir)), SF(DiagnosticListPrint(&diagnostics)));
		String test_name = PushStringF("%.*s > %.*s > %.*s", SF(category), SF(subcategory), SF(entry.name));
		if (!StringEqual(expected_output, actual_output))
		{
			printf("Fail: %.*s\n", SF(test_name));
			printf("expected:\n%.*sactual:\n%.*s", SF(expected_output), SF(actual_output));
			Assert(0);
		}
		else
		{
			printf("Pass: %.*s\n", SF(test_name));
		}
	}
}

function void
IRTests(void)
{
	IRTest(S("IR Tests"), S("Parse"), 0);
	IRTest(S("IR Tests"), S("Dead Code Elimination"), DeadCodeElimination);
}
