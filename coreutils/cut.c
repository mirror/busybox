/*
 * cut.c - minimalist version of cut
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by Mark Whitley <markw@lineo.com>, <markw@enol.com>
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
#include <stdlib.h>
#include <unistd.h> /* getopt */
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "busybox.h"


/* globals from other files */
extern int optind;
extern char *optarg;


/* globals in this file only */
static char part = 0; /* (b)yte, (c)har, (f)ields */
static int startpos = 1;
static int endpos = -1;
static char delim = '\t'; /* delimiter, default is tab */
static unsigned int supress_non_delimited_lines = 0;


/*
 * decompose_list() - parses a list and puts values into startpos and endpos.
 * valid list formats: N, N-, N-M, -M 
 */
static void decompose_list(const char *list)
{
	unsigned int nminus = 0;
	char *ptr;

	/* the list must contain only digits and no more than one minus sign */
	for (ptr = (char *)list; *ptr; ptr++) {
		if (!isdigit(*ptr) && *ptr != '-') {
			fatalError("invalid byte or field list\n");
		}
		if (*ptr == '-') {
			nminus++;
			if (nminus > 1) {
				fatalError("invalid byte or field list\n");
			}
		}
	}

	/* handle single value 'N' case */
	if (nminus == 0) {
		startpos = strtol(list, &ptr, 10);
		if (startpos == 0) {
			fatalError("missing list of fields\n");
		}
		endpos = startpos;
	}
	/* handle multi-value cases */
	else if (nminus == 1) {
		/* handle 'N-' case */
		if (list[strlen(list) - 1] == '-') {
			startpos = strtol(list, &ptr, 10);
		}
		/* handle '-M' case */
		else if (list[0] == '-') {
			endpos = strtol(&list[1], NULL, 10);
		}
		/* handle 'N-M' case */
		else {
			startpos = strtol(list, &ptr, 10);
			endpos = strtol(ptr+1, &ptr, 10);
		}

		/* a sanity check */
		if (startpos == 0) {
			startpos = 1;
		}
	}
}


/*
 * snippy-snip
 */
static void cut_file(FILE *file)
{
	char *line;
	unsigned int cr_hits = 0;

	/* go through every line in the file */
	for (line = NULL; (line = get_line_from_file(file)) != NULL; free(line)) {
		/* cut based on chars/bytes */
		if (part == 'c' || part == 'b') {
			int i;
			/* a valid end position has been specified */
			if (endpos > 0) {
				for (i = startpos-1; i < endpos; i++) {
					fputc(line[i], stdout);
				}
				fputc('\n', stdout);
			}
			/* otherwise, just go to the end of the line */
			else {
				for (i = startpos-1; line[i]; i++) {
					fputc(line[i], stdout);
				}
			}
		} 
		/* cut based on fields */
		else if (part == 'f') {
			char *ptr;
			char *start = line;
			unsigned int delims_hit = 0;

			if (delim == '\n') {
				cr_hits++;
				if (cr_hits >= startpos && cr_hits <= endpos) {
					while (*start && *start != '\n') {
						fputc(*start, stdout);
						start++;
					}
					fputc('\n', stdout);
				}
			}
			else {
				for (ptr = line; (ptr = strchr(ptr, delim)) != NULL; ptr++) {
					delims_hit++;
					if (delims_hit == (startpos - 1)) {
						start = ptr+1;
					}
					if (delims_hit == endpos) {
						break;
					}
				}
			
				/* we didn't hit any delimeters */
				if (delims_hit == 0 && !supress_non_delimited_lines) {
					fputs(line, stdout);
				}
				/* we =did= hit some delimiters */
				else if (delims_hit > 0) {
					/* we have a fixed end point */
					if (ptr) {
						while (start < ptr) {
							fputc(*start, stdout);
							start++;
						}
						fputc('\n', stdout);
					}
					/* or we're just going til the end of the line */
					else {
						while (*start) {
							fputc(*start, stdout);
							start++;
						}
					}
				}
			}
		}
	}
}

extern int cut_main(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "b:c:d:f:ns")) > 0) {
		switch (opt) {
			case 'b':
			case 'c':
			case 'f':
				/* make sure they didn't ask for two types of lists */
				if (part != 0) {
					fatalError("only one type of list may be specified");
				}
				part = (char)opt;
				decompose_list(optarg);
				break;
			case 'd':
				if (strlen(optarg) > 1) {
					fatalError("the delimiter must be a single character\n");
				}
				delim = optarg[0];
				break;
			case 'n':
				/* no-op */
				break;
			case 's':
				supress_non_delimited_lines++;
				break;
		}
	}

	if (part == 0) {
		fatalError("you must specify a list of bytes, characters, or fields\n");
	}

	if (supress_non_delimited_lines && part != 'f') {
		fatalError("suppressing non-delimited lines makes sense
	only when operating on fields\n");
	}

	if (delim != '\t' && part != 'f') {
		fatalError("a delimiter may be specified only when operating on fields\n");
	}

	/* argv[(optind)..(argc-1)] should be names of file to process. If no
	 * files were specified or '-' was specified, take input from stdin.
	 * Otherwise, we process all the files specified. */
	if (argv[optind] == NULL || (strcmp(argv[optind], "-") == 0)) {
		cut_file(stdin);
	}
	else {
		int i;
		FILE *file;
		for (i = optind; i < argc; i++) {
			file = fopen(argv[i], "r");
			if (file == NULL) {
				errorMsg("%s: %s\n", argv[i], strerror(errno));
			} else {
				cut_file(file);
				fclose(file);
			}
		}
	}

	return 0;
}
