/* vi: set sw=4 ts=4: */
/*
 * uniq implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/uniq.html */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "busybox.h"
#include "libcoreutils/coreutils.h"

static const char uniq_opts[] = "f:s:cdu\0\7\3\5\1\2\4";

int uniq_main(int argc, char **argv)
{
	FILE *in, *out;
	/* Note: Ignore the warning about dups and e0 possibly being uninitialized.
	 * They will be initialized on the fist pass of the loop (since s0 is NULL). */
	unsigned long dups, skip_fields, skip_chars, i;
	const char *s0, *e0, *s1, *e1, *input_filename;
	int opt;
	int uniq_flags = 6;		/* -u */

	skip_fields = skip_chars = 0;

	while ((opt = getopt(argc, argv, uniq_opts)) > 0) {
		if (opt == 'f') {
			skip_fields = bb_xgetularg10(optarg);
		} else if (opt == 's') {
			skip_chars = bb_xgetularg10(optarg);
		} else if ((s0 = strchr(uniq_opts, opt)) != NULL) {
			uniq_flags &= s0[4];
			uniq_flags |= s0[7];
		} else {
			bb_show_usage();
		}
	}

	input_filename = *(argv += optind);

	in = xgetoptfile_sort_uniq(argv, "r");
	if (*argv) {
		++argv;
	}
	out = xgetoptfile_sort_uniq(argv, "w");
	if (*argv && argv[1]) {
		bb_show_usage();
	}

	s0 = NULL;

	/* gnu uniq ignores newlines */
	while ((s1 = bb_get_chomped_line_from_file(in)) != NULL) {
		e1 = s1;
		for (i=skip_fields ; i ; i--) {
			e1 = bb_skip_whitespace(e1);
			while (*e1 && !isspace(*e1)) {
				++e1;
			}
		}
		for (i = skip_chars ; *e1 && i ; i--) {
			++e1;
		}
		if (s0) {
			if (strcmp(e0, e1) == 0) {
				++dups;		/* Note: Testing for overflow seems excessive. */
				continue;
			}
		DO_LAST:
			if ((dups && (uniq_flags & 2)) || (!dups && (uniq_flags & 4))) {
				bb_fprintf(out, "\0%7d\t" + (uniq_flags & 1), dups + 1);
				bb_fprintf(out, "%s\n", s0);
			}
			free((void *)s0);
		}

		s0 = s1;
		e0 = e1;
		dups = 0;
	}

	if (s0) {
		e1 = NULL;
		goto DO_LAST;
	}

	bb_xferror(in, input_filename);

	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
