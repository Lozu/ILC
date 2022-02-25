#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "utils.h"

const char *error_prefix = "error";
const char *malloc_failed = "Malloc failure";

void die(char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	fprintf(stderr, "%s: ", error_prefix);
	vfprintf(stderr, fmt, vl);
	va_end(vl);
	exit(1);
}

void *smalloc(int size)
{
	void *tmp = malloc(size);
	if (tmp == NULL)
		die("%s\n", malloc_failed);
	return tmp;
}
