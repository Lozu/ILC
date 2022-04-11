#ifndef LPARSE_H
#define LPARSE_H

#include <stdio.h>
#include <alloca.h>

enum lexem_type {
	lx_word,
	lx_number,
	lx_global_name,
	lx_variable,
	lx_var_remapped,
	lx_gn_rmp,

	lx_new_line,
	lx_parenleft,
	lx_parenright,
	lx_coma,
	lx_open_brace,
	lx_close_brace,
	lx_equal_sign,

	lx_global_spec,
	lx_func_decl,
	lx_int_spec,

	lx_cmd = 999,
	lx_cmd_copy = 1001,
	lx_cmd_add,
	lx_cmd_ret
};

enum {
	max_lexem_len = 32,
	lexem_alloca_size = 50 + max_lexem_len
};

struct coord {
	int row;
	int col;
};

union lexem_data {
	char *str_value;
	int number;
};

struct lexem_block {
	enum lexem_type lt;
	struct coord crd;
	union lexem_data dt;
};

struct lexem_list;

struct lexem_list *lexem_parse(char *filename);

struct lexem_list *ll_init();
void ll_add(struct lexem_list *ll, struct lexem_block *b);
int ll_get(struct lexem_list *ll, struct lexem_block *b);
int lexem_clever_get(struct lexem_list *l, struct lexem_block *b,
		enum lexem_type *ltvec, int ltvec_len);
struct lexem_list *ll_extract_upto_lt(struct lexem_list *l,
		enum lexem_type lt);
void ll_print(FILE *f, struct lexem_list *ll);
char *lexem_name(char *buf, enum lexem_type lt, int v,
		union lexem_data *dt);

#define LNAME(lt, v, dt) ({ char *buf = alloca(lexem_alloca_size); \
	lexem_name(buf, lt, v, dt); })

#endif
