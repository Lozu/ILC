#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "cmdargs.h"
#include "lparse.h"
#include "process.h"

static int process_entry(FILE *res, struct lexem_list *l);

void process(struct settings *sts)
{
	struct lexem_list *llist = lexem_parse(sts->input_file);
	FILE *res = fopen(sts->output_file, "w");
	if (res == NULL)
		die("%s: %s\n", sts->output_file, strerror(errno));
	while (process_entry(res, llist));
}

static void process_function(struct lexem_list *l, struct coord *fcor,
		FILE *res);

static int process_entry(FILE *res, struct lexem_list *l)
{
	enum lexem_type lt;
	struct coord cor;

	if (ll_get(l, &lt, &cor, NULL) == 0)
		return 0;

	switch(lt) {
	case new_line:
		break;
	case func_decl:
		process_function(l, &cor, res);
		break;
	default:
		die("Not a function\n");
	}
	return 1;
}

static void process_function(struct lexem_list *l, struct coord *fcor, FILE *res)
{
	struct lexem_list *funcl = ll_extract_upto_lt(l, close_brace);
#ifdef DEBUG
	printf("Extracted list:\n");
	ll_print(funcl);
	printf("Extracted list end\n");
#endif
}
