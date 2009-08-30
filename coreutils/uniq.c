/* vi: set sw=4 ts=4: */
/*
 * uniq implementation for busybox
 *
 * Copyright (C) 2005  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/uniq.html */

#include "libbb.h"

static FILE *xgetoptfile_uniq_s(char **argv, int read0write2)
{
	const char *n;

	n = *argv;
	if (n != NULL) {
		if ((n[0] != '-') || n[1]) {
			return xfopen(n, "r\0w" + read0write2);
		}
	}
	return (read0write2) ? stdout : stdin;
}

int uniq_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uniq_main(int argc UNUSED_PARAM, char **argv)
{
	FILE *in, *out;
	const char *input_filename;
	unsigned skip_fields, skip_chars, max_chars;
	unsigned opt;
	char *cur_line;
	const char *cur_compare;

	enum {
		OPT_c = 0x1,
		OPT_d = 0x2, /* print only dups */
		OPT_u = 0x4, /* print only uniq */
		OPT_f = 0x8,
		OPT_s = 0x10,
		OPT_w = 0x20,
	};

	skip_fields = skip_chars = 0;
	max_chars = INT_MAX;

	opt_complementary = "f+:s+:w+";
	opt = getopt32(argv, "cduf:s:w:", &skip_fields, &skip_chars, &max_chars);
	argv += optind;

	input_filename = *argv;

	in = xgetoptfile_uniq_s(argv, 0);
	if (*argv) {
		++argv;
	}
	out = xgetoptfile_uniq_s(argv, 2);
	if (*argv && argv[1]) {
		bb_show_usage();
	}

	cur_compare = cur_line = NULL; /* prime the pump */

	do {
		unsigned i;
		unsigned long dups;
		char *old_line;
		const char *old_compare;

		old_line = cur_line;
		old_compare = cur_compare;
		dups = 0;

		/* gnu uniq ignores newlines */
		while ((cur_line = xmalloc_fgetline(in)) != NULL) {
			cur_compare = cur_line;
			for (i = skip_fields; i; i--) {
				cur_compare = skip_whitespace(cur_compare);
				cur_compare = skip_non_whitespace(cur_compare);
			}
			for (i = skip_chars; *cur_compare && i; i--) {
				++cur_compare;
			}

			if (!old_line || strncmp(old_compare, cur_compare, max_chars)) {
				break;
			}

			++dups;	 /* testing for overflow seems excessive */
		}

		if (old_line) {
			if (!(opt & (OPT_d << !!dups))) { /* (if dups, opt & OPT_u) */
				fprintf(out, "\0%lu " + (opt & 1), dups + 1); /* 1 == OPT_c */
				fprintf(out, "%s\n", old_line);
			}
			free(old_line);
		}
	} while (cur_line);

	die_if_ferror(in, input_filename);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
