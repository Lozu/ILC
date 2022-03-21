#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "lparse.h"
#include "func.h"

#include "lparse.h"
#include "global.h"

struct cmd_list_el {
	struct command cmd;
	struct cmd_list_el *next;
};

struct cmd_list {
	struct cmd_list_el *first;
	struct cmd_list_el *last;
};

enum {
	func_max_arg_number = 6,
	cmd_pat_args = 2,
};

static const struct {
	int rtypes;
	int args[cmd_pat_args];
} command_patterns[] = {
	[cmd_copy]	=	{ int_t,	{ ut_num|ut_var, 0		} },
	[cmd_add]	=	{ int_t,	{ ut_var,		 ut_var	} },
	[cmd_ret]	=	{ void_t,	{ ut_var,		 0		} }
};

static const char *func_arg_dup = "function argument duplicate";
static const char *func_too_many_args = "too many arguments";

static const char *msg_too_many_args = "too many arguments\n";
static const char *msg_underflow = "underflow detected";
static const char *msg_overflow = "overflow detected";

struct cmd_list *cmd_list_init();
void cmd_list_add(struct cmd_list *l, struct command *c);


static void fdecl_eat_args(struct lexem_list *l, struct function *f);
static void debug_function_header(struct function *f);

void func_header_form(struct lexem_list *l, struct function *f)
{
	struct lexem_block b;
	int status;
	enum lexem_type ltvec1[2] = { lx_int_spec, lx_func_name };
	enum lexem_type ltvec2[1] =  { lx_func_name };
	enum lexem_type ltvec3[1] = { lx_parenleft };
	enum lexem_type ltvec4[1] = { lx_open_brace };

	f->name = NULL;
	status = lexem_clever_get(l, &b, ltvec1, 2);
	if (status == 0) {
		f->rt = int_t;
		lexem_clever_get(l, &b, ltvec2, 1);
		f->name = b.dt.str_value;
	} else {
		f->rt = void_t;
		f->name = b.dt.str_value;
	}
	lexem_clever_get(l, &b, ltvec3, 1);
	fdecl_eat_args(l, f);
	lexem_clever_get(l, &b, ltvec4, 1);
	debug_function_header(f);
}

static int fdecl_eat_arg(struct lexem_list *l, int anum, struct function *f);

static void fdecl_eat_args(struct lexem_list *l, struct function *f)
{
	int arg_number = 0;
	for (; arg_number < func_max_arg_number; ++arg_number) {
		if (fdecl_eat_arg(l, arg_number, f) == 0) {
			++arg_number;
			break;
		}
	}
	if (arg_number == func_max_arg_number) {
		die("%s:[%d, %d]: %s\n", f->name, f->pos.row, f->pos.col,
				func_too_many_args);
	}
	f->arg_number = arg_number;
}

static int fdecl_eat_arg(struct lexem_list *l, int anum, struct function *f)
{
	int status;
	struct lexem_block b;
	enum lexem_type vec1[2] = { lx_new_line, lx_var_remapped };
	enum lexem_type vec2[2] = { lx_parenright, lx_coma };
	status = lexem_clever_get(l, &b, vec1, 2);
	if (status == 0)
		return fdecl_eat_arg(l, anum, f);
	if (b.dt.number != anum) {
		die(" function \'%s\': variable \'%s\'[%d,%d]: %s\n", f->name,
				f->stb.var_array[b.dt.number], b.crd.row, b.crd.col,
				func_arg_dup);
	}
	return lexem_clever_get(l, &b, vec2, 2);
}

static void debug_function_header(struct function *f)
{
	if (debug[function_header] == 0)
		return;
	fprintf(stderr, "---Function: \'%s\' [%d]---\n\n",
			f->name, f->arg_number);
}

static void cmd_primal_form(struct lexem_list *l, struct function *f);
static void availability_check(int func_args,
		struct cmd_list *l, int var_number);
static void type_validity_check(struct cmd_list *l);

void cmd_form(struct lexem_list *l, struct function *f)
{
	cmd_primal_form(l, f);
	availability_check(f->arg_number, f->cl, f->stb.var_arr_len);
	type_validity_check(f->cl);
}

static int cmd_extract(struct lexem_list *l, struct cmd_list *cl);
static void debug_commands(struct cmd_list *l);

