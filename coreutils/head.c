/* vi: set sw=4 ts=4: */
/*
 * Mini head implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
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

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "busybox.h"

static int head(int len, FILE *fp)
{
	int i;
	char *input;

	for (i = 0; i < len; i++) {
		if ((input = get_line_from_file(fp)) == NULL)
			break;
		fputs(input, stdout);
		free(input);
	}
	return 0;
}

/* BusyBoxed head(1) */
int head_main(int argc, char **argv)
{
	FILE *fp;
	int need_headers, opt, len = 10, status = EXIT_SUCCESS;

	if (( argc >= 2 ) && ( argv [1][0] == '-' ) && isdigit ( argv [1][1] )) {
		len = atoi ( &argv [1][1] );
		optind = 2;
	}

	/* parse argv[] */
	while ((opt = getopt(argc, argv, "n:")) > 0) {
		switch (opt) {
		case 'n':
			len = atoi(optarg);
			if (len >= 0)
				break;
			/* fallthrough */
		default:
			show_usage();
		}
	}

	/* get rest of argv[] or stdin if nothing's left */
	if (argv[optind] == NULL) {
		head(len, stdin);
		return status;
	} 

	need_headers = optind != (argc - 1);
	while (argv[optind]) {
		if (strcmp(argv[optind], "-") == 0) {
			fp = stdin;
			argv[optind] = "standard input";
		} else {
			if ((fp = wfopen(argv[optind], "r")) == NULL)
				status = EXIT_FAILURE;
		}
		if (fp) {
			if (need_headers) {
				printf("==> %s <==\n", argv[optind]);
			}
			head(len, fp);
			if (ferror(fp)) {
				perror_msg("%s", argv[optind]);
				status = EXIT_FAILURE;
			}
			if (optind < argc - 1)
				putchar('\n');
			if (fp != stdin)
				fclose(fp);
		}
		optind++;
	}

	return status;
}
