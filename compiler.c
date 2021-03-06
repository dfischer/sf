#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "tokenizer.h"
#include "wordlink.h"
#include "codebuf.h"

#define COMPILE() codebuf_append(mode == MODE_MACRO \
		? &machine->macros \
		: &machine->program, ins)

enum mode {
	MODE_NORMAL,
	MODE_NAME,
	MODE_MACRO,
	MODE_COMMENT
};

int compiler_compile(
		struct machine *machine,
		struct wordlink **words,
		char *program,
		int doend)
{
	size_t i;
	char *token;
	enum mode mode;
	struct instruction ins;
	char *endptr;
	DTYPE val;
	size_t len;
	int address;
	struct tokenizer tokenizer;
	int comment;

	comment = 0;
	mode = MODE_NORMAL;
	tokenizer_init(&tokenizer);
	len = strlen(program);

	for (i = 0; i < len + 1; i++) {
		token = tokenizer_get(&tokenizer, program[i]);
		if (!token) continue;

		if (mode == MODE_NAME) {
			ins.type = INS_PRGM;
			ins.data.program = machine->macros.head - machine->macros.start;
			if (!wordlink_add(words, token, ins)) {
				fprintf(stderr, "Unable to create word link for \"%s\"\n", token);
			}
			mode = MODE_MACRO;
			continue;
		}

		if (strlen(token) == 1) switch (token[0]) {
		case '(':
			comment = 1;
			continue;
		case ')':
			if (comment) comment = 0;
			continue;
		case ':':
			switch (mode) {
			case MODE_NAME:
				fputs("Can not create a macro named \":\"\n", stderr);
				return 0;
			case MODE_MACRO:
				fputs("Can not define a macro within a macro\n", stderr);
				return 0;
			default:
				mode = MODE_NAME;
			}
			continue;
		case ';':
			if (mode != MODE_MACRO) {
				fputs("\";\" may only be used to terminate a macro\n", stderr);
				return 0;
			}
			ins.type = INS_END;
			if (!codebuf_append(&machine->macros, ins)) {
				fputs("Unable to grow macro buffer\n", stderr);
				return 0;
			}
			mode = MODE_NORMAL;
			continue;
		}

		/* Find word by name */
		ins = wordlink_find(mode == MODE_MACRO
				? (*words)->prev
				: *words, token);

		/* If found... */
		if (ins.type != INS_NONE) {
			/* If macro, Copy instructions instead */
			if (ins.type == INS_PRGM) {
				address = ins.data.program;
				ins = machine->macros.start[address];
				while (ins.type != INS_END) {
					COMPILE();
					ins = machine->macros.start[++address];
				}
				continue;
			}
			COMPILE();
			continue;
		} 

		val = strtod(token, &endptr);
		if (*endptr != '\0' || endptr == token) {
			fprintf(stderr, "Word \"%s\" not found\n", token);
			return 0;
		}

		ins.type = INS_PUSH;
		ins.data.value = val;
		COMPILE();
	}

	if (mode != MODE_NORMAL) return 0;

	if (doend) {
		ins.type = INS_END;
		if (!codebuf_append(&machine->program, ins)) {
			fputs("Unable to grow program buffer", stderr);
			return 0;
		}
	}

	return 1;
}
