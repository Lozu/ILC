#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "lparse.h"
#include "remap.h"

struct node {
	char *key;
	int value;
	char color;	/* 0 - black, 1 - red */
	struct node *p;
	struct node *left;
	struct node *right;
};

struct str_tree {
	struct node *root;
	struct node *null;
	int curr_val;
};

static const char *msg_dbg_var = "Variable remapping";
static const char *msg_dbg_gn = "Global names remapping";
static const struct node null = { NULL, -1, 0, &null, &null, &null };

static struct str_tree *t_init();
static int add_unique(struct str_tree *t, char *s);

#define REMAP(func_name, tbl_type, table_fill_func, debug_func, \
		lx_from, lx_to) \
struct lexem_list * func_name(struct lexem_list *l, struct tbl_type *tb) \
{ \
	struct lexem_list *res = ll_init(); \
	struct lexem_block bdata; \
	struct str_tree *var_tree = t_init(); \
 \
	while (ll_get(l, &bdata)) { \
		if (bdata.lt == lx_from) { \
			bdata.dt.number = add_unique(var_tree, bdata.dt.str_value); \
			bdata.lt = lx_to; \
		} \
		ll_add(res, &bdata); \
	} \
 \
	table_fill_func(tb, var_tree); \
	debug_func(tb, res); \
 \
	free(l); \
	free(var_tree); \
	return res; \
}

static void table_fill_gn_sym_tbl(struct gn_sym_tbl *tb,
		struct str_tree *t);
static void table_fill_var_sym_tbl(struct var_sym_tbl *tb,
		struct str_tree *t);
static void debug_gn_sym_tbl(struct gn_sym_tbl*tb, struct lexem_list *l);
static void debug_var_sym_tbl(struct var_sym_tbl*tb, struct lexem_list *l);

REMAP(remap_gns, gn_sym_tbl, table_fill_gn_sym_tbl, debug_gn_sym_tbl,
		lx_global_name, lx_gn_rmp)
REMAP(remap_vars, var_sym_tbl, table_fill_var_sym_tbl, debug_var_sym_tbl,
		lx_variable, lx_var_remapped)

#define TABLE_FILL(tbl_type, tbl_entry_type, tt_func) \
static void table_fill_ ## tbl_type (struct tbl_type *tb, \
		struct str_tree *t) \
{ \
	tb->len = t->curr_val; \
	if (tb->len == 0) { \
		tb->vec = NULL; \
		return; \
	} \
	tb->vec = smalloc(tb->len * sizeof(struct tbl_entry_type)); \
	tt_func(tb, t); \
}

static void transform_tree_gn_sym_tbl(struct gn_sym_tbl *tb,
		struct str_tree *t);
static void transform_tree_var_sym_tbl(struct var_sym_tbl *tb,
		struct str_tree *t);

TABLE_FILL(gn_sym_tbl, gn_tbl_entry, transform_tree_gn_sym_tbl)
TABLE_FILL(var_sym_tbl, var_tbl_entry, transform_tree_var_sym_tbl)

#define __TRANSFORM_TREE(tbl_type) \
static void __transform_tree_ ## tbl_type(struct tbl_type *tb, \
		struct node *null, struct node *x) \
{ \
	if (x != null) { \
		__transform_tree_ ## tbl_type(tb, null, x->left); \
		__transform_tree_ ## tbl_type(tb, null, x->right); \
		tb->vec[x->value].name =  x->key; \
		free(x); \
	} \
}

__TRANSFORM_TREE(gn_sym_tbl)
__TRANSFORM_TREE(var_sym_tbl)

#define TRANSFORM_TREE(tbl_type) \
static void transform_tree_ ## tbl_type(struct tbl_type *tb, \
		struct str_tree *t) \
{ \
	__transform_tree_ ## tbl_type(tb, t->null, t->root); \
}

TRANSFORM_TREE(gn_sym_tbl)
TRANSFORM_TREE(var_sym_tbl)

#define DEBUG_REMAPPED(tbl_type, title_msg) \
static void debug_ ## tbl_type(struct tbl_type *tb, struct lexem_list *l) \
{ \
	int i; \
	if (dbg_function_remapped == 0) \
		return; \
	eprintf("---%s---\n", title_msg); \
	ll_print(stderr, l); \
	eprintf("\tTable\n"); \
	for (i = 0; i < tb->len; ++i) { \
		eprintf("\t%d: \'%s\'\n", i, tb->vec[i].name); \
	} \
	eprintf("\n"); \
}

