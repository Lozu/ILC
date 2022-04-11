#include <errno.h>
#include <limits.h>
#include <alloca.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "lparse.h"
#include "alloc.h"
#include "process.h"
#include "global.h"
#include "func.h"

char *command_patterns[][3] = {
	[cmd_copy]	=	{ "i:n", "i:i", NULL },
	[cmd_add]	=	{ "i:ii", NULL },
	[cmd_ret]	=	{ "v:", "v:i", NULL }
};

static const char *msg_gl_dup = "global name already defined";
static const char *func_arg_dup = "function argument duplicate";
static const char *func_too_many_args = "too many arguments";

static const char *msg_too_many_args = "too many arguments\n";
static const char *msg_underflow = "underflow detected";
static const char *msg_overflow = "overflow detected";

struct cmd_list *cmd_list_init();
void cmd_list_add(struct cmd_list *l, struct command *c);

void fcall_list_add(struct fcall_list *l, int fid, char *pat,
		struct coord *pos);


static void set_gn_entry(struct gn_tbl_entry *e, struct coord *pos, int gl,
		char type, int argnum);
static void fdecl_eat_args(struct lexem_list *l, struct function *f);
static void debug_function_header(struct function *f, int gl);

void func_header_form(struct lexem_list *l, struct function *f,
		int gl_spec, struct gn_sym_tbl *tb)
{
	struct lexem_block b;
	int status;
	struct gn_tbl_entry *tmp;
	enum lexem_type ltvec1[2] = { lx_int_spec, lx_gn_rmp };
	enum lexem_type ltvec2[1] =  { lx_gn_rmp };
	enum lexem_type ltvec3[1] = { lx_parenleft };
	enum lexem_type ltvec4[1] = { lx_open_brace };

	status = lexem_clever_get(l, &b, ltvec1, 2);
	if (status == 0) {
		f->type = 'i';
		lexem_clever_get(l, &b, ltvec2, 1);
	} else {
		f->type = 0;
	}
	tmp = tb->vec + b.dt.number;
	f->name = tmp->name;
	f->pos = b.crd;

	lexem_clever_get(l, &b, ltvec3, 1);
	fdecl_eat_args(l, f);
	lexem_clever_get(l, &b, ltvec4, 1);
	set_gn_entry(tmp, &f->pos, gl_spec, f->type, f->argnum);
	debug_function_header(f, gl_spec);
}

static char *form_pattern(char rt, struct cmd_unit *args, int argnum)
{
	int i;
	char *tmp = smalloc(3 + argnum);

	tmp[0] = rt;
	tmp[1] = ':';
	for (i = 0; i < argnum; ++i) {
		if (args)
			tmp[i+2] = args[i].type;
		else
			tmp[i+2] = 'i';
	}
	tmp[i+2] = 0;
	return tmp;
}

static void set_gn_entry(struct gn_tbl_entry *e, struct coord *pos, int gl,
		char type, int argnum)
{
	e->pos = *pos;
	if (e->info != 0) {
		die("[%d,%d]:%s: %s\n", pos->row, pos->col,
				e->name, msg_gl_dup);
	}
	e->info = TBL_FUNC | TBL_DEF_HERE;
	if (gl)
		e->info |= TBL_GLOBAL;
	e->type_pattern = form_pattern(type, NULL, argnum);
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
	f->argnum = arg_number;
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
				f->stb.vec[b.dt.number], b.crd.row, b.crd.col,
				func_arg_dup);
	}
	return lexem_clever_get(l, &b, vec2, 2);
}

static void debug_function_header(struct function *f, int gl)
{
	if (dbg_function_header == 0)
		return;
	eprintf("--Function: %s\'%s\' [%d]--\n\n",
			gl ? "global " : "", f->name, f->argnum);
}

static void cmd_primal_form(struct lexem_list *l, struct function *f,
		struct gn_sym_tbl *tb);
static void availability_check(int func_args,
		struct cmd_list *l, int var_number);
static void type_validity_check(struct cmd_list *l, struct fcall_list *fcl);

/* TODO: Add funccall support */
void cmd_form(struct lexem_list *l, struct function *f,
		struct gn_sym_tbl *tb, struct fcall_list *fcl)
{
	cmd_primal_form(l, f, tb);
	availability_check(f->argnum, f->cl, f->stb.len);
	type_validity_check(f->cl, fcl);
}

