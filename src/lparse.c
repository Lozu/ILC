#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "lparse.h"

enum {
	max_lexem_len = 32
};

static const char *msg_lxm_len_limit =
	"lexem at %d, %d: lexems can't be longer than %d symbols";
static const char *msg_var_zero =
	"variable name can't be empty string";
static const char *msg_fname_zero =
	"function name can't be empty string";
static const char *msg_inv_sym = "unvalid symbol";
static const char *print_separ = "\n";

static const char *str_func_decl = "func";
static const char *str_int_spec = "i";
static const char *str_cmd_add = "add";
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
	return ll;
}

static void eat_name(FILE *ifile, char *buffer,
		struct coord *start, struct position *pos);
static void eat_variable(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos);
static void eat_number(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos);
static void eat_func_name(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos);

static void lexem_clarify(char *buffer, enum lexem_type *lt);

static void eat_lexem(FILE *ifile, char *buffer, struct position *pos,
	   	struct lexem_list *ll)
{
	struct lexem_block b;
	b.crd = pos->cor;
	if (buffer[0] == '\n') {
		b.lt = lx_new_line;
	} else if(is_alpha(buffer[0])) {
		b.lt = lx_word;
		eat_name(ifile, buffer, &b.crd, pos);
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
	} else if (is_number(buffer[0])) {
		b.lt = lx_number;
		eat_number(ifile, buffer, &b.dt, &b.crd, pos);
	} else if (buffer[0] == '=') {
		b.lt = lx_equal_sign;
	} else if (buffer[0] == '$') {
		b.lt = lx_func_name;
		eat_func_name(ifile, buffer, &b.dt, &b.crd, pos);
	} else {
		die("%d,%d: %s\n", pos->cor.row, pos->cor.col, msg_inv_sym);
	}
	if (b.lt == lx_word)
		lexem_clarify(buffer, &b.lt);
	ll_add(ll, &b);
}

static void eat_name(FILE *ifile, char *buffer,
		struct coord *start, struct position *pos)
{
	int bufpos = 1;
	for (;; ++bufpos) {
		int c = fgetc(ifile);
		if (!is_alpha(c) && !is_number(c)) {
			ungetc(c, ifile);
			break;
		}
		if (bufpos + 1 > max_lexem_len) {
			die(msg_lxm_len_limit, start->row, start->col,
					max_lexem_len);
		}
		pos_next(pos, c);
		buffer[bufpos] = c;
	}
	buffer[bufpos] = 0;
}

