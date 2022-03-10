#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "lparse.h"

enum {
	max_lexem_len = 32
};

const char *msg_lxm_len_limit =
	"lexem at %d, %d: lexems can't be longer than %d symbols";
const char *print_separ = "\n";

const char *str_func_decl = "func";
const char *str_int_spec = "i";
const char *str_cmd_add = "add";
const char *str_cmd_copy = "copy";
const char *str_cmd_ret = "ret";

struct lexem {
	struct coord cor;
	enum lexem_type lt;
	union lexem_data data;
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

static struct lexem_list *ll_init();


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
	struct coord lexem_start_cor = pos->cor;
	enum lexem_type lt;
	union lexem_data dt;
	if (buffer[0] == '\n') {
		lt = new_line;
	} else if(is_alpha(buffer[0])) {
		lt = word;
		eat_name(ifile, buffer, &lexem_start_cor, pos);
	} else if (buffer[0] == '(') {
		lt = parenleft;
	} else if (buffer[0] == ')') {
		lt = parenright;
	} else if (buffer[0] == ',') {
		lt = coma;
	} else if (buffer[0] == '%') {
		lt = variable;
		eat_variable(ifile, buffer, &dt, &lexem_start_cor, pos);
	} else if (buffer[0] == '{') {
		lt = open_brace;
	} else if (buffer[0] == '}') {
		lt = close_brace;
	} else if (is_number(buffer[0])) {
		lt = number;
		eat_number(ifile, buffer, &dt, &lexem_start_cor, pos);
	} else if (buffer[0] == '=') {
		lt = equal_sign;
	} else if (buffer[0] == '$') {
		lt = func_name;
		eat_func_name(ifile, buffer, &dt, &lexem_start_cor, pos);
	}
	if (lt == word)
		lexem_clarify(buffer, &lt);
	ll_add(ll, lt, &lexem_start_cor, &dt);
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
		if (bufpos > max_lexem_len) {
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
		if (bufpos > max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		pos_next(pos, c);
		buffer[bufpos] = c;
	}
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
		if (bufpos > max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
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
		if (bufpos > max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		buffer[bufpos] = c;
	}
	buffer[bufpos] = 0;
	dt->str_value = sstrdup(buffer);
}

static void lexem_clarify(char *buffer, enum lexem_type *lt)
{
	if (strcmp(buffer, str_func_decl) == 0)
		*lt = func_decl;
	else if(strcmp(buffer, str_int_spec) == 0)
		*lt = int_spec;
	else if(strcmp(buffer, str_cmd_add) == 0)
		*lt = cmd_add;
	else if(strcmp(buffer, str_cmd_copy) == 0)
		*lt = cmd_copy;
	else if(strcmp(buffer, str_cmd_ret) == 0)
		*lt = cmd_ret;
}

static struct lexem_list *ll_init()
{
	struct lexem_list *tmp = smalloc(sizeof(struct lexem_list));
	tmp->first = tmp->last = NULL;
	tmp->last_is_nl = 0;
	return tmp;
}

void ll_add(struct lexem_list *ll, enum lexem_type lt,
		struct coord *cor, union lexem_data *dt)
{
	if (lt == new_line && ll->last_is_nl)
		return;
	if (ll->first == NULL)
		ll->first = ll->last = smalloc(sizeof(struct lexem));
	else
		ll->last = ll->last->next = smalloc(sizeof(struct lexem));
	ll->last->lt = lt;
	ll->last->cor = *cor;
	ll->last->data = *dt;
	ll->last->next = NULL;
	ll->last_is_nl = lt == new_line;
}

int ll_get(struct lexem_list *ll, enum lexem_type *lt,
		struct coord *cor, union lexem_data *dt)
{
	struct lexem *tmp;
	if (ll->first == NULL)
		return 0;
	if (ll->first == ll->last)
		ll->last = NULL;
	tmp = ll->first;
	ll->first = ll->first->next;
	*lt = tmp->lt;
	*cor = tmp->cor;
	if (dt)
		*dt = tmp->data;
	return 1;
}

struct lexem_list *ll_extract_upto_lt(struct lexem_list *l,
		enum lexem_type lt)
{
	struct lexem_list *newl = ll_init();
	struct lexem **tmp = &l->first;

	for (; *tmp && ((*tmp)->lt != lt); tmp = &(*tmp)->next);

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

void ll_print(struct lexem_list *ll)
{
	struct lexem *tmp = ll->first;
	char *cmd_prx = "Command: ";
	for (; tmp; tmp = tmp->next) {
		printf("%02d, %02d:", tmp->cor.row, tmp->cor.col);
		switch(tmp->lt)	{
		case new_line:
			printf("Newline");
			break;
		case word:
			printf("Word");
			break;
		case parenleft:
			printf("Parenleft");
			break;
		case parenright:
			printf("Parenright");
			break;
		case coma:
			printf("Coma");
			break;
		case variable:
			printf("Var:[%s]", tmp->data.str_value);
			break;
		case open_brace:
			printf("OpenBrace");
			break;
		case close_brace:
			printf("CloseBrace");
			break;
		case number:
			printf("Number:[%s]", tmp->data.str_value);
			break;
		case equal_sign:
			printf("Equal sign");
			break;
		case func_decl:
			printf("Function declaration");
			break;
		case func_name:
			printf("function:[%s]", tmp->data.str_value);
			break;
		case int_spec:
			printf("Integer type specifier");
			break;
		case cmd_add:
			printf("%sAdd", cmd_prx);
			break;
		case cmd_copy:
			printf("%sCopy", cmd_prx);
			break;
		case cmd_ret:
			printf("%sRet", cmd_prx);
			break;
		}
		printf("%s", print_separ);
	}
}