static int cmd_extract(struct lexem_list *l, struct cmd_list *cl,
		struct gn_sym_tbl *tbl);
static void debug_commands(struct cmd_list *l, struct gn_sym_tbl *tb);

static void cmd_primal_form(struct lexem_list *l, struct function *f,
		struct gn_sym_tbl *tb)
{
	struct cmd_list *cl = cmd_list_init();
	while (cmd_extract(l, cl, tb));
	debug_commands(cl, tb);
	f->cl = cl;
}

static void cmd_eat_args(struct lexem_list *l, struct command *c);

static int cmd_extract(struct lexem_list *l, struct cmd_list *cl,
		struct gn_sym_tbl *tbl)
{
	struct command cmd;
	int type_ready = 0;
	struct lexem_block b;

	enum lexem_type  vec1[5] = { lx_new_line, lx_close_brace,
			lx_var_remapped, lx_gn_rmp,  1000000 + lx_cmd };
	enum lexem_type vec2[1] = { lx_equal_sign };
	enum lexem_type vec3[2] = { lx_gn_rmp, 1000000 + lx_cmd };
	switch(lexem_clever_get(l, &b, vec1, 5)) {
	case 0:
		return 1;
	case 1:
		return 0;
	case 2:
		cmd.ret_var.type = 'i';
		cmd.ret_var.id = b.dt.number;
		cmd.ret_var.pos = b.crd;
		lexem_clever_get(l, &b, vec2, 1);
		lexem_clever_get(l, &b, vec3, 2);
		type_ready = 1;
	case 3:
	case 4:
		if (type_ready == 0)
			cmd.ret_var.type = 'v';
		if (b.lt == lx_gn_rmp) {
			cmd.type = cmd_funccall;
			cmd.fid = b.dt.number;
			cmd.fname = tbl->vec[cmd.fid].name;
		} else {
			cmd.type = b.lt - 1000;
		}
		cmd.pos = b.crd;
		break;
	}
	cmd_eat_args(l, &cmd);
	cmd_list_add(cl, &cmd);
	return 1;
}

static int cmd_eat_arg(int is_func, struct lexem_list *l,
		struct cmd_unit *arg, int *pos);

static void cmd_eat_args(struct lexem_list *l, struct command *c)
{
	int is_func = c->type == cmd_funccall;
	struct cmd_unit argbuf[func_max_args];
	int argbuf_pos = 0;
	struct lexem_block b;
	enum lexem_type vec1[1] = { lx_parenleft };
	enum lexem_type vec2[1] = { lx_new_line };

	if (is_func)
		lexem_clever_get(l, &b, vec1, 1);

	while (argbuf_pos < func_max_args) {
		if (cmd_eat_arg(is_func, l, argbuf + argbuf_pos, &argbuf_pos) == 0)
			break;
	}

	if (is_func)
		lexem_clever_get(l, &b, vec2, 1);

	if (argbuf_pos == func_max_args)
		die("%d: %s\n", c->pos.row, msg_too_many_args);

	c->argnum = argbuf_pos;
	if (argbuf_pos > 0) {
		c->args = smalloc(argbuf_pos * sizeof(struct cmd_unit));
		memcpy(c->args, argbuf, argbuf_pos * sizeof(struct cmd_unit));
	}

	c->pat = form_pattern(c->ret_var.type, c->args, c->argnum);
}


static int cmd_get_cmdarg(struct lexem_list *l,
		struct cmd_unit *arg, int *pos);
static int cmd_get_funcarg(struct lexem_list *l,
		struct cmd_unit *arg, int *pos);

static int cmd_eat_arg(int is_func, struct lexem_list *l,
		struct cmd_unit *arg, int *pos)
{
	struct lexem_block b;
	enum lexem_type vec1[2] = { lx_new_line, lx_coma };
	enum lexem_type vec2[2] = { lx_parenright, lx_coma };

	if (!is_func) {
		if (cmd_get_cmdarg(l, arg, pos) == 0)
			return 0;
		else
			return lexem_clever_get(l, &b, vec1, 2);
	} else {
		if (cmd_get_funcarg(l, arg, pos) == 0)
			return 0;
		else
			return lexem_clever_get(l, &b, vec2, 2);
	}
}

