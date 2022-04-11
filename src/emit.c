#include <limits.h>
#include <alloca.h>
#include <assert.h>
#include <stdlib.h>

#include "global.h"
#include "func.h"
#include "alloc.h"
#include "remap.h"
#include "emit.h"

static void asm_push(int op);
static void asm_pop(int op);
static void asm_mov_wbreg(int dest, int src, int breg);
static void asm_mov_num(int dest, long long num);
static void asm_ret();
static void asm_jmp(char *lbl);
static void asm_call(char *func);

static void il_add(int dest, int src1, int src2);

static void shift_rsp(int offset);

static void emit_funccall(struct alloc *a, struct command *c);
static void emit_ccopy(struct alloc *a, struct command *c);
static void emit_cadd(struct alloc *a, struct command *c);
static void emit_cret(struct alloc *a, struct command *c);

static void (*emit_cfunc_array[])(struct alloc *a, struct command *c) = {
	emit_funccall,
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

void emit_gn_specs(struct gn_sym_tbl *tb)
{
	int i;
	for (i = 0; i < tb->len; ++i) {
		int info = tb->vec[i].info;
		if (info & TBL_GLOBAL)
			oprintf("global %s\n", tb->vec[i].name);
	}

	for (i = 0; i < tb->len; ++i) {
		int info = tb->vec[i].info;
		if ((info & TBL_DEF_HERE) == 0)
			oprintf("extern %s\n", tb->vec[i].name);
	}
	oprintf("\n");
}

static void emit_head(char *name, int offset)
{
	int r;
	oprintf("%s:\n", name);

	asm_push(rbx);
	for (r = r12; r <= r15; ++r)
		asm_push(r);
	asm_push(rbp);

	asm_mov_wbreg(rbp, rsp, rbp);

	if (offset != 0)
		shift_rsp(-offset);
}

static void emit_tail()
{
	int r;
	oprintf("%s:\n", lbl_func_end);
	asm_mov_wbreg(rsp, rbp, rbp);

	asm_pop(rbp);
	for (r = r15; r >= r12; --r)
		asm_pop(r);
	asm_pop(rbx);

	asm_ret();
	oprintf("\n");
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
	prev_offset = tmp->stid;

	assert(vs->curr->next);
	vs->curr = tmp->next;
	free(tmp);
	asm_mov_wbreg(vs->curr->stid, prev_offset, rbp);
}

#define ARG(n) a->vec[c->args[(n)].id].curr
#define RET a->vec[c->ret_var.id].curr

struct args_stack {
	int offset;
	int stack[func_max_args];
};

static void astack_add(struct args_stack *ast, int loc);
static int astack_get(struct args_stack *ast, int *loc);

static void push_func_args(struct alloc *a, struct command *c,
		struct args_stack *ast, int *offset);

static void emit_funccall(struct alloc *a, struct command *c)
{
	struct args_stack ast;
	int offset = 0;
	int r;

	ast.offset = -1;
	for (r = registers_number - 1; r >= 0; --r) {
		if (r == rbx || (r >= r12 && r <= r15))
				continue;
		asm_push(r);
	}

	push_func_args(a, c, &ast, &offset);
	asm_call(c->fname);
	shift_rsp(-offset);

	for (r = 0; r <= registers_number - 1; ++r) {
		if (r == rbx || (r >= r12 && r <= r15))
				continue;
		asm_pop(r);
	}
}

static void push_from_stack(struct args_stack *ast);

static void push_func_args(struct alloc *a, struct command *c,
		struct args_stack *ast, int *offset)
{
	int avail_regs = MIN(6, registers_number);
	int aregsc = 0;
	int i;
	for (i = 0; i < c->argnum; ++i) {
		int id = ARG(i)->stid;
		int src_pos = is_reg(id) ? mem + id * 8 : id;

		if (aregsc < avail_regs) {
			if (id == rbx || (id >= r10 && id <= 15))
				asm_mov_wbreg(aregsc, id, rbp);
			else
				asm_mov_wbreg(aregsc, src_pos, rsp);
			++aregsc;
		} else {
			*offset += 8;
			astack_add(ast, src_pos);
		}
	}
	push_from_stack(ast);
}

static void push_from_stack(struct args_stack *ast)
{
	int loc;
	while (astack_get(ast, &loc)) {
		asm_push(loc);
	}
}

static void astack_add(struct args_stack *ast, int loc)
{
	if (ast->offset == func_max_args - 1)
		die("internal error\n");
	ast->stack[++ast->offset] = loc;
}

static int astack_get(struct args_stack *ast, int *loc)
{
	if (ast->offset == -1)
		return 0;
	*loc = ast->stack[ast->offset--];
	return 1;
}

static void emit_ccopy(struct alloc *a, struct command *c)
{
	if (c->ret_var.type == 'v' || c->ret_var.id == c->args[0].id)
		return;

	if (c->args[0].type == 'n')
		asm_mov_num(RET->stid, c->args[0].id);
	else
		asm_mov_wbreg(RET->stid, ARG(0)->stid, rbp);
}

static void emit_cadd(struct alloc *a, struct command *c)
{
	il_add(RET->stid, ARG(0)->stid, ARG(1)->stid);
}

static void emit_cret(struct alloc *a, struct command *c)
{
	if (c->argnum == 1)
		asm_mov_wbreg(rax, ARG(0)->stid, rbp);
	asm_jmp(lbl_func_end);
}

static void asm_push(int op)
{
	oprintf("\tpush %s\n", OPS(op, rbp));
}

static void asm_pop(int op)
{
	oprintf("\tpop %s\n", OPS(op, rbp));
}

static void asm_mov_num(int dest, long long src);

static void asm_mov_wbreg(int dest, int src, int breg)
{
	if (dest == src)
		return;

	if (is_reg(dest) || is_reg(src)) {
		oprintf("\tmov %s, %s\n", OPS(dest, breg), OPS(src, breg));
	} else {
		oprintf("\tmov %s, %s\n", OPS(rax, breg), OPS(src, breg));
		oprintf("\tmov %s, %s\n", OPS(dest, breg), OPS(rax, breg));
	}
}

static void asm_mov_num(int dest, long long num)
{
	if (num > INT_MAX || num < INT_MIN) {
		/* nasm can handle this */
		oprintf("\tmov %s, %d\n", OPS(rax, rbp), num);
		oprintf("\tmov %s, %s\n", OPS(dest, rbp), OPS(rax, rbp));
	} else {
		if (is_reg(dest))
			oprintf("\tmov %s, %d\n", OPS(dest, rbp), num);
		else
			oprintf("\tmov dword %s, %d\n", OPS(dest, rbp), num);
	}
}

static void shift_rsp(int offset)
{
	oprintf("\t%s %s, %d\n", offset > 0 ? "add" : "sub",
			OPS(rsp, -1), abs(offset));
}

static void asm_jmp(char *lbl)
{
	oprintf("\tjmp %s\n", lbl);
}

static void asm_ret()
{
	oprintf("\tret\n");
}

static void asm_call(char *func)
{
	oprintf("\tcall %s\n", func);
}

static void il_add(int dest, int src1, int src2)
{
	if (is_reg(dest) || (is_reg(src1) && is_reg(src2))) {
		asm_mov_wbreg(dest, src1, rbp);
		oprintf("\tadd %s, %s\n", OPS(dest, rbp), OPS(src2, rbp));
	} else {
		asm_mov_wbreg(rax, src1, rbp);
		oprintf("\tadd %s, %s\n", OPS(rax, rbp), OPS(src2, rbp));
		asm_mov_wbreg(dest, rax, rbp);
	}
}
