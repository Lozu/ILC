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
	int row;
	int col;
	enum lexem_type lt;
	union {
		char *str_value;
		int number;
	};
	struct lexem *next;
};

struct lexem_list {
	int last_is_nl;
	struct lexem *first;
	struct lexem *last;
};

struct position {
	int row;
	int col;
	int last_was_nl;
};

static inline void pos_next(struct position *p, char c)
{
	if (p->last_was_nl) {
		++p->row;
		p->col = 1;
		p->last_was_nl = 0;
	} else {
		++p->col;
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
static void ll_add(struct lexem_list *ll, struct lexem *lxm);
static void ll_print(struct lexem_list *ll);


static void eat_lexem(FILE *ifile, char *buffer, struct position *pos,
	   	struct lexem_list *ll);

struct lexem_list *lexem_parse(char *filename)
{
	struct lexem_list *ll = ll_init();
	FILE *ifile = fopen(filename, "r");
	struct position pos = { 1, 0, 0 };
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
#ifdef DEBUG
	ll_print(ll);
#endif
	return ll;
}

static void eat_name(FILE *ifile, char *buffer, struct lexem *lxm,
		struct position *start, struct position *pos);
static void eat_variable(FILE *ifile, char *buffer, struct lexem *lxm,
		struct position *start, struct position *pos);
static void eat_number(FILE *ifile, char *buffer, struct lexem *lxm,
		struct position *start, struct position *pos);
static void eat_func_name(FILE *ifile, char *buffer, struct lexem *lxm,
		struct position *start, struct position *pos);
static void lexem_clarify(char *buffer, struct lexem *l);

static void eat_lexem(FILE *ifile, char *buffer, struct position *pos,
	   	struct lexem_list *ll)
{
	struct position lexem_pos = *pos;
	struct lexem lxm;
	lxm.row = pos->row;
	lxm.col = pos->col;
	if (buffer[0] == '\n') {
		lxm.lt = new_line;
	} else if(is_alpha(buffer[0])) {
		lxm.lt = word;
		eat_name(ifile, buffer, &lxm, &lexem_pos, pos);
	} else if (buffer[0] == '(') {
		lxm.lt = parenleft;
	} else if (buffer[0] == ')') {
		lxm.lt = parenright;
	} else if (buffer[0] == ',') {
		lxm.lt = coma;
	} else if (buffer[0] == '%') {
		lxm.lt = variable;
		eat_variable(ifile, buffer, &lxm, &lexem_pos, pos);
	} else if (buffer[0] == '{') {
		lxm.lt = open_brace;
	} else if (buffer[0] == '}') {
		lxm.lt = close_brace;
	} else if (is_number(buffer[0])) {
		lxm.lt = number;
		eat_number(ifile, buffer, &lxm, &lexem_pos, pos);
	} else if (buffer[0] == '=') {
		lxm.lt = equal_sign;
	} else if (buffer[0] == '$') {
		lxm.lt = func_name;
		eat_func_name(ifile, buffer, &lxm, &lexem_pos, pos);
	}
	if (lxm.lt == word)
		lexem_clarify(buffer, &lxm);
	ll_add(ll, &lxm);
}

static void eat_name(FILE *ifile, char *buffer, struct lexem *lxm,
		struct position *start, struct position *pos)
{
	int bufpos = 1;
	for (;; ++bufpos) {
		int c = fgetc(ifile);
		if (!is_alpha(c) && !is_number(c)) {
			ungetc(c, ifile);
			break;
		}
		if (bufpos == max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		pos_next(pos, c);
		buffer[bufpos] = c;
	}
	buffer[bufpos] = 0;
}

static void eat_variable(FILE *ifile, char *buffer, struct lexem *lxm,
		struct position *start, struct position *pos)
{
	int bufpos = 0;
	for (;; ++bufpos) {
		int c = fgetc(ifile);
		if (!is_alpha(c) && !is_number(c) && c != '_' && c != '.') {
			ungetc(c, ifile);
			break;
		}
		if (bufpos == max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		pos_next(pos, c);
		buffer[bufpos] = c;
	}
	buffer[bufpos] = 0;
	lxm->str_value = smalloc(bufpos);
	memcpy(lxm->str_value, buffer, bufpos);
}

static void eat_number(FILE *ifile, char *buffer, struct lexem *lxm,
		struct position *start, struct position *pos)
{
	int bufpos = 1;
	for (;; ++bufpos) {
		int c = fgetc(ifile);
		if (!is_number(c)) {
			ungetc(c, ifile);
			break;
		}
		if (bufpos == max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		buffer[bufpos] = c;
	}
	buffer[bufpos] = 0;
	lxm->str_value = smalloc(bufpos);
	memcpy(lxm->str_value, buffer, bufpos);
}

static void eat_func_name(FILE *ifile, char *buffer, struct lexem *lxm,
		struct position *start, struct position *pos)
{
	int bufpos = 0;
	for (;; ++bufpos) {
		int c = fgetc(ifile);
		if (!is_alpha(c) && !is_number(c) && c != '_') {
			ungetc(c, ifile);
			break;
		}
		if (bufpos == max_lexem_len)
			die(msg_lxm_len_limit, start->row, start->col, max_lexem_len);
		buffer[bufpos] = c;
	}
	buffer[bufpos] = 0;
	lxm->str_value = smalloc(bufpos);
	memcpy(lxm->str_value, buffer, bufpos);
}

static void lexem_clarify(char *buffer, struct lexem *l)
{
	if (strcmp(buffer, str_func_decl) == 0)
		l->lt = func_decl;
	else if(strcmp(buffer, str_int_spec) == 0)
		l->lt = int_spec;
	else if(strcmp(buffer, str_cmd_add) == 0)
		l->lt = cmd_add;
	else if(strcmp(buffer, str_cmd_copy) == 0)
		l->lt = cmd_copy;
	else if(strcmp(buffer, str_cmd_ret) == 0)
		l->lt = cmd_ret;
}

static struct lexem_list *ll_init()
{
	struct lexem_list *tmp = smalloc(sizeof(struct lexem_list));
	tmp->first = tmp->last = NULL;
	tmp->last_is_nl = 0;
	return tmp;
}

static void ll_add(struct lexem_list *ll, struct lexem *lxm)
{
	if (lxm->lt == new_line && ll->last_is_nl)
		return;
	if (ll->first == NULL)
		ll->first = ll->last = smalloc(sizeof(struct lexem_list));
	else
		ll->last = ll->last->next = smalloc(sizeof(struct lexem_list));
	*ll->last = *lxm;
	ll->last->next = NULL;
	ll->last_is_nl = lxm->lt == new_line;
}

static void ll_print(struct lexem_list *ll)
{
	struct lexem *tmp = ll->first;
	char *cmd_prx = "Command: ";
	for (; tmp; tmp = tmp->next) {
		printf("%02d, %02d:", tmp->row, tmp->col);
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
			printf("Var:[%s]", tmp->str_value);
			break;
		case open_brace:
			printf("OpenBrace");
			break;
		case close_brace:
			printf("CloseBrace");
			break;
		case number:
			printf("Number");
			break;
		case equal_sign:
			printf("Equal sign");
			break;
		case func_decl:
			printf("Function declaration");
			break;
		case func_name:
			printf("function:[%s]", tmp->str_value);
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
