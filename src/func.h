#ifndef FUNC_H
#define FUNC_H

#include "remap.h"
#include "lparse.h"

enum {
	cmd_funccall,
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
	int fid;
	char *fname;

	struct cmd_unit *args;
	int argnum;
	char *pat;
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
	struct var_sym_tbl stb;
	struct cmd_list *cl;
	struct alloc *alloc_table;
};

struct fcall_list_el {
	int fid;
	struct coord pos;
	char *pattern;
	struct fcall_list_el *next;
};

struct fcall_list {
	struct fcall_list_el *first;
	struct fcall_list_el *last;
};

void func_header_form(struct lexem_list *l, struct function *f,
		int gl_spec, struct gn_sym_tbl *tb);
void cmd_form(struct lexem_list *l, struct function *f,
		struct gn_sym_tbl *tb, struct fcall_list *cl);
void cmd_list_free(struct cmd_list *l);
void debug_fcall_list(struct fcall_list *l, struct gn_sym_tbl *tb);
void check_functions(struct gn_sym_tbl *tb, struct fcall_list *fcl);
#endif
