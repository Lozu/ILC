#include "cmdargs.h"
#include "process.h"

int main(int argc, char **argv)
{
	struct settings *sts = cmdargs_handle(argc, argv);
	process(sts);
	return 0;
}
