#ifndef ALLOC_H
#define ALLOC_H

enum {
	rsi,
	rdi,
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
	registers_number = 3
};

extern const char *reg_names[];

enum alloc_type {
	at_mem,
	at_reg
};

struct alloc_phase {
	enum alloc_type type;
	int start;
	int end;

	union {
		int reg;
		int offset;
	};
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

#endif
