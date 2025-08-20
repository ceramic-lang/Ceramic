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
#include "codegen.c"

int main(void) {
	static const size_t buffer_size = 1024;
	char buffer[buffer_size] = {0};
	size_t n = 0;

	while (n < buffer_size - 1) {
		char c = (char)getchar();
		if (c == EOF) break;
		buffer[n++] = c;
	}

	struct node *root = parse(buffer);
	codegen(root, stdout);
}
