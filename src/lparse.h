#ifndef LPARSE_H
#define LPARSE_H

enum lexem_type {
	new_line,
	parenleft,
	parenright,
	coma,
	variable,
	open_brace,
	close_brace,
	number,
	equal_sign,
	func_name,

	var_remapped,

	word,
	func_decl,
	int_spec,
	cmd_add,
	cmd_copy,
	cmd_ret
};

struct coord {
	int row;
	int col;
};

union lexem_data {
	char *str_value;
	int number;
};


struct lexem_list;

struct lexem_list *lexem_parse(char *filename);

struct lexem_list *ll_init();
void ll_add(struct lexem_list *ll, enum lexem_type lt,
		struct coord *cor, union lexem_data *dt);
int ll_get(struct lexem_list *ll, enum lexem_type *lt,
		struct coord *cor, union lexem_data *dt);
struct lexem_list *ll_extract_upto_lt(struct lexem_list *l,
		enum lexem_type lt);
void ll_print(struct lexem_list *ll);

#endif
