#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "func.h"
#include "alloc.h"

enum {
	dbg_heapsort = 0
};

const char *reg_names[] = {
	"rsi",
	"rdi",
	"rdx",
	"rcx",
	"r8",
	"r9",
	"rbx",
	"r10",
	"r11",
	"r12",
	"r13",
	"r14",
	"r15",
	"rax"
};

struct lifespan {
	int id;
	int start;
	int end;
};

struct lifes {
	struct lifespan *vec;
	int len;
	int pos;
};

typedef int(*cmpfunc)(struct lifespan *a, struct lifespan *b);

static void alloc_init(struct alloc *a, int num);
static void lifes_fill(struct lifes *ls, int num);
static void calc_lifespan(struct cmd_list *cl, int fargs, int num,
		struct lifes *ls);
static void lifes_copy(struct lifes *dest, struct lifes *src);
static void debug_lifespan(struct lifes *ls, struct sym_tbl *tb);
static int cmp_for_bystart_smaller(struct lifespan *a, struct lifespan *b);
static int cmp_for_byend_smaller(struct lifespan *a, struct  lifespan *b);
static void heapsort(struct lifes *l, cmpfunc f);
static void real_alloc(struct alloc *a,
		struct lifes *bystart, struct lifes *byend);
static void debug_allocation(struct alloc *a, struct sym_tbl *tb);

void allocate(struct function *f, struct alloc *ac)
{
	struct lifes bystart;
	struct lifes byend;

	alloc_init(ac, f->stb.var_arr_len);

	lifes_fill(&bystart, f->stb.var_arr_len);
	calc_lifespan(f->cl, f->arg_number, f->stb.var_arr_len, &bystart);
	debug_lifespan(&bystart, &f->stb);
	lifes_copy(&byend, &bystart);

	heapsort(&bystart, cmp_for_bystart_smaller);
	heapsort(&byend, cmp_for_byend_smaller);

	real_alloc(ac, &bystart, &byend);
	debug_allocation(ac, &f->stb);
}

void alloc_init(struct alloc *a, int num)
{
	int i;
	a->vec = smalloc(num * sizeof(struct var_state));
	a->len = num;
	for (i = 0; i < num; ++i) {
		a->vec[i].curr = a->vec[i].last = NULL;
	}
}

static void lifes_fill(struct lifes *ls, int num)
{
	int i;
	ls->len = num;
	ls->pos = 0;
	ls->vec = smalloc(num * sizeof(struct lifespan));
	for (i = 0; i < num; ++i) {
		ls->vec[i].id = i;
		ls->vec[i].end = ls->vec[i].start = -1;
	}
}

static void process_args(struct lifes *ls, int pos,
		struct cmd_unit *args, int anum);
static void process_ret(struct lifes *ls, int pos, struct cmd_unit *ret);

static void calc_lifespan(struct cmd_list *cl, int fargs, int num,
		struct lifes *ls)
{
	struct cmd_list_el *tmp;
	int i;

	for (i = 0; i < fargs; ++i)
		ls->vec[i].start = ls->vec[i].end = 0;

	i = 1;

	for (tmp = cl->first; tmp; tmp = tmp->next, ++i) {
		process_args(ls, i, tmp->cmd.args, tmp->cmd.arg_number);
		if (tmp->cmd.ret_var.type == 'i')
			process_ret(ls, i, &tmp->cmd.ret_var);
	}
}

static void process_args(struct lifes *ls, int pos,
		struct cmd_unit *args, int anum)
{
	int i;
	for (i = 0; i < anum; ++i) {
		if (args[i].type == 'i') {
			struct lifespan *span = ls->vec + args[i].id;
			if (span->start == -1)
				span->end = span->start = pos;
			else
				span->end = pos;
		}
	}
}

static void process_ret(struct lifes *ls, int pos, struct cmd_unit *ret)
{
	struct lifespan *span = ls->vec + ret->id;
	if (span->start == -1)
		span->end = span->start = pos;
	else
		span->end = pos;
}

static void lifes_copy(struct lifes *dest, struct lifes *src)
{
	int i;
	*dest = *src;
	dest->vec = smalloc(dest->len * sizeof(struct lifespan));
	for (i = 0; i < dest->len; ++i)
		dest->vec[i] = src->vec[i];
}

static void debug_lifespan(struct lifes *ls, struct sym_tbl *tb)
{
	int i;
	if (debug[dbg_lifespan] == 0)
		return;
	fprintf(stderr, "---Lifespan---\n");
	for (i = 0; i < ls->len; ++i) {
		fprintf(stderr, "\t\'%s\': %d - %d\n", tb->var_array[i].name,
				ls->vec[i].start, ls->vec[i].end);
	}
	fprintf(stderr, "\n");
}

