#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "lparse.h"

static const char *msg_lxm_len_limit =
	"lexem at %d, %d: lexems can't be longer than %d symbols";
static const char *msg_var_zero =
	"variable name can't be empty string";
static const char *msg_fname_zero =
	"function name can't be empty string";
static const char *msg_inv_sym = "invalid symbol";

static const char *str_global_spec = "global";
static const char *str_func_decl = "func";
static const char *str_int_spec = "i";
static const char *str_cmd_call = "call";
static const char *str_cmd_add = "add";
static const char *str_cmd_sub = "sub";
static const char *str_cmd_mul = "mul";
static const char *str_cmd_umul = "umul";
static const char *str_cmd_div = "div";
static const char *str_cmd_udiv = "udiv";
static const char *str_cmd_rem = "rem";
static const char *str_cmd_urem = "urem";
static const char *str_cmd_copy = "copy";
static const char *str_cmd_ret = "ret";

struct lexem {
	struct lexem_block lb;
	struct lexem *next;
};

struct lexem_list {
	int last_is_nl;
	struct lexem *first;
	struct lexem *last;
};

struct position {
	struct coord cor;
	int last_was_nl;
};

static inline void pos_next(struct position *p, char c)
{
	if (p->last_was_nl) {
		++p->cor.row;
		p->cor.col = 1;
		p->last_was_nl = 0;
	} else {
		++p->cor.col;
	}
	if (c == '\n')
		p->last_was_nl = 1;
}

static inline int is_separ(char c)
{
	if (c == ' ' || c == '\t')
		return 1;
	return 0;
}

static inline int is_alpha(char c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return 1;
	return 0;
}

static inline int is_number(char c)
{
	if (c >= '0' && c <= '9')
		return 1;
	return 0;
}

static void eat_lexem(FILE *ifile, char *buffer, struct position *pos,
	   	struct lexem_list *ll);
static void debug_global_lexem_parse(struct lexem_list *l);

struct lexem_list *lexem_parse(char *filename)
{
	struct lexem_list *ll = ll_init();
	FILE *ifile = fopen(filename, "r");
	struct position pos = { { 1, 0 }, 0 };
	int c;

	if (ifile == NULL)
		die("%s: %s\n", filename, strerror(errno));
	while((c = fgetc(ifile)) != EOF) {
		char buffer[max_lexem_len + 1];
		pos_next(&pos, c);
		if (is_separ(c))
			continue;
		buffer[0] = c;
		eat_lexem(ifile, buffer, &pos, ll);
	}
	if (fclose(ifile) == -1)
		die("%s: %s\n", filename, strerror(errno));
	debug_global_lexem_parse(ll);
	return ll;
}

static void eat_name(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos);
static void eat_variable(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos);
static void eat_number(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos);
static void eat_func_name(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos);

static void lexem_clarify(char *buffer, enum lexem_type *lt,
		struct coord *pos);

static void eat_lexem(FILE *ifile, char *buffer, struct position *pos,
	   	struct lexem_list *ll)
{
	struct lexem_block b;
	b.crd = pos->cor;
	if (buffer[0] == '\n') {
		b.lt = lx_new_line;
	} else if(is_alpha(buffer[0])) {
		b.lt = lx_word;
		eat_name(ifile, buffer, &b.dt, &b.crd, pos);
	} else if (buffer[0] == '(') {
		b.lt = lx_parenleft;
	} else if (buffer[0] == ')') {
		b.lt = lx_parenright;
	} else if (buffer[0] == ',') {
		b.lt = lx_coma;
	} else if (buffer[0] == '%') {
		b.lt = lx_variable;
		eat_variable(ifile, buffer, &b.dt, &b.crd, pos);
	} else if (buffer[0] == '{') {
		b.lt = lx_open_brace;
	} else if (buffer[0] == '}') {
		b.lt = lx_close_brace;
	} else if (is_number(buffer[0]) || buffer[0] == '-') {
		b.lt = lx_number;
		eat_number(ifile, buffer, &b.dt, &b.crd, pos);
	} else if (buffer[0] == '=') {
		b.lt = lx_equal_sign;
	} else if (buffer[0] == '$') {
		b.lt = lx_global_name;
		eat_func_name(ifile, buffer, &b.dt, &b.crd, pos);
	} else {
		die("%d,%d: %s\n", pos->cor.row, pos->cor.col, msg_inv_sym);
	}
	if (b.lt == lx_word)
		lexem_clarify(buffer, &b.lt, &b.crd);
	ll_add(ll, &b);
}

