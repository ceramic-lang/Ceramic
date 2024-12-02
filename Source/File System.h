typedef struct DirectoryIterator DirectoryIterator;
struct DirectoryIterator
{
	b32 finished;
	s32 fd;
	struct attrlist attribute_list;
	// PATH_MAX is 1024 so this should hold a couple entries’s worth of attributes.
	u8 attribute_buffer[8 * 1024];
	smm attribute_buffer_cursor;
	smm remaining_entry_count;
};

typedef struct DirectoryEntry DirectoryEntry;
struct DirectoryEntry
{
	String name;
	String path;
	char *path_cstring;
	s64 size;
};

function String ReadFile(DirectoryEntry entry);
function void DirectoryIterate(DirectoryIterator *iterator, String path);
function b32 DirectoryIteratorNext(DirectoryIterator *iterator, DirectoryEntry *entry);
