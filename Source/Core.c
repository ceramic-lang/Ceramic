function void
AssertZero(void *p, smm size)
{
	Assert(size >= 0);
	u8 *end = (u8 *)p + size;
	for (u8 *cursor = p; cursor != end; cursor++)
	{
		Assert(*cursor == 0);
	}
}

function void
MemoryCopy(void *dst, void *src, smm size)
{
	Assert(size >= 0);
	memcpy(dst, src, size);
}

function void
MemorySet(void *dst, u8 byte, smm size)
{
	Assert(size >= 0);
	memset(dst, byte, size);
}

function void
MemoryZero(void *dst, smm size)
{
	Assert(size >= 0);
	MemorySet(dst, 0, size);
}

function void *
MemoryFind(void *haystack, u8 needle, smm size)
{
	Assert(size >= 0);
	return memchr(haystack, needle, (umm)size);
}

function smm
MemoryCompare(void *p0, void *p1, smm size)
{
	Assert(size >= 0);
	return memcmp(p0, p1, (umm)size);
}

function b32
MemoryEqual(void *p0, void *p1, smm size)
{
	Assert(size >= 0);
	return MemoryCompare(p0, p1, size) == 0;
}

function b32
IsAlphabetic(u8 byte)
{
	return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z');
}

function b32
IsDigit(u8 byte)
{
	return byte >= '0' && byte <= '9';
}

function b32
IsAlphanumeric(u8 byte)
{
	return IsAlphabetic(byte) || IsDigit(byte);
}

function b32
IsWhitespace(u8 byte)
{
	return byte == ' ' || byte == '\n' || byte == '\t';
}
