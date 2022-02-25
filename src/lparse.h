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

	word,
	func_decl,
	int_spec,
	cmd_add,
	cmd_copy,
	cmd_ret
};

struct lexem_list;

struct lexem_list *lexem_parse(char *filename);

#endif
