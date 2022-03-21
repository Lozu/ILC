#ifndef FUNC_H
#define FUNC_H

#include "remap.h"
#include "lparse.h"

enum type {
	void_t = 1,
	int_t = 2,
};

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

	enum type rtype;
	struct cmd_unit ret_var;

	int arg_number;
	struct cmd_unit *args;
};

struct function {
	char *name;
	struct coord pos;

	enum type rt;
	int arg_number;
	struct sym_tbl stb;
	struct cmd_list *cl;
};

void func_header_form(struct lexem_list *l, struct function *f);
void cmd_form(struct lexem_list *l, struct function *f);

#endif
