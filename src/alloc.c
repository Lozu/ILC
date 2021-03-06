#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "func.h"
#include "alloc.h"

typedef unsigned long long ulonglong;

enum {
	dbg_heapsort = 0
};

static const char *reg_names[] = {
	"rdi",
	"rsi",
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
	"rax",
	"rsp",
	"rbp"
};

struct lifespan {
	unsigned int id;
	unsigned int start;
	unsigned int end;
};

struct lifes {
	struct lifespan *vec;
	int len;
	int pos;
};

typedef int(*cmpfunc)(struct lifespan *a, struct lifespan *b);

static inline int memval(int val)
{
	return mem + val;
}

static void alloc_init(struct alloc *a, int num);
static void lifes_fill(struct lifes *ls, int num);
static void lifes_free(struct lifes *l);
static void calc_lifespan(struct cmd_list *cl, int fargs, int num,
		struct lifes *ls);
static void lifes_copy(struct lifes *dest, struct lifes *src);
static void debug_lifespan(struct lifes *ls, struct var_sym_tbl *tb);
static int cmp_for_bystart_smaller(struct lifespan *a, struct lifespan *b);
static int cmp_for_byend_smaller(struct lifespan *a, struct  lifespan *b);
static void heapsort(struct lifes *l, cmpfunc f);
static void real_alloc(struct alloc *a, struct lifes *l);
static void debug_allocation(struct alloc *a, struct var_sym_tbl *tb);

struct alloc *allocate(struct function *f)
{
	struct alloc *ac = smalloc(sizeof(struct alloc));
	struct lifes l;

	alloc_init(ac, f->stb.len);
	lifes_fill(&l, f->stb.len);

	calc_lifespan(f->cl, f->argnum, f->stb.len, &l);
	debug_lifespan(&l, &f->stb);

	real_alloc(ac, &l);
	lifes_free(&l);
	debug_allocation(ac, &f->stb);
	return ac;
}

void alloc_init(struct alloc *a, int num)
{
	int i;
	a->vec = smalloc(num * sizeof(struct var_state));
	a->len = num;
	a->st_offset = 0;
	for (i = 0; i < num; ++i) {
		a->vec[i].curr = a->vec[i].last = NULL;
	}
}

void alloc_free(struct alloc *a)
{
	int i;
	for (i = 0; i < a->len; ++i)
			free(a->vec[i].curr);
	free(a->vec);
	free(a);
}

static void lifes_fill(struct lifes *ls, int num)
{
	int i;
	ls->len = num;
	ls->pos = 0;
	ls->vec = smalloc(num * sizeof(struct lifespan));
	for (i = 0; i < num; ++i) {
		ls->vec[i].id = i;
		ls->vec[i].end = ls->vec[i].start = UINT_MAX;
	}
}