static void eat_variable(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos)
{
	int bufpos = 0;
	for (;; ++bufpos) {
		int c = fgetc(ifile);
		if (!is_alpha(c) && !is_number(c) && c != '_' && c != '.') {
			ungetc(c, ifile);
			break;
		}
		if (bufpos + 1 > max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		pos_next(pos, c);
		buffer[bufpos] = c;
	}
	if (bufpos == 0)
		die("%d,%d: %s\n", start->row, start->col, msg_var_zero);
	buffer[bufpos] = 0;
	dt->str_value = sstrdup(buffer);
}

static void eat_number(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos)
{
	int bufpos = 1;
	for (;; ++bufpos) {
		int c = fgetc(ifile);
		if (!is_number(c)) {
			ungetc(c, ifile);
			break;
		}
		if (bufpos + 1 > max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		pos_next(pos, c);
		buffer[bufpos] = c;
	}
	buffer[bufpos] = 0;
	dt->str_value = sstrdup(buffer);
}

static void eat_func_name(FILE *ifile, char *buffer, union lexem_data *dt,
		struct coord *start, struct position *pos)
{
	int bufpos = 0;
	for (;; ++bufpos) {
		int c = fgetc(ifile);
		if (!is_alpha(c) && !is_number(c) && c != '_') {
			ungetc(c, ifile);
			break;
		}
		if (bufpos + 1 > max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		pos_next(pos, c);
		buffer[bufpos] = c;
	}
	if (bufpos == 0)
		die("%d, %d: %s\n", start->row, start->col, msg_fname_zero);
	buffer[bufpos] = 0;
	dt->str_value = sstrdup(buffer);
}

static void lexem_clarify(char *buffer, enum lexem_type *lt)
{
	if (strcmp(buffer, str_func_decl) == 0)
		*lt = lx_func_decl;
	else if(strcmp(buffer, str_int_spec) == 0)
		*lt = lx_int_spec;
	else if(strcmp(buffer, str_cmd_add) == 0)
		*lt = lx_cmd_add;
	else if(strcmp(buffer, str_cmd_copy) == 0)
		*lt = lx_cmd_copy;
	else if(strcmp(buffer, str_cmd_ret) == 0)
		*lt = lx_cmd_ret;
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
	return 1;
}

static void lexem_types_print(FILE *f, enum lexem_type *vec, int vec_len);
static void lexem_print(FILE *f, int should,
		enum lexem_type lt, union lexem_data *dt);


int lexem_clever_get(struct lexem_list *l, struct lexem_block *b,
		enum lexem_type *ltvec, int ltvec_len)
{
	int i;
	if (ll_get(l, b) == 0) {
		fprintf(stderr, "expected ");
		lexem_types_print(stderr, ltvec, ltvec_len);
		fprintf(stderr, ", not eof\n");
	}
	for (i = 0; i < ltvec_len; ++i) {
		if (ltvec[i] >= 1000000) {
			if (b->lt >= ltvec[i] - 1000000)
				return i;
		} else if (b->lt == ltvec[i]) {
			return i;
		}
	}
	fprintf(stderr, "%d,%d: expected ", b->crd.row, b->crd.col);
	lexem_types_print(stderr, ltvec, ltvec_len);
	fprintf(stderr, ", not ");
	lexem_print(stderr, 0, b->lt, NULL);
	fprintf(stderr, "\n");
	exit(1);
	return -1;
}

static void lexem_types_print(FILE *f, enum lexem_type *vec, int vec_len)
{
	int i;
	for (i = 0; i < vec_len ; ++i) {
		lexem_print(f, 0, vec[i] > 1000000 ?
				vec[i] - 1000000 : vec[i], NULL);
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

static void lexem_print(FILE *f, int should, enum lexem_type lt,
		union lexem_data *dt)
{
	char *cmd_prx = "Command: ";
	int value = 0;
	switch (lt) {
	case lx_new_line:
		fprintf(f, "Newline");
		break;
	case lx_word:
		fprintf(f, "Word");
		value = 1;
		break;
	case lx_parenleft:
		fprintf(f, "Parenleft");
		break;
	case lx_parenright:
		fprintf(f, "Parenright");
		break;
	case lx_coma:
		fprintf(f, "Coma");
		break;
	case lx_variable:
		fprintf(f, "Variable");
		value = 1;
		break;
	case lx_var_remapped:
		fprintf(f, "Renamed variable");
		value = 2;
		break;
	case lx_open_brace:
		fprintf(f, "Openbrace");
		break;
	case lx_close_brace:
		fprintf(f, "Closebrace");
		break;
	case lx_number:
		fprintf(f, "Number");
		value = 1;
		break;
	case lx_equal_sign:
		fprintf(f, "Equal sign");
		break;
	case lx_func_decl:
		fprintf(f, "Function declaration");
		break;
	case lx_func_name:
		fprintf(f, "Function");
		value = 1;
		break;
	case lx_int_spec:
		fprintf(f, "Integer type specifier");
		break;
	case lx_cmd_add:
		fprintf(f, "%sAdd", cmd_prx);
		break;
	case lx_cmd_copy:
		fprintf(f, "%sCopy", cmd_prx);
		break;
	case lx_cmd_ret:
		fprintf(f, "%sReturn", cmd_prx);
		break;
	}
	if (should && value > 0) {
		if (value == 1)
			fprintf(f, "[%s]", dt->str_value);
		else
			fprintf(f, "[%d]", dt->number);
	}
}

void ll_print(struct lexem_list *ll)
{
	struct lexem *tmp = ll->first;
	for (; tmp; tmp = tmp->next) {
		printf("%02d, %02d:", tmp->lb.crd.row, tmp->lb.crd.col);
		lexem_print(stdout, 1, tmp->lb.lt, &tmp->lb.dt);
		printf("%s", print_separ);
	}
}
