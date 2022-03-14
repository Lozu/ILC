#ifndef RENAME_H
#define RENAME_H

#include "lparse.h"

struct tbl_entry {
	char *name;
};

struct sym_tbl {
	int var_arr_len;
	struct tbl_entry *var_array;
};

struct lexem_list *remap(struct lexem_list *l, struct sym_tbl *tb);

#endif
