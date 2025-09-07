static uint8_t *arena_start;
static uint8_t *arena_used;
static uint8_t *arena_end;

static const size_t default_chunk_size = 64 * 1024;

static size_t pad_pow2(uintptr_t offset, size_t align) {
	size_t result = offset - (offset & (align - 1));
	if (result == offset) result = 0;
	return result;
}

static void *push_size(size_t size, size_t align) {
	size_t padding = pad_pow2((uintptr_t)arena_used, align);

	if (arena_end - arena_used < (ptrdiff_t)(padding + size)) {
		size_t chunk_size = size + align;
		chunk_size = default_chunk_size > chunk_size ? default_chunk_size : chunk_size;
		arena_start = calloc(chunk_size, 1);
		arena_used = arena_start;
		arena_end = arena_start + chunk_size;
		padding = pad_pow2((uintptr_t)arena_used, align);
	}

	assert(arena_end - arena_used >= (ptrdiff_t)(padding + size));
	void *result = arena_used + padding;
	assert(((uintptr_t)result & (align - 1)) == 0);
	arena_used += padding + size;
	return result;
}
