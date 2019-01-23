#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "types.h"
#include "instruction.h"
#include "wordlink.h"
#include "stack.h"
#include "codebuf.h"
#include "machine.h"
#include "compiler.h"
#include "std.h"
#include "readfile.h"

#define DEFAULT_WIDTH 256
#define DEFAULT_HEIGHT 256
#define MAX_DIM 8192

#define EXIT(_err) do { res = (_err); goto out; } while (0)

struct colorf {
	DTYPE r;
	DTYPE g;
	DTYPE b;
};

struct color {
	unsigned char r;
	unsigned char g;
	unsigned char b;
};

static void outputcolor(struct stack *stack, unsigned char **out)
{
	struct colorf c;
	switch (stack->index) {
	case 0:
		c.r = 0; c.g = 0; c.b = 0;
		break;
	case 1:
		c.r = stack_pop(stack); c.g = 0; c.b = 0;
		break;
	case 2:
		c.g = stack_pop(stack); c.r = stack_pop(stack); c.b = 0;
		break;
	default:
		c.b = stack_pop(stack);
		c.g = stack_pop(stack);
		c.r = stack_pop(stack);
		break;
	}

	c.r = c.r > 1.0 ? 1.0 : c.r < 0.0 ? 0.0 : c.r;
	c.g = c.g > 1.0 ? 1.0 : c.g < 0.0 ? 0.0 : c.g;
	c.b = c.b > 1.0 ? 1.0 : c.b < 0.0 ? 0.0 : c.b;

	*(*out)++ = (unsigned char)(c.r * 255);
	*(*out)++ = (unsigned char)(c.g * 255);
	*(*out)++ = (unsigned char)(c.b * 255);
}

int main(int argc, char **argv)
{
	struct machine m;
	size_t x, y, width, height, written;
	unsigned char *bufp;
	struct instruction *insp;
	unsigned char *framebuf = NULL;
	struct wordlink *words = NULL;
	char *program = NULL;
	int res = 0;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s [INIT] FILENAME\n", argv[0]);
		EXIT(1);
	}

	if (!machine_init(&m)) {
		fputs("Can't initialize VM\n", stderr);
		EXIT(1);
	}

	program = readfile(argv[argc == 2 ? 1 : 2]);
	if (program == NULL) {
		fprintf(stderr, "Can't read file \"%s\"\n", argv[argc == 2 ? 1 : 2]);
		EXIT(1);
	}

	if(!std_init(&words)) {
		fputs("Can't initialize standard library\n", stderr);
		EXIT(1);
	}

	if (argc == 3) {
		res = compiler_compile(&m, &words, argv[1], 0);
		/* Compiler reports its own errors to stderr */
		if (!res) EXIT(1);
	}

	res = compiler_compile(&m, &words, program, 1);
	if (!res) EXIT(1);

	free(program);
	program = NULL;

	wordlink_free(&words);

	/* Dry run to get width and height and check for stack under/overflows */
	m.mem[STD_WIDTH_LOC] = DEFAULT_WIDTH;
	m.mem[STD_HEIGHT_LOC] = DEFAULT_HEIGHT;
	insp = m.program.start;
	while (insp->type != INS_END) {
		instruction_execute(*insp++, &m);
		if (m.dstack.index < 0) {
			fputs("Data stack underflow\n", stderr);
			EXIT(1);
		} else if (m.rstack.index < 0) {
			fputs("Return stack underflow\n", stderr);
			EXIT(1);
		} else if (m.dstack.index > STACKSIZE) {
			fputs("Data stack overflow\n", stderr);
			EXIT(1);
		} else if (m.rstack.index > STACKSIZE) {
			fputs("Return stack overflow\n", stderr);
			EXIT(1);
		}
	}

	if (m.mem[STD_WIDTH_LOC] < 0.0 || m.mem[STD_HEIGHT_LOC] < 0.0) {
		fputs("Output dimensions may not be negative\n", stderr);
	}
	width = (size_t)m.mem[STD_WIDTH_LOC];
	height = (size_t)m.mem[STD_HEIGHT_LOC];
	if (width > MAX_DIM || height > MAX_DIM) {
		fprintf(stderr, "Output dimensions exceed maximum (%d)\n", MAX_DIM);
		EXIT(1);
	}

	if (!(framebuf = malloc(width * height * 3))) {
		fputs("Can't allocate frame buffer\n", stderr);
		EXIT(1);
	}
	bufp = framebuf;

	m.dstack.index = 0;
	m.rstack.index = 0;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			m.mem[0] = x / (DTYPE)width;
			m.mem[1] = y / (DTYPE)height;

			m.dstack.index = 0;
			m.rstack.index = 0;
			insp = m.program.start;
			while (insp->type != INS_END) {
				instruction_execute(*insp++, &m);
			}

			outputcolor(&m.dstack, &bufp);
		}
	}

	printf("P6\n%lu %lu\n255\n", width, height);
	written = fwrite(framebuf, 3, width * height, stdout);
	if (written != width * height) {
		fprintf(stderr, "%lu\n", written);
		fputs("Unable to write complete image to stdout\n", stderr);
		EXIT(1);
	}

out:
	free(framebuf);
	free(program);
	wordlink_free(&words);
	machine_free(&m);
	return res;
}
