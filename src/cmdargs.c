#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
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
const char *ifile_sfx = ".il";
const char *ofile_sfx = ".s";

static void settings_check(struct settings *s);
static void settings_complete(struct settings *sts);
static void settings_print(struct settings *sts);

struct settings * cmdargs_handle(int argc, char **argv)
{
	struct settings *sts = smalloc(sizeof(struct settings));
	if (argc == 1 || (argc == 2 && strcmp(argv[1], flag_help) == 0)) {
		printf(msg_help, argv[0]);
		exit(0);
	}
	for (++argv; *argv; ++argv) {
		if (strcmp(*argv, flag_ofile) == 0) {
			if (argv[1] == NULL)
				die("%s: %s\n", flag_ofile, msg_no_ofile);
			++argv;
			sts->output_file = *argv;
		} else {
			break;
		}
	}
	if (*argv == NULL)
		die("%s\n", msg_no_ifile);
	sts->input_file = *argv;
	if (argv[1] != NULL)
		die("%s: %s\n", argv[1], msg_dup_ifile);
	settings_check(sts);
	settings_complete(sts);
#ifdef DEBUG
	settings_print(sts);
#endif
	return sts;
}

static void settings_check(struct settings *s)
{
	if (*s->input_file == 0)
		die("%s\n", msg_empty_ifile);
	if (s->output_file && *s->output_file == 0)
		die("%s\n", msg_empty_ofile);
}

static void settings_complete(struct settings *sts)
{
	char *tmp;
	int prxlen;

	if (sts->output_file)
		return;
	tmp = strstr(sts->input_file, ifile_sfx);
	if (tmp)
		prxlen = tmp - sts->input_file;
	else
		prxlen = strlen(sts->input_file);
	sts->output_file = smalloc(prxlen + strlen(ofile_sfx) + 1);
	memcpy(sts->output_file, sts->input_file, prxlen);
	memcpy(sts->output_file + prxlen, ofile_sfx, strlen(ofile_sfx));
	sts->output_file[prxlen + strlen(ofile_sfx)] = 0;
}

static void settings_print(struct settings *sts)
{
	printf("Input file: %s\n", sts->input_file);
	printf("Output file: %s\n", sts->output_file);
}
