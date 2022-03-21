#include <stdlib.h>
#include <stdio.h>

#include "utils.h"
#include "lparse.h"
#include "func.h"

static const char *func_arg_dup = "function argument duplicate";
static const char *func_too_many_args = "too many arguments";

static void eat_args(struct lexem_list *l, struct function *f);

void func_header_form(struct lexem_list *l, struct function *f)
{
	struct lexem_block b;
	int status;
	enum lexem_type ltvec1[2] = { lx_int_spec, lx_func_name };
	enum lexem_type ltvec2[1] =  { lx_func_name };
	enum lexem_type ltvec3[1] = { lx_parenleft };
	enum lexem_type ltvec4[1] = { lx_open_brace };

	f->name = NULL;
	status = lexem_clever_get(l, &b, ltvec1, 2);
	if (status == 0) {
		f->rt = int_t;
		lexem_clever_get(l, &b, ltvec2, 1);
		f->name = b.dt.str_value;
	} else {
		f->rt = void_t;
		f->name = b.dt.str_value;
	}
	lexem_clever_get(l, &b, ltvec3, 1);
	eat_args(l, f);
	lexem_clever_get(l, &b, ltvec4, 1);
	printf("Header: %d, [%s], %d\n", f->rt, f->name, f->arg_number);
}

static int eat_arg(struct lexem_list *l, int anum, struct function *f);

static void eat_args(struct lexem_list *l, struct function *f)
{
	int arg_number = 0;
	for (; arg_number < func_max_arg_number; ++arg_number) {
		if (eat_arg(l, arg_number, f) == 0) {
			++arg_number;
			break;
		}
	}
	if (arg_number == func_max_arg_number) {
		die("%s:[%d, %d]: %s\n", f->name, f->pos.row, f->pos.col,
				func_too_many_args);
	}
	f->arg_number = arg_number;
}

static int eat_arg(struct lexem_list *l, int anum, struct function *f)
{
	int status;
	struct lexem_block b;
	enum lexem_type vec1[2] = { lx_new_line, lx_var_remapped };
	enum lexem_type vec2[2] = { lx_parenright, lx_coma };
	status = lexem_clever_get(l, &b, vec1, 2);
	if (status == 0)
		return eat_arg(l, anum, f);
	if (b.dt.number != anum) {
		die(" function \'%s\': variable \'%s\'[%d,%d]: %s\n", f->name,
				f->stb.var_array[b.dt.number], b.crd.row, b.crd.col,
				func_arg_dup);
	}
	return lexem_clever_get(l, &b, vec2, 2);
}
