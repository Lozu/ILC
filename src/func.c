#include <errno.h>
#include <limits.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "lparse.h"
#include "func.h"

#include "lparse.h"
#include "global.h"

static const struct {
	char rt;
	char *args[3];
} command_patterns[] = {
	[cmd_copy]	=	{ 'i',	{ "n", "i", NULL } },
	[cmd_add]	=	{ 'i',	{ "ii", NULL } },
	[cmd_ret]	=	{ 0,	{ "", "i", NULL } }
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
		f->type = 'i';
		lexem_clever_get(l, &b, ltvec2, 1);
		f->name = b.dt.str_value;
	} else {
		f->type = 0;
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
	for (; arg_number < func_max_args; ++arg_number) {
		if (fdecl_eat_arg(l, arg_number, f) == 0) {
			++arg_number;
			break;
		}
	}
	if (arg_number == func_max_args) {
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
	if (debug[dbg_function_header] == 0)
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
	struct lexem_block b;

	enum lexem_type  vec1[4] = { lx_new_line, lx_close_brace,
			lx_var_remapped, 1000000 + lx_cmd_copy };
	enum lexem_type vec2[1] = { lx_equal_sign };
	enum lexem_type vec3[1] = { 1000000 + lx_cmd_copy };
	switch(lexem_clever_get(l, &b, vec1, 4)) {
	case 0:
		return 1;
	case 1:
		return 0;
	case 2:
		cmd.ret_var.type = 'i';
		cmd.ret_var.id = b.dt.number;
		cmd.ret_var.pos = b.crd;
		lexem_clever_get(l, &b, vec2, 1);
		lexem_clever_get(l, &b, vec3, 1);
		type_ready = 1;
	case 3:
		if (type_ready == 0)
			cmd.ret_var.type = 'v';
		cmd.type = b.lt - 1000;
		cmd.pos = b.crd;
		break;
	}
	cmd_eat_args(l, &cmd);
	cmd_list_add(cl, &cmd);
	return 1;
}

static int cmd_eat_arg(struct lexem_list *l, struct cmd_unit *arg, int *pos);

static void cmd_eat_args(struct lexem_list *l, struct command *c)
{
	struct cmd_unit argbuf[func_max_args];
	int argbuf_pos = 0;
	while (argbuf_pos < func_max_args) {
		if (cmd_eat_arg(l, argbuf + argbuf_pos, &argbuf_pos) == 0)
			break;
	}

	if (argbuf_pos == func_max_args)
		die("%d: %s\n", c->pos.row, msg_too_many_args);

	c->args = smalloc(argbuf_pos * sizeof(struct cmd_unit));
	memcpy(c->args, argbuf, argbuf_pos * sizeof(struct cmd_unit));
	c->arg_number = argbuf_pos;
}

static long long convert_number(char *s, struct coord *pos);

static int cmd_eat_arg(struct lexem_list *l, struct cmd_unit *arg, int *pos)
{
	enum lexem_type vec1[3] = { lx_var_remapped, lx_number, lx_new_line };
	enum lexem_type vec2[2] = { lx_new_line, lx_coma };
	struct lexem_block b;
	switch(lexem_clever_get(l, &b, vec1, 3)) {
	case 0:
		arg->type = 'i';
		arg->id = b.dt.number;
		break;
	case 1:
		arg->type = 'n';
		arg->id = convert_number(b.dt.str_value, &b.crd);
		break;
	case 2:
		if (*pos == 0)
			return 0;
		else
			die("%d,%d: %s - expected %s or %s, not %s\n",
					b.crd.row, b.crd.col, LNAME(lx_var_remapped, 0, NULL),
					LNAME(lx_number, 0, NULL), LNAME(lx_new_line, 0, NULL));
		break;
	}
	++*pos;
	arg->pos = b.crd;
	return lexem_clever_get(l, &b, vec2, 2);
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

static char *get_argpat(char *buf, struct command *c)
{
	int i;
	for (i = 0; i < c->arg_number; ++i)
		buf[i] = c->args[i].type;
	buf[i] = 0;
	return buf;
}

#define GET_ARGPAT(c) \
		({ char *buf = alloca(func_max_args + 1); \
		get_argpat(buf, (c)); })

static void debug_print_arg(struct cmd_unit *u, int last);

static void debug_commands(struct cmd_list *l)
{
	struct cmd_list_el *tmp;
	if (debug[dbg_commands] == 0)
		return;
	fprintf(stderr, "---Commands---\n");
	for (tmp = l->first; tmp; tmp = tmp->next) {
		int i;
		if (tmp->cmd.ret_var.type == 'i')
			fprintf(stderr, "  [%lld]\t", tmp->cmd.ret_var.id);
		else
			fprintf(stderr, "  \t");

		fprintf(stderr, "%s(%c:%s):\t",
				LNAME(tmp->cmd.type + 1000, 1, NULL), tmp->cmd.ret_var.type,
				GET_ARGPAT(&tmp->cmd));

		for (i = 0; i < tmp->cmd.arg_number; ++i) {
			debug_print_arg(tmp->cmd.args + i,
					i + 1 == tmp->cmd.arg_number);
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\n");
}

static void debug_print_arg(struct cmd_unit *u, int last)
{
	switch(u->type) {
	case 'n':
		fprintf(stderr, "%lld", u->id);
		break;
	case 'i':
		fprintf(stderr, "[%lld]", u->id);
		break;
	}
	fprintf(stderr, !last ?  ", " : "");
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
		if (tmp->cmd.ret_var.type == 'i')
			acheck_inc_res(&table, &tmp->cmd.ret_var);
	}
}

static void acheck_args(struct acheck_tbl *t, struct cmd_unit *array,
		int len)
{
	int i;
	for (i = 0; i < len; ++i) {
		if (array[i].type == 'i' && t->tbl[array[i].id] == 0)
			die("%d,%d: arg %d undeclared\n", array[i].pos.row,
					array[i].pos.col, i + 1);
	}
}

static void acheck_inc_res(struct acheck_tbl *t, struct cmd_unit *res)
{
	t->tbl[res->id] = 1;
}

static void tv_check_args(struct command *c);
static void tv_check_ret(int ctype, struct coord *pos, char rtype);

static void type_validity_check(struct cmd_list *l)
{
	struct cmd_list_el *tmp;
	for (tmp = l->first; tmp; tmp = tmp->next) {
		tv_check_args(&tmp->cmd);
		tv_check_ret(tmp->cmd.type, &tmp->cmd.pos, tmp->cmd.ret_var.type);
	}
}

static void tv_check_args(struct command *c)
{
	char **tmp = command_patterns[c->type].args;
	int i;
	for (i = 0; tmp[i]; ++i) {
		if (strcmp(*tmp, GET_ARGPAT(c)) == 0)
			return;
	}
	if (!tmp[i]) {
		die("%d: %s - invalid pattern (%s)\n", c->pos.row,
				LNAME(c->ret_var.type + 1000, 0, NULL), GET_ARGPAT(c));
	}
}

static void tv_check_ret(int ctype, struct coord *pos, char rtype)
{
	char ret = command_patterns[ctype].rt;
	if (ret == 'i' && rtype == 0)
		warn("%d,%d: %s - return value ignored\n", pos->row, pos->col,
				LNAME(ctype + 1000, 0, NULL));
	if (ret == 0 && rtype == 'i')
		die("%d,%d: %s - void assignment\n", pos->row, pos->col,
				LNAME(ctype + 1000, 0, NULL));
}

struct cmd_list *cmd_list_init()
{
	struct cmd_list *tmp = smalloc(sizeof(struct cmd_list));
	tmp->first = tmp->last = NULL;
	return tmp;
}

void cmd_list_add(struct cmd_list *l, struct command *c)
{
	if (l->first == NULL)
		l->first = l->last = smalloc(sizeof(struct cmd_list_el));
	else
		l->last = l->last->next = smalloc(sizeof(struct cmd_list_el));
	l->last->next = NULL;
	l->last->cmd = *c;
}
