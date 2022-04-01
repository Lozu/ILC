#include <limits.h>
#include <alloca.h>
#include <assert.h>
#include <stdlib.h>

#include "global.h"
#include "func.h"
#include "alloc.h"
#include "emit.h"

static void emit_ccopy(struct alloc *a, struct command *c);
static void emit_cadd(struct alloc *a, struct command *c);
static void emit_cret(struct alloc *a, struct command *c);

void (*emit_cfunc_array[])(struct alloc *a, struct command *c) = {
	emit_ccopy,
	emit_cadd,
	emit_cret
};


static void emit_head(char *f, int offset);
static void emit_tail();
static void move(int pos, struct alloc *a, struct command *c);

void asm_emit(struct function *f)
{
	int pos = 1;
	struct cmd_list_el *tmp;

	emit_head(f->name, f->alloc_table->st_offset);

	for (tmp = f->cl->first; tmp; tmp = tmp->next, ++pos) {
		if (dbg_emit_borders)
			oprintf("; @cmd %d\n", pos);
		move(pos, f->alloc_table, &tmp->cmd);
		emit_cfunc_array[tmp->cmd.type](f->alloc_table, &tmp->cmd);
	}

	emit_tail();
}

static void emit_head(char *name, int offset)
{
	oprintf("global %s\n", name);
	oprintf("%s:\n", name);
	oprintf("\tpush rbx\n");
	oprintf("\tpush r12\n");
	oprintf("\tpush r13\n");
	oprintf("\tpush r14\n");
	oprintf("\tpush r15\n");

	oprintf("\tpush rbp\n");
	oprintf("\tmov rbp, rsp\n");
	if (offset != 0)
		oprintf("\tsub rsp, %d\n", -offset);
}

static void emit_tail()
{
	oprintf("%s:\n", lbl_func_end);
	oprintf("\tmov rsp, rbp\n");
	oprintf("\tpop rbp\n");

	oprintf("\tpop r15\n");
	oprintf("\tpop r14\n");
	oprintf("\tpop r13\n");
	oprintf("\tpop r12\n");
	oprintf("\tpop rbx\n");
	oprintf("\tret\n");
}

static void move_single(int pos, struct alloc *a, struct cmd_unit *u);

static void move(int pos, struct alloc *a, struct command *c)
{
	int i;
	for (i = 0; i < c->argnum; ++i)
		move_single(pos, a, c->args + i);
	move_single(pos, a, &c->ret_var);
}

static void move_single(int pos, struct alloc *a, struct cmd_unit *u)
{
	struct var_state *vs;
	struct alloc_phase *tmp;
	int prev_offset;

	if (u->type != 'i')
		return;

	vs = a->vec + u->id;
	if (vs->curr->end >= pos)
		return;

	tmp = vs->curr;
	prev_offset = tmp->offset;

	assert(vs->curr->next);
	vs->curr = tmp->next;
	free(tmp);
	oprintf("\tmov %s, [rsp%+d]\n", reg_names[vs->curr->reg], prev_offset);
}


static char *state_get(char *buf, struct alloc_phase *a)
{
	if (a->type == at_reg)
		return reg_names[a->reg];
	if (a->type == at_mem)
		sprintf(buf, "[rsp%+d]", a->offset);
	return buf;
}

#define STATE_GET(a) ({ char buf[60]; state_get(buf, (a)); })

#define ARG0 a->vec[c->args[0].id].curr
#define ARG1 a->vec[c->args[1].id].curr
#define RET a->vec[c->ret_var.id].curr

#define ARG0_ST STATE_GET(ARG0)
#define ARG1_ST STATE_GET(ARG1)
#define RET_ST STATE_GET(RET)

static void emit_ccopy(struct alloc *a, struct command *c)
{
	if (c->ret_var.type == 'v')
		return;
	if (c->ret_var.id == c->args[0].id)
		return;
	if (c->args[0].type == 'n') {
		oprintf("\tmov %s, %d\n", RET_ST, c->args[0].id);
	} else {
		oprintf("\tmov %s, %s\n", RET_ST, ARG0_ST);
	}
}

static void emit_cadd(struct alloc *a, struct command *c)
{
	if (RET->type == at_mem && (ARG0->type == at_mem ||
				ARG1->type == at_mem)) {
		oprintf("\tmov %s, %s\n", reg_names[rax], ARG0_ST);
		oprintf("\tadd %s, %s\n", reg_names[rax], ARG1_ST);
		oprintf("\tmov %s, %s\n", RET, reg_names[rax]);
	}
	oprintf("\tmov %s, %s\n", RET_ST, ARG0_ST);
	oprintf("\tadd %s, %s\n", RET_ST, ARG1_ST);
}

static void emit_cret(struct alloc *a, struct command *c)
{
	if (c->args[0].type == 'i')
		oprintf("\tmov %s, %s\n", reg_names[rax], ARG0_ST);
	oprintf("\tjmp %s\n", lbl_func_end);
}
