#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define countof(array) (sizeof(array) / sizeof((array)[0]))

_Noreturn static void unreachable(void) {
	__builtin_trap();
}

static void assert(bool b) {
	if (!b) unreachable();
}

#include "ceramic.h"
#include "parser.c"
#include "type.c"
#include "codegen.c"

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "ceramic: usage: ceramic <output path>\n");
		return 1;
	}
	char *output_path = argv[1];

	char buffer[1024] = {0};
	size_t n = 0;

	while (n < countof(buffer) - 1) {
		char c = (char)getchar();
		if (c == EOF) break;
		buffer[n++] = c;
	}

	struct node *root = parse(buffer);
	typecheck(root);
	codegen(root, fopen(output_path, "w"));
}
