#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdio.h>

enum {
	dbg_settings,
	dbg_global_lexem_parse,
	dbg_function_header,
	dbg_function_remapped,
	dbg_commands,
	dbg_lifespan,
	dbg_allocation
};

extern char debug[];
extern FILE *output_file;

static inline void eprintf(char *fmt, ...) {
	va_list vl;
	va_start(vl, fmt);
	vfprintf(stderr, fmt, vl);
	va_end(vl);
}

void die(char *fmt, ...);
void warn(char *fmt, ...);
void *smalloc(int size);
char *sstrdup(char *s);

#endif
