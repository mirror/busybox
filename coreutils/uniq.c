/* vi: set sw=4 ts=4: */
/*
 * uniq implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
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

/* The extra data is flags to make -d and -u switch each other off */
static const char uniq_opts[] = "cudf:s:\0\7\3\5\1\2\4";

#define SHOW_COUNT		1
#define SHOW_UNIQUE		2
#define SHOW_DUPLICATE	4

static FILE *open_arg(char **argv, char *mode)
{
	char *n=*argv;

	return (n && *n != '-' && n[1]) ? bb_xfopen(n, mode) :
		*mode=='r' ? stdin : stdout;
}


int uniq_main(int argc, char **argv)
{
	FILE *in, *out;
	unsigned long dups, skip_fields, skip_chars, i;
	const char *oldline, *oldskipped, *line, *skipped, *input_filename;
	int opt;
	int uniq_flags = SHOW_UNIQUE | SHOW_DUPLICATE;

	skip_fields = skip_chars = 0;

	while ((opt = getopt(argc, argv, uniq_opts)) > 0) {
		if (opt == 'f') skip_fields = bb_xgetularg10(optarg);
		else if (opt == 's') skip_chars = bb_xgetularg10(optarg);

		/* This bit uses the extra data at the end of uniq_opts to make
		 * -d and -u switch each other off in a very small amount of space */
		
		else if ((line = strchr(uniq_opts, opt)) != NULL) {
			uniq_flags &= line[8];
			uniq_flags |= line[11];
		} else bb_show_usage();
	}

	input_filename = *(argv += optind);

	in = open_arg(argv, "r");
	if (*argv) ++argv;
	out = open_arg(argv, "w");
	if (*argv && argv[1]) bb_show_usage();

	line = skipped = NULL;

NOT_DUPLICATE:
	oldline = line;
	oldskipped = skipped;
	dups = 0;
	
	/* gnu uniq ignores newlines */
	while ((line = bb_get_chomped_line_from_file(in)) != NULL) {
		skipped = line;
		for (i=skip_fields ; i ; i--) {
			skipped = bb_skip_whitespace(skipped);
			while (*skipped && !isspace(*skipped)) ++skipped;
		}
		for (i = skip_chars ; *skipped && i ; i--) ++skipped;
		if (oldline) {
			if (!strcmp(oldskipped, skipped)) {
				++dups;		/* Note: Testing for overflow seems excessive. */
				continue;
			}
DO_LAST:
			if (uniq_flags & (dups ? SHOW_DUPLICATE : SHOW_UNIQUE)) {
				bb_fprintf(out, "\0%7d " + (uniq_flags & SHOW_COUNT), dups + 1);
				bb_fprintf(out, "%s\n", oldline);
			}
			free((void *)oldline);
		}
		goto NOT_DUPLICATE;
	}

	if (oldline) goto DO_LAST;

	bb_xferror(in, input_filename);

	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
