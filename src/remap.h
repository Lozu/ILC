#ifndef RENAME_H
#define RENAME_H

#include "lparse.h"

struct var_tbl_entry {
	char *name;
};

struct var_sym_tbl {
	int len;
	struct var_tbl_entry *vec;
};

#define TBL_FUNC_MASK 1
#define TBL_DATA_MASK 2
#define TBL_GLOBAL 4
#define TBL_EXTERNAL 8

struct gn_tbl_entry {
	char *name;
	int info;
	char *type_pattern;
};

struct gn_sym_tbl {
	int len;
	struct gn_tbl_entry *vec;
};

struct lexem_list * remap_gns(struct lexem_list *l, struct gn_sym_tbl *tb);
struct lexem_list * remap_vars(struct lexem_list *l,
		struct var_sym_tbl *tb);
void var_sym_tbl_free(struct var_sym_tbl *s);
void gn_sym_tbl_free(struct gn_sym_tbl *t);

#endif
