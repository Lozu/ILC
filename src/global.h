#ifndef UTILS_H
#define UTILS_H

enum {
	dbg_settings,
	dbg_global_lexem_parse,
	dbg_function_header,
	dbg_function_remapped,
	dbg_commands,
	dbg_livespan
};

extern char debug[];

void die(char *fmt, ...);
void warn(char *fmt, ...);
void *smalloc(int size);
char *sstrdup(char *s);

#endif
