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

#include "busybox.h"

static const char uniq_opts[] = "f:s:" "cdu\0\1\2\4";

static FILE *xgetoptfile_uniq_s(char **argv, int read0write2)
{
	const char *n;

	if ((n = *argv) != NULL) {
		if ((*n != '-') || n[1]) {
			return xfopen(n, "r\0w" + read0write2);
		}
	}
	return (read0write2) ? stdout : stdin;
}

int uniq_main(int argc, char **argv);
int uniq_main(int argc, char **argv)
{
	FILE *in, *out;
	unsigned long dups, skip_fields, skip_chars, i, uniq_flags;
	const char *s0, *e0, *s1, *e1, *input_filename;
	int opt;

	uniq_flags = skip_fields = skip_chars = 0;

	while ((opt = getopt(argc, argv, uniq_opts)) > 0) {
		if ((opt == 'f') || (opt == 's')) {
			unsigned long t = xatoul(optarg);
			if (opt == 'f') {
				skip_fields = t;
			} else {
				skip_chars = t;
			}
		} else if ((s0 = strchr(uniq_opts, opt)) != NULL) {
			uniq_flags |= s0[4];
		} else {
			bb_show_usage();
		}
	}

	input_filename = *(argv += optind);

	in = xgetoptfile_uniq_s(argv, 0);
	if (*argv) {
		++argv;
	}
	out = xgetoptfile_uniq_s(argv, 2);
	if (*argv && argv[1]) {
		bb_show_usage();
	}

	s1 = e1 = NULL;				/* prime the pump */

	do {
		s0 = s1;
		e0 = e1;
		dups = 0;

		/* gnu uniq ignores newlines */
		while ((s1 = xmalloc_getline(in)) != NULL) {
			e1 = s1;
			for (i = skip_fields; i; i--) {
				e1 = skip_whitespace(e1);
				while (*e1 && !isspace(*e1)) {
					++e1;
				}
			}
			for (i = skip_chars; *e1 && i; i--) {
				++e1;
			}

			if (!s0 || strcmp(e0, e1)) {
				break;
			}

			++dups;		 /* Note: Testing for overflow seems excessive. */
		}

		if (s0) {
			if (!(uniq_flags & (2 << !!dups))) {
				fprintf(out, "\0%d " + (uniq_flags & 1), dups + 1);
				fprintf(out, "%s\n", s0);
			}
			free((void *)s0);
		}
	} while (s1);

	die_if_ferror(in, input_filename);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
