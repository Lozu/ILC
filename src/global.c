#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"

char debug[] = {
	[dbg_settings] = 1,
	[dbg_global_lexem_parse] = 1,
	[dbg_function_header] = 1,
	[dbg_function_remapped] = 1,
	[dbg_commands] = 1,
	[dbg_lifespan] = 1,
	[dbg_allocation] = 1
};

static const char *error_prefix = "error";
static const char *warn_prefix = "warning";
static const char *malloc_failed = "malloc failure";

static void inform(int mode, char *fmt, va_list vl)
{
	char *prx;
	if (mode == 1)
		prx = error_prefix;
	else
		prx = warn_prefix;
	fprintf(stderr, "%s:", prx);
	vfprintf(stderr, fmt, vl);
	if (mode == 1) {
		exit(1);
	}
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
