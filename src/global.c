#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"

char dbg_settings = 1;
char dbg_global_lexem_parse = 1;
char dbg_function_header = 1;
char dbg_function_remapped = 1;
char dbg_commands = 1;
char dbg_lifespan = 1;
char dbg_allocation = 1;
char dbg_emit_borders = 1;

static const char *error_prefix = "error";
static const char *warn_prefix = "warning";
static const char *malloc_failed = "malloc failure";

const char *lbl_func_end = ".end";

FILE *output_file = NULL;

static void inform(int mode, char *fmt, va_list vl)
{
	char *prx;
	if (mode == 1)
		prx = error_prefix;
	else
		prx = warn_prefix;
	eprintf("%s:", prx);
	vfprintf(stderr, fmt, vl);
	if (mode == 1)
		exit(1);
}

void die(char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	inform(1, fmt, vl);
	va_end(vl);
}

void warn(char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	inform(0, fmt, vl);
	va_end(vl);
}

void *smalloc(int size)
{
	void *tmp = malloc(size);
	if (tmp == NULL)
		die("%s\n", malloc_failed);
	return tmp;
}

char *sstrdup(char *s)
{
	char *tmp = smalloc(strlen(s) + 1);
	memcpy(tmp, s, strlen(s) + 1);
	return tmp;
}