static void cmd_primal_form(struct lexem_list *l, struct function *f)
{
	struct cmd_list *cl = cmd_list_init();
	while (cmd_extract(l, cl));
	debug_commands(cl);
	f->cl = cl;
}

static void cmd_eat_args(struct lexem_list *l, struct command *c);

static int cmd_extract(struct lexem_list *l, struct cmd_list *cl)
{
	struct command cmd;
	int type_ready = 0;
	int status;
	struct lexem_block b;

	enum lexem_type  vec1[4] = { lx_new_line, lx_close_brace,
			lx_var_remapped, 1000000 + lx_cmd_copy };
	enum lexem_type vec2[1] = { lx_equal_sign };
	enum lexem_type vec3[1] = { 1000000 + lx_cmd_copy };
	status = lexem_clever_get(l, &b, vec1, 4);
	switch(status) {
	case 0:
		return 1;
	case 1:
		return 0;
	case 2:
		cmd.rtype = int_t;
		cmd.ret_var.id = b.dt.number;
		cmd.ret_var.pos = b.crd;
		lexem_clever_get(l, &b, vec2, 1);
		lexem_clever_get(l, &b, vec3, 1);
		type_ready = 1;
	case 3:
		if (type_ready == 0)
			cmd.rtype = void_t;
		cmd.type = b.lt - 1000;
		cmd.pos = b.crd;
		break;
	}
	cmd_eat_args(l, &cmd);
	cmd_list_add(cl, &cmd);
	return 1;
}

static int cmd_eat_arg(struct lexem_list *l, struct cmd_unit *arg);

static void cmd_eat_args(struct lexem_list *l, struct command *c)
{
	struct cmd_unit argbuf[128];
	int argbuf_pos = 0;
	for (; argbuf_pos < 128; ++argbuf_pos) {
		if (cmd_eat_arg(l, argbuf + argbuf_pos) == 0) {
			++argbuf_pos;
			break;
		}
	}
	if (argbuf_pos == 128)
		die("%d: %s\n", c->pos.row, msg_too_many_args);
	memcpy(c->args, argbuf, argbuf_pos * sizeof(struct cmd_unit));
	c->arg_number = argbuf_pos;
}

static long long convert_number(char *s, struct coord *pos);

static int cmd_eat_arg(struct lexem_list *l, struct cmd_unit *arg)
{
	enum lexem_type vec1[2] = { lx_var_remapped, lx_number };
	enum lexem_type vec2[2] = { lx_coma, lx_new_line };
	int status;
	struct lexem_block b;
	status = lexem_clever_get(l, &b, vec1, 2);
	if (status == 0) {
		arg->type = ut_var;
		arg->id = b.dt.number;
	} else {
		arg->type = ut_num;
		arg->id = convert_number(b.dt.str_value, &b.crd);
	}
	arg->pos = b.crd;
	status = lexem_clever_get(l, &b, vec2, 2);
	return status == 0;
}

static long long convert_number(char *s, struct coord *pos)
{
	long long res = strtoll(s, NULL, 10);
	if (errno == ERANGE) {
		if (res == LLONG_MAX)
			die("%d,%d:\'%s\': %s\n", pos->row, pos->col, s, msg_overflow);
		else
			die("%d,%d:\'%s\': %s\n", pos->row, pos->col, s, msg_underflow);
	}
	return res;
}

static void debug_print_arg(struct cmd_unit *u, int last);

static void debug_commands(struct cmd_list *l)
{
	struct cmd_list_el *tmp;
	if (debug[commands] == 0)
		return;
	fprintf(stderr, "---Commands---\n");
	for (tmp = l->first; tmp; tmp = tmp->next) {
		int i;
		if (tmp->cmd.rtype == int_t)
			fprintf(stderr, "  [%lld]\t", tmp->cmd.ret_var.id);
		else
			fprintf(stderr, "  \t");
		fprintf(stderr, "%s: ", LNAME(tmp->cmd.type + 1000, 1, NULL));
		for (i = 0; i < tmp->cmd.arg_number; ++i)
			debug_print_arg(tmp->cmd.args + i, i + 1 == tmp->cmd.arg_number);
	}
	fprintf(stderr, "\n");
}

static void debug_print_arg(struct cmd_unit *u, int last)
{
	switch(u->type) {
	case ut_num:
		fprintf(stderr, "%lld", u->id);
		break;
	case ut_var:
		fprintf(stderr, "[%lld]", u->id);
		break;
	}
	fprintf(stderr, last ? "\n" : ", ");
}