static int cmp_for_bystart_smaller(struct lifespan *a, struct lifespan *b)
{
	return a->start > b->start;
}

static int cmp_for_byend_smaller(struct lifespan *a, struct  lifespan *b)
{
	return a->end > b->end;
}

static void heapify(struct lifespan *vec, int len, int i, cmpfunc f)
{
	int left = 2 * i + 1;
	int right = 2 * i + 2;
	int largest;
	if (left < len && f(vec + left, vec + i))
		largest = left;
	else
		largest = i;
	if (right < len && f(vec + right, vec + largest))
		largest = right;
	if (largest != i) {
		struct lifespan tmp = vec[i];
		vec[i] = vec[largest];
		vec[largest] = tmp;
		heapify(vec, len, largest, f);
	}
}

static void build_heap(struct lifespan *vec, int len, cmpfunc f)
{
	int i;
	for (i = len / 2; i >= 0; --i)
		heapify(vec, len, i, f);
}

static void dbg_int_heapsort(struct lifespan *l, int len);

static void heapsort(struct lifes *l, cmpfunc f)
{
	int i;
	build_heap(l->vec, l->len, f);
	for (i = l->len - 1; i >= 1; --i) {
		struct lifespan tmp = l->vec[i];
		l->vec[i] = l->vec[0];
		l->vec[0] = tmp;
		heapify(l->vec, i, 0, f);
	}
	if (dbg_heapsort)
		dbg_int_heapsort(l->vec, l->len);
}

static void dbg_int_heapsort(struct lifespan *l, int len)
{
	int i;
	fprintf(stderr, "---Heapsort---\n");
	for (i = 0; i < len; ++i)
		fprintf(stderr, "\t%d: %d, %d\n", l[i].id, l[i].start, l[i].end);
}

struct register_stack {
	int reg;
	struct register_stack *next;
};

struct deprived_unit {
	struct lifespan *ls;
	struct deprived_unit *prev;
	struct deprived_unit *next;
};

struct deprived_deque {
	struct deprived_unit *first;
	struct deprived_unit *last;
};

static struct register_stack *register_stack_init();
static struct register_stack *rstack_add(struct register_stack *s, int reg);
int rstack_get(struct register_stack **s);

void alloc_ins_var_state(struct alloc *a, int var,
		enum alloc_type at, int value, int start, int end);

static int deprd_fget(struct deprived_deque *d);
static int deprd_bget(struct deprived_deque *d);
static void deprd_ins(struct deprived_deque *d, struct lifespan *l);

static void resolve_args(struct alloc *a, struct register_stack **rs,
		struct lifes *l, struct deprived_deque *d);
static void discard_inneed_dead(struct deprived_deque *dq, int start);
static void	discard_dead(struct lifes *l, int start,
		struct alloc *a, struct register_stack **rs);
static void give_to_in_need(struct deprived_deque *d, int start,
		struct register_stack **rs, struct alloc *a, struct lifes *tbl);

static void real_alloc(struct alloc *a,
		struct lifes *bystart, struct lifes *byend)
{
	struct register_stack *rs = register_stack_init();
	int stack_offset = 0;
	struct deprived_deque dq = { NULL, NULL };

	resolve_args(a, &rs, bystart, &dq);
	for (; bystart->pos < bystart->len; ++bystart->pos) {
		int start = bystart->vec[bystart->pos].start;
		struct lifespan *nl;
		int reg;

		discard_inneed_dead(&dq, start);
		discard_dead(byend, start, a, &rs);

		nl = bystart->vec + bystart->pos;
		reg = rstack_get(&rs);
		if (reg != -1) {
			alloc_ins_var_state(a, nl->id, at_reg, reg, nl->start, nl->end);
		} else {
			alloc_ins_var_state(a, nl->id, at_mem, stack_offset -= 8,
					nl->start, nl->end);
			deprd_ins(&dq, nl);
		}

		give_to_in_need(&dq, start, &rs, a, bystart);
	}
}

static struct register_stack *register_stack_init()
{
	int i;
	struct register_stack *tmp = NULL;
	for (i = registers_number - 1; i >= 0; --i)
		tmp = rstack_add(tmp, i);
	return tmp;
}

static void resolve_args(struct alloc *a, struct register_stack **rs,
		struct lifes *l, struct deprived_deque *d)
{
	int offset = 8;
	int i;
	for (; l->pos < l->len && l->vec[l->pos].start == 0; ++l->pos);

	for (i = 0; i < l->pos; ++i) {
		int reg;
		if (i < 6 && (reg = rstack_get(rs)) != -1) {
			alloc_ins_var_state(a, i, at_reg,
					reg, 0, l->vec[i].end);
		} else {
			alloc_ins_var_state(a, i, at_mem,
					offset += 8, 0, l->vec[i].end);
			deprd_ins(d, l->vec + l->pos);
		}
	}
}

