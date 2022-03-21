#ifndef FHEAD_H
#define FHEAD_H

#include "remap.h"
#include "lparse.h"

enum {
	func_max_arg_number = 6
};

enum type {
	void_t,
	int_t,
};

struct function {
	char *name;
	struct coord pos;

	enum type rt;
	int arg_number;
	struct sym_tbl stb;
};

void func_header_form(struct lexem_list *l, struct function *f);

#endif
