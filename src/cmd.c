#include <stdlib.h>
#include <stdio.h>

#include "lparse.h"
#include "utils.h"
#include "cmd.h"

enum cmd_type {
	cmd_copy,
	cmd_add,
	cmd_ret
};

enum unit_type {
	ut_number,
	ut_var
};

struct cmd_unit {
	enum unit_type type;
	int id;
	struct coord pos;
};

struct command {
	enum cmd_type cmd;
	struct coord pos;

	enum type rv;
	struct cmd_unit ret_var;

	int arg_number;
	struct cmd_unit *args;
};

struct cmd_list_el {
	struct command cmd;
	struct cmd_list_el *next;
};

struct cmd_list {
	struct cmd_list_el *first;
	struct cmd_list_el *last;
};

const char *msg_too_many_args = "too many arguments\n";

struct cmd_list *cmd_list_init();
void cmd_list_add(struct cmd_list *l, struct command *c);
int cmd_list_get(struct cmd_list *l, struct command *c);

static void cmd_form_precheck(struct lexem_list *l, struct function *f);

void cmd_form(struct lexem_list *l, struct function *f)
{
	cmd_form_precheck(l, f);
}

static int cmd_extract(struct lexem_list *l, struct cmd_list *cl);

static void cmd_form_precheck(struct lexem_list *l, struct function *f)
{
	struct cmd_list *cl = cmd_list_init();
	while (cmd_extract(l, cl));
}

static void eat_args(struct lexem_list *l, struct command *c);

static int cmd_extract(struct lexem_list *l, struct cmd_list *cl)
{
	struct command cmd;
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
			cmd.rv = int_t;
			cmd.ret_var.id = b.dt.number;
			cmd.ret_var.pos = b.crd;
			cmd.ret_var.type = ut_var;
			lexem_clever_get(l, &b, vec2, 1);
			lexem_clever_get(l, &b, vec3, 1);
		case 3:
			cmd.cmd = b.lt - 1000;
			cmd.pos = b.crd;
			break;
	}
	eat_args(l, &cmd);
	return 1;
}

static int eat_arg(struct lexem_list *l, struct cmd_unit *arg);

static void eat_args(struct lexem_list *l, struct command *c)
{
	struct cmd_unit argbuf[128];
	int argbuf_pos = 0;
	int i;
	for (; argbuf_pos < 128; ++argbuf_pos) {
		if (eat_arg(l, argbuf + argbuf_pos) == 0) {
			++argbuf_pos;
			break;
		}
	}
	if (argbuf_pos == 128)
		die("%d: %s\n", c->pos.row, msg_too_many_args);
	for (i = 0; i < argbuf_pos; ++i)
		printf("%d:", argbuf[i].id);
	printf("\n");
}

static int eat_arg(struct lexem_list *l, struct cmd_unit *arg)
{
	enum lexem_type vec1[2] = { lx_var_remapped, lx_number };
	enum lexem_type vec2[2] = { lx_coma, lx_new_line };
	int status;
	struct lexem_block b;
	status = lexem_clever_get(l, &b, vec1, 2);
	if (status == 0)
		arg->type = ut_var;
	else
		arg->type = ut_number;
	arg->id = b.dt.number;
	arg->pos = b.crd;
	status = lexem_clever_get(l, &b, vec2, 2);
	return status == 0;
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

int cmd_list_get(struct cmd_list *l, struct command *c)
{
	struct cmd_list_el *tmp = l->first;
	if (l->first == NULL)
		return 0;
	if (l->first == l->last)
		l->last = NULL;
	l->first = l->first->next;
	*c = tmp->cmd;
	free(tmp);
	return 1;
}

