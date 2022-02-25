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

static int process_entry(FILE *res, struct lexem_list *l)
{
	return 0;
}
