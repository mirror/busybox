/* vi: set sw=4 ts=4: */
/*
 * Mini head implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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

#include "internal.h"
#include <errno.h>
#include <stdio.h>

const char head_usage[] =
	"head [OPTION] [FILE]...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nPrint first 10 lines of each FILE to standard output.\n"
	"With more than one FILE, precede each with a header giving the\n"
	"file name. With no FILE, or when FILE is -, read standard input.\n\n"

	"Options:\n" "\t-n NUM\t\tPrint first NUM lines instead of first 10\n"
#endif
	;

int head(int len, FILE * src)
{
	int i;
	char buffer[1024];

	for (i = 0; i < len; i++) {
		fgets(buffer, 1024, src);
		if (feof(src)) {
			break;
		}
		fputs(buffer, stdout);
	}
	return 0;
}

/* BusyBoxed head(1) */
int head_main(int argc, char **argv)
{
	char opt;
	int len = 10, tmplen, i;

	/* parse argv[] */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			opt = argv[i][1];
			switch (opt) {
			case 'n':
				tmplen = 0;
				if (++i < argc)
					tmplen = atoi(argv[i]);
				if (tmplen < 1)
					usage(head_usage);
				len = tmplen;
				break;
			case '-':
			case 'h':
				usage(head_usage);
			default:
				errorMsg("invalid option -- %c\n", opt);
				usage(head_usage);
			}
		} else {
			break;
		}
	}

	/* get rest of argv[] or stdin if nothing's left */
	if (i >= argc) {
		head(len, stdin);

	} else {
		int need_headers = ((argc - i) > 1);

		for (; i < argc; i++) {
			FILE *src;

			src = fopen(argv[i], "r");
			if (!src) {
				errorMsg("%s: %s\n", argv[i], strerror(errno));
			} else {
				/* emulating GNU behaviour */
				if (need_headers) {
					fprintf(stdout, "==> %s <==\n", argv[i]);
				}
				head(len, src);
				if (i < argc - 1) {
					fprintf(stdout, "\n");
				}
			}
		}
	}
	return(0);
}

/* $Id: head.c,v 1.12 2000/07/14 01:51:25 kraai Exp $ */
