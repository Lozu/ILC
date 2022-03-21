#ifndef UTILS_H
#define UTILS_H

enum {
	settings,
	global_lexem_parse,
	function_header,
	function_remapped,
	commands
};

extern char debug[];

void die(char *fmt, ...);
void warn(char *fmt, ...);
void *smalloc(int size);
char *sstrdup(char *s);

#endif
