#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/attr.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "Core.h"
#include "Strings.h"
#include "File System.h"
#include "IR.h"

#include "Core.c"
#include "Strings.c"
#include "File System.c"
#include "IR.c"

s32
main(void)
{
	IRTests();

	u32 machine_code[] = {
	        0xd2800b80, // mov x0, #92
	        0xd65f03c0, // ret
	};

	u8 string_table[] = "\0_main";
	struct nlist_64 symbol_table[] = {
	        {
	                .n_un.n_strx = 1,
	                .n_type = N_SECT | N_EXT,
	                .n_sect = 1, // Sections are numbered starting from 1.
	        },
	};

	smm section_content_size = 0;

	struct mach_header_64 header = {0};
	header.magic = MH_MAGIC_64;
	header.cputype = CPU_TYPE_ARM64;
	header.filetype = MH_OBJECT;

	u16 os_version_major = 15;
	u8 os_version_minor = 0;
	u8 os_version_patch = 0;

	u16 sdk_version_major = 15;
	u8 sdk_version_minor = 0;
	u8 sdk_version_patch = 0;

	struct build_version_command platform_command = {0};
	platform_command.cmd = LC_BUILD_VERSION;
	platform_command.cmdsize = sizeof(struct build_version_command);
	platform_command.platform = PLATFORM_MACOS;
	platform_command.minos =
	        ((u32)os_version_major << 16) | ((u32)os_version_minor << 8) | ((u32)os_version_patch << 0);
	platform_command.sdk =
	        ((u32)sdk_version_major << 16) | ((u32)sdk_version_minor << 8) | ((u32)sdk_version_patch << 0);
	header.ncmds++;
	header.sizeofcmds += platform_command.cmdsize;

	// Object files have all their sections nested inside one “blank” segment for compactness.
	struct segment_command_64 segment_command = {0};
	segment_command.cmd = LC_SEGMENT_64;
	segment_command.cmdsize = sizeof(struct segment_command_64);
	segment_command.fileoff = (u64)section_content_size;

	struct section_64 section = {0};
	memcpy(&section.sectname, "__text", sizeof("__text") - 1);
	memcpy(&section.segname, "__TEXT", sizeof("__TEXT") - 1);
	section.size = sizeof(machine_code);
	section.offset = (u32)segment_command.fileoff;
	section.align = 2; // ARM64 requires instructions to be aligned to 4-byte (2^2) boundaries.
	section.flags = S_REGULAR | S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS;
	segment_command.nsects++;
	segment_command.cmdsize += sizeof(struct section_64);
	segment_command.vmsize += section.size;
	segment_command.filesize += section.size;
	section_content_size += section.size;

	header.ncmds++;
	header.sizeofcmds += segment_command.cmdsize;

	struct symtab_command symbol_table_command = {0};
	symbol_table_command.cmd = LC_SYMTAB;
	symbol_table_command.cmdsize = sizeof(struct symtab_command);

	symbol_table_command.symoff = (u32)section_content_size;
	symbol_table_command.nsyms = sizeof(symbol_table) / sizeof(symbol_table[0]);
	section_content_size += sizeof(symbol_table);

	symbol_table_command.stroff = (u32)section_content_size;
	symbol_table_command.strsize = sizeof(string_table);
	section_content_size += sizeof(string_table);

	header.ncmds++;
	header.sizeofcmds += symbol_table_command.cmdsize;

	smm section_content_offset = sizeof(struct mach_header_64) + header.sizeofcmds;
	segment_command.fileoff += (u64)section_content_offset;
	section.offset += (u64)section_content_offset;
	symbol_table_command.symoff += section_content_offset;
	symbol_table_command.stroff += section_content_offset;

	smm total_file_size = section_content_offset + section_content_size;
	u8 *content = calloc((umm)total_file_size, 1);
	u8 *cursor = content;

	*(struct mach_header_64 *)cursor = header;
	cursor += sizeof(struct mach_header_64);

	*(struct build_version_command *)cursor = platform_command;
	cursor += sizeof(struct build_version_command);

	*(struct segment_command_64 *)cursor = segment_command;
	cursor += sizeof(struct segment_command_64);

	*(struct section_64 *)cursor = section;
	cursor += sizeof(struct section_64);

	*(struct symtab_command *)cursor = symbol_table_command;
	cursor += sizeof(struct symtab_command);

	Assert(cursor - content == section_content_offset);

	memcpy(cursor, machine_code, sizeof(machine_code));
	cursor += sizeof(machine_code);

	memcpy(cursor, symbol_table, sizeof(symbol_table));
	cursor += sizeof(symbol_table);

	memcpy(cursor, string_table, sizeof(string_table));
	cursor += sizeof(string_table);

	Assert(cursor - content == total_file_size);

	s32 fd = open("main.o", O_WRONLY | O_CREAT | O_TRUNC, 0666);
	Assert(fd >= 0);
	smm bytes_written = write(fd, content, (umm)total_file_size);
	Assert(bytes_written == total_file_size);
}