static long long convert_number(char *s, struct coord *pos);

static int cmd_get_cmdarg(struct lexem_list *l,
		struct cmd_unit *arg, int *pos)
{
	struct lexem_block b;
	enum lexem_type vec[3] = { lx_var_remapped, lx_number, lx_new_line };

	switch(lexem_clever_get(l, &b, vec, 3)) {
	case 0:
		arg->type = 'i';
		arg->id = b.dt.number;
		break;
	case 1:
		arg->type = 'n';
		arg->id = convert_number(b.dt.str_value, &b.crd);
		free(b.dt.str_value);
		break;
	case 2:
		if (*pos == 0)
			return 0;
		else
			die("%d,%d: %s - expected %s or %s, not %s\n",
					b.crd.row, b.crd.col, LNAME(lx_var_remapped, 0, NULL),
					LNAME(lx_number, 0, NULL), LNAME(lx_new_line, 0, NULL));
	}
	arg->pos = b.crd;
	++*pos;
	return 1;
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

static int cmd_get_funcarg(struct lexem_list *l,
		struct cmd_unit *arg, int *pos)
{
	struct lexem_block b;
	enum lexem_type vec[2] = { lx_var_remapped, lx_new_line };
	switch(lexem_clever_get(l, &b, vec, 1)) {
	case 0:
		arg->type = 'i';
		arg->id = b.dt.number;
		break;
	case 1:
		if (*pos == 0)
			return 0;
		else
			die("%d,%d: %s - expected %s, not %s\n", b.crd.row, b.crd.col,
					LNAME(lx_var_remapped, 0, NULL),
					LNAME(lx_new_line, 0, NULL));
	}
	arg->pos = b.crd;
	++*pos;
	return 1;
}

static void debug_print_arg(struct cmd_unit *u, int last);

static void debug_commands(struct cmd_list *l, struct gn_sym_tbl *tb)
{
	struct cmd_list_el *tmp;
	if (dbg_commands == 0)
		return;
	eprintf("--Commands--\n");
	for (tmp = l->first; tmp; tmp = tmp->next) {
		int i;
		if (tmp->cmd.ret_var.type == 'i')
			eprintf("  [%lld]\t", tmp->cmd.ret_var.id);
		else
			eprintf("  \t");

		if (tmp->cmd.type != cmd_funccall) {
			eprintf("%s(%s):\t", LNAME(tmp->cmd.type + 1000, 0, NULL),
					tmp->cmd.pat);
		} else {
			eprintf("%s(%s):\t", tb->vec[tmp->cmd.fid].name, tmp->cmd.pat);
		}

		for (i = 0; i < tmp->cmd.argnum; ++i) {
			debug_print_arg(tmp->cmd.args + i,
					i + 1 == tmp->cmd.argnum);
		}
		eprintf("\n");
	}
}

static void debug_print_arg(struct cmd_unit *u, int last)
{
	switch(u->type) {
	case 'n':
		eprintf("%lld", u->id);
		break;
	case 'i':
		eprintf("[%lld]", u->id);
		break;
	}
	eprintf(!last ?  ", " : "");
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
		acheck_args(&table, tmp->cmd.args, tmp->cmd.argnum);
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

static char *tv_check_args(struct command *c);
static void tv_check_ret(char rtype, struct command *c);

static void type_validity_check(struct cmd_list *l, struct fcall_list *fcl)
{
	struct cmd_list_el *tmp;
	for (tmp = l->first; tmp; tmp = tmp->next) {
		if (tmp->cmd.type != cmd_funccall) {
			char *template = tv_check_args(&tmp->cmd);
			tv_check_ret(template[0], &tmp->cmd);
			free(tmp->cmd.pat);
		} else {
			fcall_list_add(fcl, tmp->cmd.fid, tmp->cmd.pat, &tmp->cmd.pos);
		}
	}
}

static char *tv_check_args(struct command *c)
{
	char **tmp = command_patterns[c->type];
	int i;
	for (i = 0; tmp[i]; ++i) {
		if (strcmp(tmp[i] + 2, c->pat+2) == 0)
			return tmp[i];
	}
	die("%d: %s - invalid pattern (%s)\n", c->pos.row,
			LNAME(c->type + 1000, 0, NULL), c->pat);
	return NULL;
}

static void tv_check_ret(char rtype, struct command *c)
{
	if (rtype == 'i' && c->ret_var.type == 'v')
		warn("%d,%d: %s - return value ignored\n", c->pos.row, c->pos.col,
				LNAME(c->type + 1000, 0, NULL));
	if (rtype == 'v' && c->ret_var.type == 'i')
		die("%d,%d: %s - void assignment\n", c->pos.row, c->pos.col,
				LNAME(c->type + 1000, 0, NULL));
}

static int not_set(struct gn_sym_tbl *tb, int id);
static void set(struct gn_sym_tbl *tb, int id, struct coord *pos,
		char *pat);
static void check(struct fcall_list_el *t, struct gn_tbl_entry *e);
static void fcall_list_free(struct fcall_list *l);

void check_functions(struct gn_sym_tbl *tb, struct fcall_list *fcl)
{
	struct fcall_list_el *tmp;
	for (tmp = fcl->first; tmp; tmp = tmp->next) {
		if (not_set(tb, tmp->fid))
			set(tb, tmp->fid, &tmp->pos, tmp->pattern);
		else
			check(tmp, tb->vec + tmp->fid);
	}

	if (dbg_free_all_mem)
		fcall_list_free(fcl);
}

static int not_set(struct gn_sym_tbl *tb, int id)
{
	return tb->vec[id].info == 0;
}

static void set(struct gn_sym_tbl *tb, int id, struct coord *pos, char *pat)
{
	tb->vec[id].pos = *pos;
	tb->vec[id].type_pattern = strdup(pat);
}

static void check(struct fcall_list_el *t, struct gn_tbl_entry *e)
{
	if (strcmp(t->pattern + 2, e->type_pattern + 2) != 0) {
		die("type mismatch: %s[%d,%d] (%s) with (%s) (in [%d, %d])\n",
				e->name, t->pos.col, t->pos.col, t->pattern,
				e->type_pattern, e->pos.col, e->pos.row);
	}

	if (t->pattern[0] == 'i' && e->type_pattern[0] == 'v' &&
			e->info & TBL_DEF_HERE) {
		die("return type mismatch: %s[%d,%d] (%s) with (%s) (in [%d, %d])\n",
				e->name, t->pos.col, t->pos.col, t->pattern,
				e->type_pattern, e->pos.col, e->pos.row);
	}
	if (t->pattern[0] == 'i' && e->type_pattern[0] == 'v') {
		free(e->type_pattern);
		e->type_pattern = t->pattern;
		t->pattern = NULL;
	}
}

void cmd_list_free(struct cmd_list *l)
{
	struct cmd_list_el *tmp;
	while (l->first) {
		tmp = l->first;
		l->first = tmp->next;

		if (tmp->cmd.argnum > 0)
			free(tmp->cmd.args);
		free(tmp);
	}
	free(l);
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

void fcall_list_add(struct fcall_list *l, int fid, char *pat,
		struct coord *pos)
{
	struct fcall_list_el *tmp = smalloc(sizeof(struct fcall_list_el));
	tmp->fid = fid;
	tmp->pos = *pos;
	tmp->pattern = pat;
	tmp->next = NULL;
	if (l->first == NULL)
		l->first = l->last = tmp;
	else
		l->last = l->last->next = tmp;
}

static void fcall_list_free(struct fcall_list *l)
{
	struct fcall_list_el *tmp;
	while (l->first) {
		tmp = l->first;
		l->first = tmp->next;
		if (tmp->pattern)
			free(tmp->pattern);
		free(tmp);
	}
}

void debug_fcall_list(struct fcall_list *l, struct gn_sym_tbl *tb)
{
	struct fcall_list_el *tmp;
	if (dbg_fcall_list == 0)
		return;
	eprintf("\n----Function call list----\n");
	for (tmp = l->first; tmp; tmp = tmp->next) {
		eprintf("\t%s: [%d, %d] %s\n", tb->vec[tmp->fid].name,
				tmp->pos.row, tmp->pos.col, tmp->pattern);
	}
}