static void discard_inneed_dead(struct deprived_deque *dq, int start)
{
	while (dq->first && dq->first->ls->end < start)
		deprd_fget(dq);
}


static void	discard_dead(struct lifes *l, int start,
		struct alloc *a, struct register_stack **rs)
{
	for (; l->pos < l->len && l->vec[l->pos].end < start; ++l->pos) {
		struct var_state *vs = a->vec + l->vec[l->pos].id;
		if (vs->last->type == at_reg) {
			*rs = rstack_add(*rs, vs->last->reg);
		}
	}
}

static void give_to_in_need(struct deprived_deque *d, int start,
		struct register_stack **rs, struct alloc *a, struct lifes *tbl)
{
	int reg = rstack_get(rs);
	int id;
	if (reg == -1)
		return;
	id = deprd_bget(d);
	if (id == -1)
		return;
	a->vec[id].curr->end = start - 1;
	alloc_ins_var_state(a, id, at_reg, reg,
			start, tbl->vec[id].end);
	give_to_in_need(d, start, rs, a, tbl);
}

static struct register_stack *rstack_add(struct register_stack *s, int reg)
{
	struct register_stack *tmp = smalloc(sizeof(struct register_stack));
	tmp->next = s;
	tmp->reg = reg;
	return tmp;
}

int rstack_get(struct register_stack **s)
{
	struct register_stack *tmp = *s;
	int res;
	if (*s == NULL)
		return -1;
	res = tmp->reg;
	*s = tmp->next;
	free(tmp);
	return res;
}

void alloc_ins_var_state(struct alloc *a, int var,
		enum alloc_type at, int value, int start, int end)
{
	struct var_state *vs = a->vec + var;
	struct alloc_phase *tmp = smalloc(sizeof(struct alloc_phase));
	tmp->start = start;
	tmp->end = end;
	tmp->next = NULL;
	if (vs->curr == NULL)
		vs->curr = vs->last = tmp;
	else
		vs->last = vs->last->next = tmp;
	tmp->type = at;
	if (at == at_reg)
		tmp->reg = value;
	else
		tmp->offset = value;
}

static int deprd_fget(struct deprived_deque *d)
{
	struct deprived_unit *tmp;
	int res;

	if (d->first == NULL)
		return -1;
	if (d->first == d->last)
		d->last = NULL;
	tmp = d->first;
	d->first = tmp->next;
	res = tmp->ls->id;
	if (d->first)
		d->first->prev = NULL;
	free(tmp);
	return res;
}

static int deprd_bget(struct deprived_deque *d)
{
	struct deprived_unit *tmp;
	int res;
	if (d->first == NULL)
		return -1;
	if (d->first == d->last)
		d->first = NULL;
	tmp = d->last;
	d->last = d->last->prev;
	res = tmp->ls->id;
	if (d->last)
		d->last->next = NULL;
	free(tmp);
	return res;
}

static void deprd_ins(struct deprived_deque *d, struct lifespan *l)
{
	struct deprived_unit *tmp = d->last;
	struct deprived_unit *tmp2 = smalloc(sizeof(struct deprived_unit));
	tmp2->ls = l;

	for (; tmp && tmp->ls->end > l->end; tmp = tmp->prev);

	if (tmp == NULL) {
		d->first = d->last = tmp2;
		d->first->prev = d->first->next = NULL;
	} else {
		tmp2->prev = tmp;
		if (tmp->next == NULL)
			d->last = tmp2;
		else
			tmp->next->prev = tmp2;
		tmp2->next = tmp2->next;
		tmp->next = tmp2;
	}
}

static void print_alloc_phases(char *var, struct alloc_phase *vp);

static void debug_allocation(struct alloc *a, struct sym_tbl *tb)
{
	int i;
	if (debug[dbg_allocation] == 0)
		return;
	fprintf(stderr, "---Allocation---\n");
	for (i = 0; i < a->len; ++i) {
		if (a->vec[i].curr == NULL)
			continue;
		print_alloc_phases(tb->var_array[i].name, a->vec[i].curr);
	}
	fprintf(stderr, "\n");
}

static void print_alloc_phases(char *var, struct alloc_phase *vp)
{
	fprintf(stderr, "\t");
	for (; vp; vp = vp->next) {
		if (vp->type == at_reg) {
			fprintf(stderr, "[\'%s\': (%s) %d - %d]", var,
					reg_names[vp->reg], vp->start, vp->end);
		} else {
			fprintf(stderr, "[\'%s\': (%+d) %d - %d]", var,
					vp->offset, vp->start, vp->end);
		}
		fprintf(stderr, vp->next ? " -> " : "\n");
	}
}
