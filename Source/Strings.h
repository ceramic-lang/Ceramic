typedef struct String String;
struct String
{
	u8 *data;
	smm length;
};

#define S(literal) ((String){(u8 *)(literal), size_of(literal) - 1})
#define SF(string) (s32)(string).length, (string).data

function String StringCopy(String string);
function String StringSlice(String string, smm start, smm length);
function b32 StringEqual(String string1, String string2);
function smm StringFind(String haystack, String needle);
function b32 StringCut(String string, String *before, String separator, String *after);

function String PushStringFV(char *fmt, va_list ap);
function String PushStringF(char *fmt, ...) __attribute__((format(printf, 1, 2)));

function b32 U64FromString(String string, u64 *value);
function char *CStringFromString(String string);

typedef struct StringNode StringNode;
struct StringNode
{
	StringNode *next;
	String string;
};

typedef struct StringList StringList;
struct StringList
{
	StringNode *first;
	StringNode *last;
	smm count;
	smm total_length;
};

function void StringListPush(StringList *list, String string);
function void StringListPushF(StringList *list, char *fmt, ...) __attribute__((format(printf, 2, 3)));
function String StringListJoin(StringList list);
