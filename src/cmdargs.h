#ifndef CMDARGS_H
#define CMDARGS_H

struct settings {
	char *input_file;
	char *output_file;
};

struct settings * cmdargs_handle(int argc, char **argv);

#endif
