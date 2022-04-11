#ifndef PROCESS_H
#define PROCESS_H

#include "lparse.h"
#include "remap.h"
#include "cmdargs.h"
#include "func.h"

struct func_list_el {
	struct function *f;
	struct func_list_el *next;
};

struct func_list {
	struct func_list_el *first;
	struct func_list_el *last;
};

struct state {
	struct lexem_list *l;
	struct gn_sym_tbl global_tbl;
	struct func_list fl;
	struct fcall_list fcl;
};

void process(struct settings *sts);

#endif
