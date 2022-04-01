#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "cmdargs.h"
#include "lparse.h"
#include "remap.h"
#include "func.h"
#include "alloc.h"
#include "emit.h"
#include "process.h"

static int process_entry(FILE *res, struct lexem_list *l);

void process(struct settings *sts)
{
	struct lexem_list *llist = lexem_parse(sts->input_file);
	output_file = fopen(sts->output_file, "w");
	if (output_file == NULL)
		die("%s: %s\n", sts->output_file, strerror(errno));
	while (process_entry(output_file, llist));
}

static void process_function(struct lexem_list *l, FILE *res);

static int process_entry(FILE *res, struct lexem_list *l)
{
	struct lexem_block b;

	if (ll_get(l, &b) == 0)
		return 0;

	switch(b.lt) {
	case lx_new_line:
		break;
	case lx_func_decl:
		process_function(l, res);
		break;
	default:
		die("Not a function\n");
	}
	return 1;
}

static void process_function(struct lexem_list *l, FILE *res)
{
	struct function f;
	struct lexem_list *funcl = ll_extract_upto_lt(l, lx_close_brace);
	struct lexem_list *rnml = remap(funcl, &f.stb);

	func_header_form(rnml, &f);
	cmd_form(rnml, &f);

	f.alloc_table = allocate(&f);
	asm_emit(&f);
}
