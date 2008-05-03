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
		if ((*n != '-') || n[1]) {
			return xfopen(n, "r\0w" + read0write2);
		}
	}
	return (read0write2) ? stdout : stdin;
}

int uniq_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uniq_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	FILE *in, *out;
	const char *s0, *e0, *s1, *e1, *input_filename;
	unsigned long dups;
	unsigned skip_fields, skip_chars, max_chars;
	unsigned opt;
	unsigned i;

	enum {
		OPT_c = 0x1,
		OPT_d = 0x2,
		OPT_u = 0x4,
		OPT_f = 0x8,
		OPT_s = 0x10,
		OPT_w = 0x20,
	};

	skip_fields = skip_chars = 0;
	max_chars = -1;

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

	s1 = e1 = NULL; /* prime the pump */

	do {
		s0 = s1;
		e0 = e1;
		dups = 0;

		/* gnu uniq ignores newlines */
		while ((s1 = xmalloc_fgetline(in)) != NULL) {
			e1 = s1;
			for (i = skip_fields; i; i--) {
				e1 = skip_whitespace(e1);
				e1 = skip_non_whitespace(e1);
			}
			for (i = skip_chars; *e1 && i; i--) {
				++e1;
			}

			if (!s0 || strncmp(e0, e1, max_chars)) {
				break;
			}

			++dups;	 /* note: testing for overflow seems excessive. */
		}

		if (s0) {
			if (!(opt & (OPT_d << !!dups))) { /* (if dups, opt & OPT_e) */
				fprintf(out, "\0%ld " + (opt & 1), dups + 1); /* 1 == OPT_c */
				fprintf(out, "%s\n", s0);
			}
			free((void *)s0);
		}
	} while (s1);

	die_if_ferror(in, input_filename);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
