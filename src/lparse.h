#ifndef LPARSE_H
#define LPARSE_H

enum lexem_type {
	lx_new_line,
	lx_parenleft,
	lx_parenright,
	lx_coma,
	lx_variable,
	lx_var_remapped,
	lx_open_brace,
	lx_close_brace,
	lx_number,
	lx_equal_sign,
	lx_func_name,

	lx_word,
	lx_func_decl,
	lx_int_spec,

	lx_cmd_copy = 1000,
	lx_cmd_add,
	lx_cmd_ret
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
void ll_print(struct lexem_list *ll);

#endif
