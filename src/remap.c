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

static const struct node null = { NULL, -1, 0, &null, &null, &null };

static struct str_tree *t_init();
static int add_unique(struct str_tree *t, char *s);

static void remap_var(struct lexem_list *res, struct lexem_block *b,
		struct str_tree *t);
static void var_table_fill(struct sym_tbl *tb, struct str_tree *t);
static void debug_function_remapped(struct lexem_list *l,
		struct tbl_entry *array, int len);
	
struct lexem_list *remap(struct lexem_list *l, struct sym_tbl *tb)
{
	struct lexem_list *res = ll_init();
	struct lexem_block bdata;
	struct str_tree *var_tree = t_init();

	while (ll_get(l, &bdata)) {
		if (bdata.lt == lx_variable)
			remap_var(res, &bdata, var_tree);
		else
			ll_add(res, &bdata);
	}
	var_table_fill(tb, var_tree);
	debug_function_remapped(res, tb->var_array, tb->var_arr_len);
	return res;
}

static void remap_var(struct lexem_list *res, struct lexem_block *b,
		struct str_tree *t) 
{
	b->dt.number = add_unique(t, b->dt.str_value);
	b->lt = lx_var_remapped;
	ll_add(res, b);
}

static void transform_tree(struct tbl_entry *tbe, struct str_tree *t);

static void var_table_fill(struct sym_tbl *tb, struct str_tree *t)
{
	tb->var_arr_len = t->curr_val;
	if (tb->var_arr_len == 0) {
		tb->var_array = NULL;
		return;
	}
	tb->var_array = smalloc(tb->var_arr_len * sizeof(struct tbl_entry));
	transform_tree(tb->var_array, t);
}


static void __transform_tree(struct tbl_entry *array, struct node *null,
		struct node *x);

static void transform_tree(struct tbl_entry *tbe, struct str_tree *t)
{
	__transform_tree(tbe, t->null, t->root);
}

static void __transform_tree(struct tbl_entry *array, struct node *null,
		struct node *x)
{
	if (x != null) {
		__transform_tree(array, null, x->left);
		__transform_tree(array, null, x->right);
		array[x->value].name = x->key;
		free(x);
	}
}

static void debug_function_remapped(struct lexem_list *l,
		struct tbl_entry *array, int len)
{
	int i;
	if (debug[function_remapped] == 0)
		return;
	fprintf(stderr, "---Function remapped---\n");
	ll_print(stderr, l);
	fprintf(stderr, "Variables:\n");
	for (i = 0; i < len; ++i)
		printf("\t%d: \'%s\'\n", i, array[i].name);
	fprintf(stderr, "\n");
}

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
	struct node *z = getnode(t);

	while (x != t->null) {
		int val;
		y = x;
		val = strcmp(y->key, s);

		if (val == 0)
			return y->value;
		if (val > 0) {
			x = x->left;
			last = 0;
		} else {
			x = x->right;
			last = 1;
		}
	}
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
