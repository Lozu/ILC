#ifndef ALLOC_H
#define ALLOC_H

enum {
	rdi,
	rsi,
	rdx,
	rcx,
	r8,
	r9,

	rbx,
	r10,
	r11,
	r12,
	r13,
	r14,
	r15,

	rax, /* Temporary register */
	rsp,
	rbp,

	registers_number = rax - 1,
	mem = (1 << 30),
	op_sbuf_len = 20
};

static inline int is_reg(int val)
{
	return val < 1000;
}

static inline int is_mem(int val)
{
	return val >= 1000;
}

struct alloc_phase {
	int start;
	int end;
	int stid; /* if < 1000 then reg=& else memory=&-mem */
	struct alloc_phase *next;
};

struct var_state {
	struct alloc_phase *curr;
	struct alloc_phase *last;
};

struct alloc {
	struct var_state *vec;
	int st_offset;
	int len;
};

struct function;

struct alloc *allocate(struct function *f);
void alloc_free(struct alloc *a);

#define OPS(op, br) ({ char *buf = alloca(op_sbuf_len); op_string((op), buf, (br)); })
char *op_string(int op, char *buf, int basereg);

#endif
