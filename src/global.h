#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdio.h>

extern char dbg_acts_start;
extern char dbg_settings;
extern char dbg_global_lexem_parse;
extern char dbg_gn_borders;
extern char dbg_function_header;
extern char dbg_vars_remapped;
extern char dbg_commands;
extern char dbg_lifespan;
extern char dbg_allocation;
extern char dbg_fcall_list;
extern char dbg_gn_pre;
extern char dbg_gn_post;
extern char dbg_emit_borders;

extern char dbg_free_all_mem;

extern FILE *output_file;

extern const char *lbl_func_end;

#define MIN(a, b) ((a) > (b) ? (a) : (b))

static inline void eprintf(char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	vfprintf(stderr, fmt, vl);
	va_end(vl);
}

static inline void oprintf(char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	vfprintf(output_file, fmt, vl);
	va_end(vl);
}

void die(char *fmt, ...);
void warn(char *fmt, ...);
void *smalloc(int size);
char *sstrdup(char *s);

#endif
