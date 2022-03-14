#ifndef FHEAD_H
#define FHEAD_H

#include "lparse.h"

enum {
	func_max_arg_count = 6
};

enum ret_type {
	void_t,
	int_t,
};

struct func_header {
	char *name;
	enum ret_type rt;
	int arg_number;
	int args[func_max_arg_count];
};

struct function {
	struct func_header fh;
};

void func_header_form(struct lexem_list *l, struct func_header *fh);

#endif
