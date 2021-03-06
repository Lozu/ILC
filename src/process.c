#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "cmdargs.h"
#include "lparse.h"
#include "process.h"
#include "remap.h"
#include "func.h"
#include "alloc.h"
#include "emit.h"
#include "process.h"

static const char *msg_free_gl = "lonely global specifier";

static void fl_add(struct func_list *fl, struct function *f);

static void act1(struct state *st, struct settings *sts);
static void act2(struct state *st);

void process(struct settings *sts)
{
	struct state st;
	memset(&st, 0, sizeof(struct state));

	act1(&st, sts);
	act2(&st);
}

static FILE *open_file(char *s);
static int preprocess_entry(struct state *s);

static void act1(struct state *st, struct settings *sts)
{
	if (dbg_acts_start)
		eprintf("\n\n------First act begin------\n\n");

	output_file = open_file(sts->output_file);
	if (dbg_free_all_mem)
		free(sts->output_file);

	st->l = remap_gns(lexem_parse(sts->input_file), &st->global_tbl);
	debug_gn_sym_tbl_pre(&st->global_tbl);

	while (preprocess_entry(st));
}

static FILE *open_file(char *s)
{
	FILE *tmp = fopen(s, "w");
	if (tmp == NULL)
		die("%s: %s\n", s, strerror(errno));
	return tmp;
}

static struct function *process_function(int gl_spec, struct lexem_list *l,
		struct gn_sym_tbl *tbl, struct fcall_list *cl);

static int preprocess_entry(struct state *s)
{
	static struct coord gl_pos = { -1, 0 };

	struct lexem_block b;
	struct function *f;

	if (ll_get(s->l, &b) == 0) {
		if (gl_pos.row != -1)
			die("[%d,%d]:%s\n", gl_pos.row, gl_pos.col, msg_free_gl);
		return 0;
	}

	switch(b.lt) {
	case lx_new_line:
		break;
	case lx_global_spec:
		gl_pos = b.crd;
		break;
	case lx_func_decl:
		f = process_function(gl_pos.row != -1, s->l,
				&s->global_tbl, &s->fcl);
		fl_add(&s->fl, f);
		gl_pos.row = -1;
		break;
	default:
		die("Not a function\n");
	}
	return 1;
}

static struct function *process_function(int gl_spec, struct lexem_list *l,
		struct gn_sym_tbl *tbl, struct fcall_list *cl)
{
	struct function *f = smalloc(sizeof(struct function));
	if (dbg_gn_borders)
		eprintf("----Working on global name----\n\n");

	struct lexem_list *funcl = ll_extract_upto_lt(l, lx_close_brace);
	struct lexem_list *rnml = remap_vars(funcl, &f->stb);
	debug_var_sym_tbl(&f->stb, rnml);

	func_header_form(rnml, f, gl_spec, tbl);
	cmd_form(rnml, f, tbl, cl);

	f->alloc_table = allocate(f);
	free(rnml);
	return f;
}

static void func_free(struct function *f);

static void act2(struct state *st)
{
	if (dbg_acts_start)
		eprintf("\n\n------Second act begin------\n\n");

	if (dbg_free_all_mem)
		free(st->l);

	debug_fcall_list(&st->fcl, &st->global_tbl);
	check_functions(&st->global_tbl, &st->fcl);
	debug_gn_sym_tbl_post(&st->global_tbl);
	emit_gn_specs(&st->global_tbl);

	while (st->fl.first) {
		struct func_list_el *tmp = st->fl.first;
		st->fl.first = tmp->next;

		asm_emit(tmp->f);
		if (dbg_free_all_mem) {
			func_free(tmp->f);
			free(tmp);
		}
	}

	if (dbg_free_all_mem)
		gn_sym_tbl_free(&st->global_tbl);

	fclose(output_file);
}

static void func_free(struct function *f)
{
	var_sym_tbl_free(&f->stb);
	cmd_list_free(f->cl);
	alloc_free(f->alloc_table);
	free(f);
}

static void fl_add(struct func_list *fl, struct function *f)
{
	struct func_list_el *tmp = smalloc(sizeof(struct func_list_el));
	if (fl->first == NULL)
		fl->first = fl->last = tmp;
	else
		fl->last = fl->last->next = tmp;
	tmp->f = f;
	tmp->next = NULL;
}
