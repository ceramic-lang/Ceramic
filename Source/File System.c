function String
ReadFile(DirectoryEntry entry)
{
	String result = {0};
	result.length = entry.size;
	result.data = calloc((umm)result.length, (umm)size_of(u8));

	s32 fd = open(entry.path_cstring, O_RDONLY, 0);
	Assert(fd >= 0);

	smm bytes_read = read(fd, result.data, (umm)result.length);
	Assert(bytes_read == result.length);

	return result;
}

function void
DirectoryIterate(DirectoryIterator *iterator, String path)
{
	AssertZeroStruct(iterator);

	iterator->fd = open(CStringFromString(path), O_RDONLY, 0);
	Assert(iterator->fd >= 0);

	iterator->attribute_list.bitmapcount = ATTR_BIT_MAP_COUNT;
	iterator->attribute_list.commonattr = ATTR_CMN_RETURNED_ATTRS | ATTR_CMN_NAME | ATTR_CMN_FULLPATH;
	iterator->attribute_list.fileattr = ATTR_FILE_DATALENGTH;
}

function b32
DirectoryIteratorNext(DirectoryIterator *iterator, DirectoryEntry *entry)
{
	AssertZeroStruct(entry);

	if (iterator->finished)
	{
		return 0;
	}

	if (iterator->remaining_entry_count == 0)
	{
		iterator->remaining_entry_count = getattrlistbulk(iterator->fd, &iterator->attribute_list,
		        iterator->attribute_buffer, size_of(iterator->attribute_buffer), 0);
		if (iterator->remaining_entry_count == 0)
		{
			iterator->finished = 1;
			return 0;
		}
		iterator->attribute_buffer_cursor = 0;
	}

	Assert(iterator->remaining_entry_count >= 1);

	Assert(iterator->attribute_buffer_cursor >= 0);
	Assert(iterator->attribute_buffer_cursor < size_of(iterator->attribute_buffer));

	u8 *attribute_buffer_end = iterator->attribute_buffer + size_of(iterator->attribute_buffer);
	u8 *cursor = iterator->attribute_buffer + iterator->attribute_buffer_cursor;

	smm entry_length = *(u32 *)cursor;
	cursor += size_of(u32);

	attribute_set_t attribute_set = *(attribute_set_t *)cursor;
	cursor += size_of(attribute_set_t);

	if (attribute_set.commonattr & ATTR_CMN_NAME)
	{
		attrreference_t name_reference = *(attrreference_t *)cursor;

		String name_original = {0};
		name_original.data = cursor + name_reference.attr_dataoffset;
		Assert(name_original.data < attribute_buffer_end);
		name_original.length = name_reference.attr_length;
		Assert(name_original.data + name_original.length < attribute_buffer_end);

		// Trim off null terminator.
		Assert(name_original.data[name_original.length - 1] == 0);
		name_original.length--;

		entry->name = StringCopy(name_original);
		cursor += size_of(attrreference_t);
	}

	if (attribute_set.commonattr & ATTR_CMN_FULLPATH)
	{
		attrreference_t path_reference = *(attrreference_t *)cursor;

		String path_original = {0};
		path_original.data = cursor + path_reference.attr_dataoffset;
		Assert(path_original.data < attribute_buffer_end);
		path_original.length = path_reference.attr_length;
		Assert(path_original.data + path_original.length < attribute_buffer_end);

		entry->path = StringCopy(path_original);
		entry->path.length--;
		entry->path_cstring = (char *)entry->path.data;

		cursor += size_of(attrreference_t);
	}

	entry->size = -1;
	if (attribute_set.fileattr & ATTR_FILE_DATALENGTH)
	{
		entry->size = *(s64 *)cursor;
		cursor += size_of(s64);
	}

	iterator->attribute_buffer_cursor += entry_length;
	iterator->remaining_entry_count--;

	return 1;
}
