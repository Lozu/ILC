#ifndef FUNC_H
#define FUNC_H

#include "remap.h"
#include "lparse.h"

enum {
	cmd_copy,
	cmd_add,
	cmd_ret
};

enum {
	func_max_args = 128,
};

struct cmd_unit {
	char type;
	long long id;
	struct coord pos;
};

struct command {
	int type;
	struct coord pos;

	struct cmd_unit ret_var;

	struct cmd_unit *args;
	int argnum;
};

struct cmd_list_el {
	struct command cmd;
	struct cmd_list_el *next;
};

struct cmd_list {
	struct cmd_list_el *first;
	struct cmd_list_el *last;
};

struct function {
	char *name;
	struct coord pos;

	char type;
	int argnum;
	struct sym_tbl stb;
	struct cmd_list *cl;
	struct alloc *alloc_table;
};

void func_header_form(struct lexem_list *l, struct function *f);
void cmd_form(struct lexem_list *l, struct function *f);

#endif