static void lifes_free(struct lifes *l)
{
	free(l->vec);
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
		process_args(ls, i, tmp->cmd.args, tmp->cmd.argnum);
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
			if (span->start == UINT_MAX)
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

static void debug_lifespan(struct lifes *ls, struct var_sym_tbl *tb)
{
	int i;
	if (dbg_lifespan == 0)
		return;
	eprintf("\n--Lifespan--\n");
	for (i = 0; i < ls->len; ++i) {
		eprintf("\t\'%s\': %d - %d\n", tb->vec[i].name,
				ls->vec[i].start, ls->vec[i].end);
	}
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
	eprintf("---Heapsort---\n");
	for (i = 0; i < len; ++i)
		eprintf("\t%d: %d, %d\n", l[i].id, l[i].start, l[i].end);
}

struct reg_stack {
	int pointer;
	char stack[registers_number];
};

struct deprived_unit {
	int id;
	int end;
	struct deprived_unit *prev;
	struct deprived_unit *next;
};

struct deprived_deque {
	struct deprived_unit *first;
	struct deprived_unit *last;
};

static void rstack_init(struct reg_stack *s);
static void rstack_add(struct reg_stack *s, int reg);
static int rstack_get(struct reg_stack *s);

void alloc_ins_var_state(struct alloc *a, int var, int value,
		int start, int end);

static int deprd_fget(struct deprived_deque *d);
static int deprd_bget(struct deprived_deque *d);
static void deprd_ins(struct deprived_deque *d, struct lifespan *l);
static void deprd_free(struct deprived_deque *d);

static const ulonglong bystart_mark = 1ULL << 63;
static const ulonglong byend_mark = 1ULL << 62;

static int resolve_args(struct alloc *a, struct reg_stack *rs,
		struct lifes *tbl, struct deprived_deque *d);
static ulonglong getpos(struct lifes *bystart, struct lifes *byend);
static void discard_inneed_dead(struct deprived_deque *dq, int start);
static void	discard_dead(struct lifes *l, int start,
		struct alloc *a, struct reg_stack *rs);
static void alloc_var(struct lifes *l, struct alloc *a, int *offset,
		struct reg_stack *rs, struct deprived_deque *d, int start);
static void give_to_in_need(struct deprived_deque *d, int start,
		struct reg_stack *rs, struct alloc *a, struct lifes *tbl);

static void real_alloc(struct alloc *a, struct lifes *l)
{
	struct reg_stack rs;
	ulonglong pos;
	struct deprived_deque dq = { NULL, NULL };
	struct lifes bystart;
	struct lifes byend;

	lifes_copy(&bystart, l);
	lifes_copy(&byend, l);
	heapsort(&bystart, cmp_for_bystart_smaller);
	heapsort(&byend, cmp_for_byend_smaller);

	rstack_init(&rs);
	bystart.pos = resolve_args(a, &rs, l, &dq);

	while ((pos = getpos(&bystart, &byend)) != LLONG_MAX) {
		discard_inneed_dead(&dq, pos);

		if (pos & byend_mark)
			discard_dead(&byend, pos, a, &rs);

		if (pos & bystart_mark)
			alloc_var(&bystart, a, &a->st_offset, &rs, &dq, pos);

		give_to_in_need(&dq, pos, &rs, a, l);
	}
	lifes_free(&bystart);
	lifes_free(&byend);
	deprd_free(&dq);
}

static ulonglong getpos(struct lifes *bystart, struct lifes *byend)
{
	unsigned long long min = LLONG_MAX;
	int mask = 0;

	if (bystart->pos < bystart->len &&
			bystart->vec[bystart->pos].start < min) {
		min = bystart->vec[bystart->pos].start;
		mask |= 1;
	}
	if (byend->pos < byend->len &&
			byend->vec[byend->pos].end < min &&
			byend->vec[byend->pos].end != byend->vec[byend->len-1].end
			) {
		min = byend->vec[byend->pos].end + 1;
		mask |= 2;
	}
	if (mask & 1)
		min |= bystart_mark;
	if (mask & 2)
		min |= byend_mark;
	return min;
}

static int resolve_args(struct alloc *a, struct reg_stack *rs,
		struct lifes *tbl, struct deprived_deque *d)
{
	int offset = 8;
	int i;
	for (i = 0; i < tbl->len && tbl->vec[i].start == 0; ++i) {
		int reg;
		if (i < 6 && (reg = rstack_get(rs)) != -1) {
			alloc_ins_var_state(a, i, reg, 0, tbl->vec[i].end);
		} else {
			alloc_ins_var_state(a, i, memval(offset += 8),
					0, tbl->vec[i].end);
			deprd_ins(d, tbl->vec + i);
		}
	}
	return i;
}

static void discard_inneed_dead(struct deprived_deque *dq, int start)
{
	while (dq->first && dq->first->end < start)
		deprd_fget(dq);
}


static void	discard_dead(struct lifes *l, int start,
		struct alloc *a, struct reg_stack *rs)
{
	for (; l->pos < l->len && l->vec[l->pos].end < start; ++l->pos) {
		if (a->vec[l->vec[l->pos].id].last->stid < 1000)
			rstack_add(rs, a->vec[l->vec[l->pos].id].last->stid);
	}
}

static void alloc_var(struct lifes *l, struct alloc *a, int *offset,
		struct reg_stack *rs, struct deprived_deque *d, int start)
{
	for (; l->pos < l->len && l->vec[l->pos].start == start; ++l->pos) {
		struct lifespan *tmp = l->vec + l->pos;
		int reg = rstack_get(rs);
		if (reg != -1) {
			alloc_ins_var_state(a, tmp->id, reg, start, tmp->end);
		} else {
			alloc_ins_var_state(a, tmp->id, memval(*offset -= 8),
					start, tmp->end);
			deprd_ins(d, tmp);
		}
	}
}

static void give_to_in_need(struct deprived_deque *d, int start,
		struct reg_stack *rs, struct alloc *a, struct lifes *tbl)
{
	int reg;
	int id;

	if (d->first == NULL || rs->pointer == -1)
		return;

	id = deprd_bget(d);
	reg = rstack_get(rs);

	a->vec[id].curr->end = start - 1;
	alloc_ins_var_state(a, id, reg, start, tbl->vec[id].end);
	give_to_in_need(d, start, rs, a, tbl);
}

static void rstack_init(struct reg_stack *s)
{
	int i;
	s->pointer = -1;
	for (i = registers_number - 1; i >= 0; --i)
		rstack_add(s, i);
}

static void rstack_add(struct reg_stack *s, int reg)
{
	if (s->pointer == registers_number - 1)
		die("internal error\n");
	s->stack[++s->pointer] = reg;
}

static int rstack_get(struct reg_stack *s)
{
	if (s->pointer == -1)
		return -1;
	return s->stack[s->pointer--];
}

void alloc_ins_var_state(struct alloc *a, int var, int value,
		int start, int end)
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
	tmp->stid = value;
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
	res = tmp->id;
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
	res = tmp->id;
	if (d->last)
		d->last->next = NULL;
	free(tmp);
	return res;
}

