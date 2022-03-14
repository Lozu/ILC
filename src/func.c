#include "utils.h"
#include "lparse.h"
#include "func.h"

struct block {
	struct coord crd;
	enum lexem_type lt;
	union lexem_data dt;
};

void func_header_form(struct lexem_list *l, struct func_header *fh)
{
	struct block b;
	if (ll_get(l, &b.lt, &b.crd, &b.dt) == 0)
		die("");
}
