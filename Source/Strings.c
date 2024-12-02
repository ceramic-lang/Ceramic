function String
StringCopy(String string)
{
	Assert(string.length >= 0);
	String result = {0};
	result.data = calloc((umm)string.length, (umm)size_of(u8));
	result.length = string.length;
	MemoryCopy(result.data, string.data, result.length);
	return result;
}

function String
StringSlice(String string, smm start, smm length)
{
	Assert(string.length >= 0);
	Assert(start >= 0);
	Assert(length >= 0);
	Assert(start <= string.length);
	Assert(start + length <= string.length);

	String result = string;
	result.data += start;
	result.length = length;
	return result;
}

function b32
StringEqual(String string1, String string2)
{
	Assert(string1.length >= 0);
	Assert(string2.length >= 0);
	b32 result = 0;
	if (string1.length == string2.length)
	{
		result = MemoryEqual(string1.data, string2.data, string1.length);
	}
	return result;
}

function smm
StringFind(String haystack, String needle)
{
	Assert(haystack.length >= 0);
	Assert(needle.length >= 0);
	smm result = -1;

	if (needle.length == 0)
	{
		result = 0;
	}
	else if (haystack.length > 0)
	{
		u8 needle_first = needle.data[0];
		u8 *cursor = haystack.data;
		u8 *end = haystack.data + haystack.length;

		for (; cursor + needle.length <= end;)
		{
			u8 *match = MemoryFind(cursor, needle_first, end - cursor);
			if (match == 0)
			{
				break;
			}
			Assert(match + needle.length <= end);

			Assert(match[0] == needle.data[0]);
			if (MemoryEqual(match + 1, needle.data + 1, needle.length - 1))
			{
				result = match - haystack.data;
				break;
			}

			cursor = match + 1;
		}
	}

	return result;
}

function b32
StringCut(String string, String *before, String separator, String *after)
{
	Assert(string.length >= 0);
	Assert(separator.length >= 0);
	b32 result = 0;
	MemoryZeroStruct(before);
	MemoryZeroStruct(after);

	smm index = StringFind(string, separator);
	if (index >= 0)
	{
		result = 1;
		*before = StringSlice(string, 0, index);
		*after = StringSlice(string, index + separator.length, string.length - index - separator.length);
	}

	return result;
}

function String
PushStringFV(char *fmt, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);

	String result = {0};
	result.length = vsnprintf(0, 0, fmt, ap);
	result.data = calloc((umm)(result.length + 1), size_of(u8));
	smm bytes_written = vsnprintf((char *)result.data, result.length + 1, fmt, ap2) + 1;
	Assert(bytes_written == result.length + 1);

	va_end(ap2);
	return result;
}

function String
PushStringF(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	String result = PushStringFV(fmt, ap);
	va_end(ap);
	return result;
}

function b32
U64FromString(String string, u64 *value)
{
	Assert(string.length >= 0);
	b32 result = 1;
	u64 n = 0;

	for (smm i = 0; i < string.length; i++)
	{
		u8 c = string.data[i];
		if (c >= '0' && c <= '9')
		{
			n = 10 * n + (c - '0');
		}
		else
		{
			result = 0;
			break;
		}
	}

	*value = n;
	return result;
}

function char *
CStringFromString(String string)
{
	Assert(string.length >= 0);
	char *cstring = calloc((umm)string.length + 1, (umm)size_of(char));
	MemoryCopy(cstring, string.data, string.length);
	return cstring;
}

function void
StringListPush(StringList *list, String string)
{
	Assert(list->count >= 0);
	Assert(list->total_length >= 0);
	Assert(string.length >= 0);
	if (string.length > 0)
	{
		StringNode *node = calloc(1, sizeof(StringNode));
		node->string = string;
		if (list->first == 0)
		{
			list->first = node;
		}
		else
		{
			list->last->next = node;
		}
		list->last = node;
		list->count++;
		list->total_length += string.length;
	}
}

function void
StringListPushF(StringList *list, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	String string = PushStringFV(fmt, ap);
	StringListPush(list, string);
	va_end(ap);
}

function String
StringListJoin(StringList list)
{
	Assert(list.count >= 0);
	Assert(list.total_length >= 0);
	String result = {0};
	result.data = calloc((umm)list.total_length, size_of(u8));
	for (StringNode *node = list.first; node != 0; node = node->next)
	{
		String string = node->string;
		Assert(string.length > 0);
		MemoryCopy(result.data + result.length, string.data, string.length);
		result.length += string.length;
	}
	return result;
}