static void deprd_ins(struct deprived_deque *d, struct lifespan *l)
{
	struct deprived_unit *tmp = d->last;
	struct deprived_unit *tmp2 = smalloc(sizeof(struct deprived_unit));
	tmp2->id = l->id;
	tmp2->end = l->end;

	for (; tmp && tmp->end > tmp2->end; tmp = tmp->prev);

	if (tmp == NULL) {
		if (d->first == NULL) {
			d->first = d->last = tmp2;
			d->first->prev = d->first->next = NULL;
		} else {
			d->first->prev = tmp2;
			tmp2->next = d->first;
			d->first = tmp2;
		}
	} else {
		tmp2->prev = tmp;
		if (tmp->next == NULL)
			d->last = tmp2;
		else
			tmp->next->prev = tmp2;
		tmp2->next = tmp->next;
		tmp->next = tmp2;
	}
}

static void deprd_free(struct deprived_deque *d)
{
	struct deprived_unit *tmp;
	while (d->first) {
		tmp = d->first;
		d->first = tmp->next;
		free(tmp);
	}
}

char *op_string(int op, char *buf, int basereg)
{
	if (op < 1000) {
		sprintf(buf, reg_names[op]);
	} else {
		op -= mem;
		if (op == 0)
			sprintf(buf, "[%s]", reg_names[basereg]);
		else
			sprintf(buf, "[%s%+d]", reg_names[basereg], op);
	}
	return buf;
}

static void print_alloc_phases(char *var, struct alloc_phase *vp);

static void debug_allocation(struct alloc *a, struct var_sym_tbl *tb)
{
	int i;
	if (dbg_allocation == 0)
		return;
	eprintf("\n--Allocation--\n");
	for (i = 0; i < a->len; ++i) {
		if (a->vec[i].curr == NULL)
			continue;
		print_alloc_phases(tb->vec[i].name, a->vec[i].curr);
	}
}

static void print_alloc_phases(char *var, struct alloc_phase *vp)
{
	eprintf("\t %s| ", var);
	for (; vp; vp = vp->next) {
		eprintf("%s: (%d-%d)", OPS(vp->stid, rbp), vp->start, vp->end);
		eprintf(vp->next ? " -> " : "\n");
	}
}
