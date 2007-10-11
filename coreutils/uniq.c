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

static const char uniq_opts[] ALIGN1 = "cdu" "f:s:" "cdu\0\1\2\4";

static FILE *xgetoptfile_uniq_s(char **argv, int read0write2)
{
	const char *n;

	n = *argv;
	if (n != NULL) {
		if ((*n != '-') || n[1]) {
			return xfopen(n, "r\0w" + read0write2);
		}
	}
	return (read0write2) ? stdout : stdin;
}

int uniq_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uniq_main(int argc, char **argv)
{
	FILE *in, *out;
	unsigned long dups, skip_fields, skip_chars, i;
	const char *s0, *e0, *s1, *e1, *input_filename;
	unsigned opt;

	enum {
		OPT_c = 0x1,
		OPT_d = 0x2,
		OPT_u = 0x4,
		OPT_f = 0x8,
		OPT_s = 0x10,
	};

	skip_fields = skip_chars = 0;

	opt = getopt32(argv, "cduf:s:", &s0, &s1);
	if (opt & OPT_f)
		skip_fields = xatoul(s0);
	if (opt & OPT_s)
		skip_chars = xatoul(s1);
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
				e1 = skip_non_whitespace(e1);
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
			if (!(opt & (OPT_d << !!dups))) { /* (if dups, opt & OPT_e) */
				fprintf(out, "\0%d " + (opt & 1), dups + 1);
				fprintf(out, "%s\n", s0);
			}
			free((void *)s0);
		}
	} while (s1);

	die_if_ferror(in, input_filename);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
