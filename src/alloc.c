#include <stdio.h>
#include <string.h>

#include "global.h"
#include "func.h"
#include "alloc.h"

struct livespan {
	int id;
	int start;
	int end;
};

struct lives {
	int num;
	struct livespan *vec;
};

static void lives_fill(struct lives *ls, int num);
static void calc_livespan(struct cmd_list *cl, int fargs, int num,
		struct lives *ls);

void allocate(struct cmd_list *cl, int fargs, int num)
{
	struct lives ls;
	lives_fill(&ls, num);
	calc_livespan(cl, fargs, num, &ls);
}

static void lives_fill(struct lives *ls, int num)
{
	int i;
	ls->num = num;
	ls->vec = smalloc(num * sizeof(struct livespan));
	for (i = 0; i < num; ++i) {
		ls->vec[i].id = i;
		ls->vec[i].end = ls->vec[i].start = -1;
	}
}

static void process_args(struct lives *ls, int pos,
		struct cmd_unit *args, int anum);
static void process_ret(struct lives *ls, int pos, struct cmd_unit *ret);
static void debug_livespan(struct lives *ls);

static void calc_livespan(struct cmd_list *cl, int fargs, int num,
		struct lives *ls)
{
	struct cmd_list_el *tmp;
	int i;

	for (i = 0; i < fargs; ++i)
		ls->vec[i].start = ls->vec[i].end = 0;

	i = 1;

	for (tmp = cl->first; tmp; tmp = tmp->next, ++i) {
		process_args(ls, i, tmp->cmd.args, tmp->cmd.arg_number);
		if (tmp->cmd.rpat == 'i')
			process_ret(ls, i, &tmp->cmd.ret_var);
	}
	debug_livespan(ls);
}

static void process_args(struct lives *ls, int pos,
		struct cmd_unit *args, int anum)
{
	int i;
	for (i = 0; i < anum; ++i) {
		if (args[i].type == 'i') {
			struct livespan *span = ls->vec + args[i].id;
			if (span->start == -1)
				span->end = span->start = pos;
			else
				span->end = pos;
		}
	}
}

static void process_ret(struct lives *ls, int pos, struct cmd_unit *ret)
{
	struct livespan *span = ls->vec + ret->id;
	if (span->start == -1)
		span->end = span->start = pos;
	else
		span->end = pos;
}

static void debug_livespan(struct lives *ls)
{
	int i;
	if (debug[dbg_livespan] == 0)
		return;
	fprintf(stderr, "---Livespan---\n");
	for (i = 0; i < ls->num; ++i) {
		fprintf(stderr, "\t%d: %d - %d\n", i, ls->vec[i].start,
				ls->vec[i].end);
	}
	fprintf(stderr, "\n");
}
