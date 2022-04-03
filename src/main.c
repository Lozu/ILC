#include "cmdargs.h"
#include "process.h"

int main(int argc, char **argv)
{
	struct settings s;
	cmdargs_handle(argc, argv, &s);
	process(&s);
	return 0;
}