DEBUG_REMAPPED(gn_sym_tbl, msg_dbg_gn)
DEBUG_REMAPPED(var_sym_tbl, msg_dbg_var)

#define SYM_TBL_FREE(tbl_type) \
void tbl_type ## _free(struct tbl_type *t) \
{ \
	int i; \
	for (i = 0; i < t->len; ++i) \
		free(t->vec[i].name); \
	free(t->vec); \
}

SYM_TBL_FREE(gn_sym_tbl)
SYM_TBL_FREE(var_sym_tbl)

static struct node *getnode(struct str_tree *t);

static struct str_tree *t_init()
{
	struct str_tree *tmp = smalloc(sizeof(struct str_tree));
	tmp->null = &null;
	tmp->root = tmp->null;
	tmp->curr_val = 0;
	return tmp;
}

static struct node *getnode(struct str_tree *t)
{
	struct node *tmp = smalloc(sizeof(struct node));
	tmp->color = 0;
	tmp->key = NULL;
	tmp->value = -1;
	tmp->p = tmp->left = tmp->right = t->null;
	return tmp;
}

static void fixup(struct str_tree *t, struct node *x);

static int add_unique(struct str_tree *t, char *s)
{
	int last = -1;
	struct node *y = t->null;
	struct node *x = t->root;
	struct node *z;

	while (x != t->null) {
		int val;
		y = x;
		val = strcmp(y->key, s);

		if (val == 0) {
			free(s);
			return y->value;
		}
		if (val > 0) {
			x = x->left;
			last = 0;
		} else {
			x = x->right;
			last = 1;
		}
	}
	z = getnode(t);
	z->p = y;
	if (y == t->null)
		t->root = z;
	else if (last == 0)
		y->left = z;
	else
		y->right = z;

	z->key = s;
	z->value = t->curr_val;
	++t->curr_val;
	fixup(t, z);
	return z->value;
}

static void right_rotate(struct str_tree *t, struct node *x);
static void left_rotate(struct str_tree *t, struct node *x);

static void fixup(struct str_tree *t, struct node *x)
{
	while (x->p->p != t->null && x->p->color == 1) {
		if (x->p == x->p->p->left) {
			struct node *y = x->p->p->right;
			if (y->color == 1) {
				x->p->color = y->color = 0;
				x->p->p->color = 1;
				x = x->p->p;
			} else if (x == x->p->right) {
				x = x->p;
				left_rotate(t, x->p->p);
			} else {
				x->p->color = 0;
				x->p->p->color = 1;
				right_rotate(t, x->p->p);
			}
		} else {
			struct node *y = x->p->p->left;
			if (y->color == 1) {
				x->p->color = y->color = 0;
				x->p->p->color = 1;
				x = x->p->p;
			} else if (x == x->p->left) {
				x = x->p;
				right_rotate(t, x);
			} else {
				x->p->color = 0;
				x->p->p->color = 1;
				left_rotate(t, x->p->p);
			}
		}
	}
	t->root->color = 0;
}

static inline void set_parent(struct str_tree *t, struct node *x,
		struct node *p)
{
	if (x != t->null)
		x->p = p;
}

static void left_rotate(struct str_tree *t, struct node *x)
{
	/*
	 *      x              y
	 *     / \            / \
	 *    1   y    ->    x   3
	 *       / \        / \
	 *      2   3      1   2
	 */

	struct node rx = *x;
	struct node ry = *x->right;

	struct node *y = x->right;

	y->p = rx.p;
	if (y->p->left == x)
		y->p->left = y;
	else if (y->p->right == x)
		y->p->right = y;

	y->left = x;
	x->p = y;
	x->right = ry.left;
	set_parent(t, x->right, x);

	if (y->p == t->null)
		t->root = y;
}

static void right_rotate(struct str_tree *t, struct node *x)
{
	/*
	 *        x          y
	 *       / \        / \
	 *      y   3  ->  1   x
	 *     / \            / \
	 *    1   2          2   3
	 */

	struct node rx = *x;
	struct node ry = *x->left;

	struct node *y = x->left;

	y->p = rx.p;
	if (y->p->left == x)
		y->p->left = y;
	else if (y->p->right == x)
		y->p->right = y;
	
	y->right = x;
	x->p = y;
	x->left = ry.left;
	set_parent(t, x->left, x);

	if (y->p == t->null)
		t->root = y;
}
