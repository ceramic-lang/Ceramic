typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef ptrdiff_t smm;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t umm;

typedef s8 b8;
typedef s16 b16;
typedef s32 b32;
typedef s64 b64;

#define function static
#define global static
#define local_persist static
#define read_only __attribute__((section("__DATA_CONST,__const")))

#define size_of(x) ((smm)(sizeof(x)))

#define ArrayCount(a) (size_of(a) / size_of((a)[0]))

#define Assert(b) \
	if (!(b)) \
	{ \
		__builtin_debugtrap(); \
	}

function void AssertZero(void *p, smm size);
#define AssertZeroStruct(p) (AssertZero((p), size_of(*(p))))

function void MemoryCopy(void *dst, void *src, smm size);
function void MemorySet(void *dst, u8 byte, smm size);
function void MemoryZero(void *dst, smm size);
function void *MemoryFind(void *haystack, u8 needle, smm size);
function smm MemoryCompare(void *p0, void *p1, smm size);
function b32 MemoryEqual(void *p0, void *p1, smm size);

#define MemoryZeroStruct(dst) (MemoryZero((dst), size_of(*(dst))))

function b32 IsAlphabetic(u8 byte);
function b32 IsDigit(u8 byte);
function b32 IsAlphanumeric(u8 byte);
function b32 IsWhitespace(u8 byte);