struct acheck_tbl {
	char *tbl;
	int len;
};

static void acheck_args(struct acheck_tbl *t, struct cmd_unit *array,
		int len);
static void acheck_inc_res(struct acheck_tbl *t, struct cmd_unit *res);

static void availability_check(int func_args, struct cmd_list *l,
		int var_number)
{
	struct cmd_list_el *tmp;
	struct acheck_tbl table = { alloca(var_number), var_number };

	memset(table.tbl, 1, func_args);
	memset(table.tbl + func_args, 0, var_number - func_args);

	for (tmp = l->first; tmp; tmp = tmp->next) {
		acheck_args(&table, tmp->cmd.args, tmp->cmd.arg_number);
		if (tmp->cmd.rtype == int_t)
			acheck_inc_res(&table, &tmp->cmd.ret_var);
	}
}

static void acheck_args(struct acheck_tbl *t, struct cmd_unit *array,
		int len)
{
	int i;
	for (i = 0; i < len; ++i) {
		if (array[i].type == ut_var && t->tbl[array[i].id] == 0)
			die("%d,%d: arg %d undeclared\n", array[i].pos.row,
					array[i].pos.col, i + 1);
	}
}

static void acheck_inc_res(struct acheck_tbl *t, struct cmd_unit *res)
{
	t->tbl[res->id] = 1;
}

static void tv_check_ret_val(int ctype, struct coord *pos, enum type rtype);
static void tv_check_args(int ctype, struct coord *pos,
		struct cmd_unit *args, int anum);

static void type_validity_check(struct cmd_list *l)
{
	struct cmd_list_el *tmp;
	for (tmp = l->first; tmp; tmp = tmp->next) {
		tv_check_ret_val(tmp->cmd.type, &tmp->cmd.pos, tmp->cmd.rtype);
		tv_check_args(tmp->cmd.type, &tmp->cmd.pos, tmp->cmd.args,
				tmp->cmd.arg_number);
	}
}

static void tv_check_ret_val(int ctype, struct coord *pos, enum type rtype)
{
	int ret = command_patterns[ctype].rtypes;
	if (ret > void_t && rtype == void_t) {
		warn("%d,%d: %s - return value ignored\n", pos->row, pos->col,
				LNAME(ctype + 1000, 0, NULL));
	} else if ((ret & rtype) == 0) {
		die("%d,%d: %s - return value mismatch\n", pos->row, pos->col,
				LNAME(ctype + 1000, 0, NULL));
	}
}

static inline int cmd_pat_anum(int ctype)
{
	int *tmp = command_patterns[ctype].args;
	int i;
	for (i = 0; i < cmd_pat_args && tmp[i]; ++i);
	return i;
}

static void tv_check_args(int ctype, struct coord *pos,
		struct cmd_unit *args, int anum)
{
	int i;
	int argn = cmd_pat_anum(ctype);
	if (argn != anum) {
		die("%d, %d: %s - argument number mismatch\n", pos->row, pos->col,
				LNAME(ctype + 1000, 0, NULL));
	}
	for (i = 0; i < cmd_pat_args && command_patterns[ctype].args[i]; ++i) {
		if ((command_patterns[ctype].args[i] & args[i].type) == 0) {
			die("%d,%d: %s - %d argument invalid type\n", pos->row,
					pos->col, LNAME(ctype + 1000, 0, NULL), i + 1);
		}
	}
}

struct cmd_list *cmd_list_init()
{
	struct cmd_list *tmp = smalloc(sizeof(struct cmd_list));
	tmp->first = tmp->last = NULL;
	return tmp;
}

static void command_copy(struct command *dest, struct command *src);

void cmd_list_add(struct cmd_list *l, struct command *c)
{
	if (l->first == NULL)
		l->first = l->last = smalloc(sizeof(struct cmd_list_el));
	else
		l->last = l->last->next = smalloc(sizeof(struct cmd_list_el));
	l->last->next = NULL;
	command_copy(&l->last->cmd, c);
}

static void command_copy(struct command *dest, struct command *src)
{
	*dest = *src;
	dest->args = malloc(src->arg_number * sizeof(struct cmd_unit));
	memcpy(dest->args, src->args, src->arg_number * sizeof(struct cmd_unit));
}