#define EAT_TMP(func_name, buf_pos, predicate, save_buf, \
		die_if_none, die_msg) \
static void func_name(FILE *ifile, char *buffer, union lexem_data *dt, \
		struct coord *start, struct position *pos) \
{ \
	int bufpos = buf_pos; \
	for (;; ++bufpos) { \
		int c = fgetc(ifile); \
		if (!(predicate)) { \
			ungetc(c, ifile); \
			break; \
		} \
		if (bufpos + 1 > max_lexem_len) \
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len); \
		pos_next(pos, c); \
		buffer[bufpos] = c; \
	} \
	if (die_if_none && bufpos == 0) \
		die("%d,%d: %s\n", start->row, start->col, die_msg); \
	buffer[bufpos] = 0; \
	if (!save_buf) \
		return; \
	dt->str_value = sstrdup(buffer); \
}

EAT_TMP(eat_name, 1, is_alpha(c) || is_number(c), 0, 0, NULL)
EAT_TMP(eat_variable, 0, is_alpha(c) || is_number(c) || \
		c == '_' || c == '.', 1, 1, msg_var_zero)
EAT_TMP(eat_number, 1, is_number(c), 1, 0, NULL)
EAT_TMP(eat_func_name, 0, is_alpha(c) || is_number(c) || c == '_',
		1, 1, msg_fname_zero)

static void lexem_clarify(char *buffer, enum lexem_type *lt,
		struct coord *pos)
{
	if (strcmp(buffer, str_func_decl) == 0)
		*lt = lx_func_decl;
	else if(strcmp(buffer, str_int_spec) == 0)
		*lt = lx_int_spec;
	else if(strcmp(buffer, str_cmd_call) == 0)
		*lt = lx_cmd_call;
	else if(strcmp(buffer, str_cmd_add) == 0)
		*lt = lx_cmd_add;
	else if (strcmp(buffer, str_cmd_sub) == 0)
		*lt = lx_cmd_sub;
	else if (strcmp(buffer, str_cmd_mul) == 0)
		*lt = lx_cmd_mul;
	else if (strcmp(buffer, str_cmd_umul) == 0)
		*lt = lx_cmd_umul;
	else if (strcmp(buffer, str_cmd_div) == 0)
		*lt = lx_cmd_div;
	else if (strcmp(buffer, str_cmd_udiv) == 0)
		*lt = lx_cmd_udiv;
	else if (strcmp(buffer, str_cmd_rem) == 0)
		*lt = lx_cmd_rem;
	else if (strcmp(buffer, str_cmd_urem) == 0)
		*lt = lx_cmd_urem;
	else if(strcmp(buffer, str_cmd_copy) == 0)
		*lt = lx_cmd_copy;
	else if(strcmp(buffer, str_cmd_ret) == 0)
		*lt = lx_cmd_ret;
	else if(strcmp(buffer, str_global_spec) == 0)
		*lt = lx_global_spec;
	else
		die("[%d,%d]: unknown lexem\n", pos->row, pos->col);
}

static void debug_global_lexem_parse(struct lexem_list *l)
{
	if (dbg_global_lexem_parse == 0)
		return;
	eprintf("----Global lexem parse----\n");
	ll_print(stderr, l);
	eprintf("\n");
}

struct lexem_list *ll_init()
{
	struct lexem_list *tmp = smalloc(sizeof(struct lexem_list));
	tmp->first = tmp->last = NULL;
	tmp->last_is_nl = 0;
	return tmp;
}

void ll_add(struct lexem_list *ll, struct lexem_block *b)
{
	if (b->lt == lx_new_line && ll->last_is_nl)
		return;
	if (ll->first == NULL)
		ll->first = ll->last = smalloc(sizeof(struct lexem));
	else
		ll->last = ll->last->next = smalloc(sizeof(struct lexem));
	ll->last->lb.lt = b->lt;
	ll->last->lb.crd = b->crd;
	ll->last->lb.dt = b->dt;
	ll->last->next = NULL;
	ll->last_is_nl = b->lt == lx_new_line;
}

int ll_get(struct lexem_list *ll, struct lexem_block *b)
{
	struct lexem *tmp;
	if (ll->first == NULL)
		return 0;
	if (ll->first == ll->last)
		ll->last = NULL;
	tmp = ll->first;
	ll->first = ll->first->next;
	b->lt = tmp->lb.lt;
	b->crd = tmp->lb.crd;
	b->dt = tmp->lb.dt;
	free(tmp);
	return 1;
}

