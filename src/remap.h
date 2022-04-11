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

#define TBL_FUNC 1
#define TBL_DEF_HERE 2
#define TBL_GLOBAL 4

struct gn_tbl_entry {
	char *name;
	struct coord pos;
	int info;
	char *type_pattern;
};

struct gn_sym_tbl {
	int len;
	struct gn_tbl_entry *vec;
};

struct lexem_list *remap_gns(struct lexem_list *l, struct gn_sym_tbl *tb);
struct lexem_list *remap_vars(struct lexem_list *l,
		struct var_sym_tbl *tb);
void debug_gn_sym_tbl_pre(struct gn_sym_tbl *tb);
void debug_gn_sym_tbl_post(struct gn_sym_tbl *tb);
void debug_var_sym_tbl(struct var_sym_tbl *tb, struct lexem_list *l);
void var_sym_tbl_free(struct var_sym_tbl *s);
void gn_sym_tbl_free(struct gn_sym_tbl *t);

#endif
