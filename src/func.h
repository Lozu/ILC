#ifndef FUNC_H
#define FUNC_H

#include "remap.h"
#include "lparse.h"

enum {
	cmd_copy,
	cmd_add,
	cmd_ret
};

enum unit_type {
	ut_num = 1,
	ut_var = 2
};

struct cmd_unit {
	enum unit_type type;
	long long id;
	struct coord pos;
};

struct command {
	int type;
	struct coord pos;

	char rpat;
	struct cmd_unit ret_var;

	struct cmd_unit *args;
	int arg_number;
	char *argpat;
	int arg_pat;
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
	int arg_number;
	struct sym_tbl stb;
	struct cmd_list *cl;
};

void func_header_form(struct lexem_list *l, struct function *f);
void cmd_form(struct lexem_list *l, struct function *f);

#endif