static void lexem_types_print(FILE *f, enum lexem_type *vec, int vec_len);

int lexem_clever_get(struct lexem_list *l, struct lexem_block *b,
		enum lexem_type *ltvec, int ltvec_len)
{
	int i;
	if (ll_get(l, b) == 0) {
		eprintf("expected ");
		lexem_types_print(stderr, ltvec, ltvec_len);
		eprintf(", not eof\n");
	}
	for (i = 0; i < ltvec_len; ++i) {
		if (ltvec[i] >= 1000000) {
			if (b->lt > ltvec[i] - 1000000)
				return i;
		} else if (b->lt == ltvec[i]) {
			return i;
		}
	}
	eprintf("%d,%d: expected ", b->crd.row, b->crd.col);
	lexem_types_print(stderr, ltvec, ltvec_len);
	eprintf(", not ");
	eprintf("%s\n", LNAME(b->lt, 0, NULL));
	exit(1);
	return -1;
}

static void lexem_types_print(FILE *f, enum lexem_type *vec, int vec_len)
{
	int i;
	char *buf = alloca(lexem_alloca_size);
	for (i = 0; i < vec_len ; ++i) {
		fprintf(f, "%s", lexem_name(buf, vec[i] > 1000000 ?
				vec[i] - 1000000 : vec[i], 0, NULL));
		if (i < vec_len - 1)
			fprintf(f, " or ");
	}
}

struct lexem_list *ll_extract_upto_lt(struct lexem_list *l,
		enum lexem_type lt)
{
	struct lexem_list *newl = ll_init();
	struct lexem **tmp = &l->first;

	for (; *tmp && ((*tmp)->lb.lt != lt); tmp = &(*tmp)->next);

	if (*tmp == NULL)
		return NULL;

	newl->first = l->first;
	newl->last = *tmp;
	l->first = newl->last->next;
	newl->last->next = NULL;
	if (l->first == NULL)
		l->last = NULL;
	return newl;
}

#define P "command-"

char *lexem_names[] = {
	[lx_word] 			=	"word",			/**/
	[lx_new_line] 		=	"newline",
	[lx_parenleft]		=	"parenleft",
	[lx_parenright]		=	"parenright",
	[lx_coma]			=	"coma",
	[lx_variable]		=	"variable",		/**/
	[lx_var_remapped]	=	"var",			/**/
	[lx_open_brace]		=	"openbrace",
	[lx_close_brace]	=	"closebrace",
	[lx_number]			=	"number",
	[lx_equal_sign]		=	"equal sign",
	[lx_global_spec]	=	"global spec",
	[lx_func_decl]		=	"function declaration",
	[lx_global_name]	=	"raw global name",
	[lx_gn_rmp]			=	"global name",	/**/
	[lx_int_spec]		=	"integer type specifier",
	[lx_cmd]			=	"command",
	[lx_cmd_call]		=	P"call",
	[lx_cmd_add]		=	P"add",
	[lx_cmd_sub]		=	P"sub",
	[lx_cmd_mul]		=	P"mul",
	[lx_cmd_umul]		=	P"umul",
	[lx_cmd_div]		=	P"div",
	[lx_cmd_udiv]		=	P"udiv",
	[lx_cmd_rem]		=	P"rem",
	[lx_cmd_urem]		=	P"urem",
	[lx_cmd_copy]		=	P"copy",
	[lx_cmd_ret]		=	P"ret"
};

char *lexem_name(char *buf, enum lexem_type lt, int v, union lexem_data *dt)
{
	sprintf(buf, lexem_names[lt]);

	if (lt < lx_new_line && v) {
		if (lt >= lx_var_remapped)
			sprintf(buf + strlen(lexem_names[lt]), "[%d]", dt->number);
		else
			sprintf(buf + strlen(lexem_names[lt]), "[%s]", dt->str_value);
	}
	return buf;
}

void ll_print(FILE *f, struct lexem_list *ll)
{
	struct lexem *tmp = ll->first;
	for (; tmp; tmp = tmp->next) {
		fprintf(f, "%02d,%02d: %s\n", tmp->lb.crd.row, tmp->lb.crd.col,
				LNAME(tmp->lb.lt, 1, &tmp->lb.dt));
	}
}
