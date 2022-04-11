#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "cmdargs.h"

const char *msg_help =
"Usage: %s [-o <resfile> ] <file>\n"
"    Run program without any arguments or with only '--help' argument, to\n"
"    get brief help.\n\n"
"    -o <resfile>      Specify name of the output file.\n";
const char *flag_help = "--help";
const char *flag_ofile = "-o";
const char *msg_no_ofile = "missing output file";
const char *msg_no_ifile = "no input file specified";
const char *msg_dup_ifile = "input file already given";
const char *msg_empty_ifile = "input file name is empty string";
const char *msg_empty_ofile = "outputfile name is empty string";
const char *msg_ifile_name_too_long = "input file name is too long";
const char *msg_ofile_name_too_long = "output file name is too long";

const char *ifile_sfx = ".il";
const char *ofile_sfx = ".s";

static void settings_check(struct settings *s);
static void settings_complete(struct settings *sts);
static void debug_settings_print(struct settings *sts);

void cmdargs_handle(int argc, char **argv, struct settings *s)
{
	s->output_file = NULL;

	if (argc == 1 || (argc == 2 && strcmp(argv[1], flag_help) == 0)) {
		printf(msg_help, argv[0]);
		exit(0);
	}
	for (++argv; *argv; ++argv) {
		if (strcmp(*argv, flag_ofile) == 0) {
			if (argv[1] == NULL)
				die("%s: %s\n", flag_ofile, msg_no_ofile);
			++argv;
			s->output_file = *argv;
		} else {
			break;
		}
	}
	if (*argv == NULL)
		die("%s\n", msg_no_ifile);
	s->input_file = *argv;
	if (argv[1] != NULL)
		die("%s: %s\n", argv[1], msg_dup_ifile);
	settings_check(s);
	settings_complete(s);
	debug_settings_print(s);
}

static void settings_check(struct settings *s)
{
	if (s->input_file[0] == 0)
		die("%s\n", msg_empty_ifile);
	if (s->output_file && s->output_file[0] == 0)
		die("%s\n", msg_empty_ofile);
	if (strlen(s->input_file) > 256)
		die("%s\n", msg_ifile_name_too_long);
	if (s->output_file && strlen(s->output_file) > 256)
		die("%s\n", msg_ofile_name_too_long);
}

static int ends_with(char *s1, char *s2);

static void settings_complete(struct settings *sts)
{
	char buffer[267];
	char *tmp;
	int pos;

	if (sts->output_file)
		return;

	tmp = strrchr(sts->input_file, '/');
	if (tmp == NULL)
		pos = 0;
	else
		pos = tmp - sts->input_file + 1;
	strcpy(buffer, sts->input_file + pos);

	pos = ends_with(buffer, ifile_sfx);
	strcpy(buffer + pos, ofile_sfx);
	sts->output_file = sstrdup(buffer);
}

static int ends_with(char *s1, char *s2)
{
	int i;
	int offset = strlen(s1) - strlen(s2);
	if (offset < 0)
		return strlen(s1);
	for (i = 0; s1[offset + i] != 0 && s2[i] != 0 &&
			s1[offset + i] == s2[i]; ++i);
	if (s2[i] != 0)
		return strlen(s1);
	return offset;
}

static void debug_settings_print(struct settings *sts)
{
	if (dbg_settings == 0)
		return;
	eprintf("------Settings------\n\n");
	eprintf("Input file: %s\n", sts->input_file);
	eprintf("Output file: %s\n", sts->output_file);
}
