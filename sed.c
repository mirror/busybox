/* vi: set sw=4 ts=4: */
/*
 * Mini sed implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 *
 * Modifications for addresses and append command have been
 * written by Marco Pantaleoni <panta@prosa.it>, <panta@elasticworld.org>
 * and are:
 * Copyright (C) 1999 Marco Pantaleoni.
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
#include "regexp.h"
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

static const char sed_usage[] =
	"sed [-n] -e script [file...]\n\n"
	"Allowed sed scripts come in the following form:\n"
	"\t'ADDR [!] COMMAND'\n\n"
	"\twhere address ADDR can be:\n"
	"\t  NUMBER    Match specified line number\n"
	"\t  $         Match last line\n"
	"\t  /REGEXP/  Match specified regexp\n"
	"\t  (! inverts the meaning of the match)\n\n"
	"\tand COMMAND can be:\n"
	"\t  s/regexp/replacement/[igp]\n"
	"\t     which attempt to match regexp against the pattern space\n"
	"\t     and if successful replaces the matched portion with replacement.\n\n"
	"\t  aTEXT\n"
	"\t     which appends TEXT after the pattern space\n"
	"Options:\n"
	"-e\tadd the script to the commands to be executed\n"
	"-n\tsuppress automatic printing of pattern space\n\n"
#if defined BB_REGEXP
	"This version of sed matches full regular expresions.\n";
#else
	"This version of sed matches strings (not full regular expresions).\n";
#endif

/* Flags & variables */

typedef enum { f_none, f_replace, f_append } sed_function;

#define NO_LINE		-2
#define LAST_LINE	-1
static int addr_line = NO_LINE;
static char *addr_pattern = NULL;
static int negated = 0;

#define SKIPSPACES(p)		do { while (isspace(*(p))) (p)++; } while (0)

#define BUFSIZE		1024

static inline int at_last(FILE * fp)
{
	int res = 0;

	if (feof(fp))
		return 1;
	else {
		char ch;

		if ((ch = fgetc(fp)) == EOF)
			res++;
		ungetc(ch, fp);
	}
	return res;
}

static void do_sed_repl(FILE * fp, char *needle, char *newNeedle,
						int ignoreCase, int printFlag, int quietFlag)
{
	int foundOne = FALSE;
	char haystack[BUFSIZE];
	int line = 1, doit;

	while (fgets(haystack, BUFSIZE - 1, fp)) {
		doit = 0;
		if (addr_pattern) {
			doit = !find_match(haystack, addr_pattern, FALSE);
		} else if (addr_line == NO_LINE)
			doit = 1;
		else if (addr_line == LAST_LINE) {
			if (at_last(fp))
				doit = 1;
		} else {
			if (line == addr_line)
				doit = 1;
		}
		if (negated)
			doit = 1 - doit;
		if (doit) {
			foundOne =
				replace_match(haystack, needle, newNeedle, ignoreCase);

			if (foundOne == TRUE && printFlag == TRUE) {
				fprintf(stdout, haystack);
			}
		}

		if (quietFlag == FALSE) {
			fprintf(stdout, haystack);
		}

		line++;
	}
}

static void do_sed_append(FILE * fp, char *appendline, int quietFlag)
{
	char buffer[BUFSIZE];
	int line = 1, doit;

	while (fgets(buffer, BUFSIZE - 1, fp)) {
		doit = 0;
		if (addr_pattern) {
			doit = !find_match(buffer, addr_pattern, FALSE);
		} else if (addr_line == NO_LINE)
			doit = 1;
		else if (addr_line == LAST_LINE) {
			if (at_last(fp))
				doit = 1;
		} else {
			if (line == addr_line)
				doit = 1;
		}
		if (negated)
			doit = 1 - doit;
		if (quietFlag == FALSE) {
			fprintf(stdout, buffer);
		}
		if (doit) {
			fputs(appendline, stdout);
			fputc('\n', stdout);
		}

		line++;
	}
}

extern int sed_main(int argc, char **argv)
{
	FILE *fp;
	char *needle = NULL, *newNeedle = NULL;
	char *name;
	char *cp;
	int ignoreCase = FALSE;
	int printFlag = FALSE;
	int quietFlag = FALSE;
	int stopNow;
	char *line_s = NULL, saved;
	char *appendline = NULL;
	char *pos;
	sed_function sed_f = f_none;

	argc--;
	argv++;
	if (argc < 1) {
		usage(sed_usage);
	}

	if (**argv == '-') {
		argc--;
		cp = *argv++;
		stopNow = FALSE;

		while (*++cp && stopNow == FALSE) {
			switch (*cp) {
			case 'n':
				quietFlag = TRUE;
				break;
			case 'e':
				if (*(cp + 1) == 0 && --argc < 0) {
					usage(sed_usage);
				}
				if (*++cp != 's')
					cp = *argv++;

				/* Read address if present */
				SKIPSPACES(cp);
				if (*cp == '$') {
					addr_line = LAST_LINE;
					cp++;
				} else {
					if (isdigit(*cp)) {	/* LINE ADDRESS   */
						line_s = cp;
						while (isdigit(*cp))
							cp++;
						if (cp > line_s) {
							/* numeric line */
							saved = *cp;
							*cp = '\0';
							addr_line = atoi(line_s);
							*cp = saved;
						}
					} else if (*cp == '/') {	/* PATTERN ADDRESS */
						pos = addr_pattern = cp + 1;
						pos = strchr(pos, '/');
						if (!pos)
							usage(sed_usage);
						*pos = '\0';
						cp = pos + 1;
					}
				}

				SKIPSPACES(cp);
				if (*cp == '!') {
					negated++;
					cp++;
				}

				/* Read command */

				SKIPSPACES(cp);
				switch (*cp) {
				case 's':		/* REPLACE */
					if (strlen(cp) <= 3 || *(cp + 1) != '/')
						break;
					sed_f = f_replace;

					pos = needle = cp + 2;

					for (;;) {
						pos = strchr(pos, '/');
						if (pos == NULL) {
							usage(sed_usage);
						}
						if (*(pos - 1) == '\\') {
							pos++;
							continue;
						}
						break;
					}
					*pos = 0;
					newNeedle = ++pos;
					for (;;) {
						pos = strchr(pos, '/');
						if (pos == NULL) {
							usage(sed_usage);
						}
						if (*(pos - 1) == '\\') {
							pos++;
							continue;
						}
						break;
					}
					*pos = 0;
					if (pos + 2 != 0) {
						while (*++pos) {
							switch (*pos) {
							case 'i':
								ignoreCase = TRUE;
								break;
							case 'p':
								printFlag = TRUE;
								break;
							case 'g':
								break;
							default:
								usage(sed_usage);
							}
						}
					}
					cp = pos;
					/* fprintf(stderr, "replace '%s' with '%s'\n", needle, newNeedle); */
					break;

				case 'a':		/* APPEND */
					if (strlen(cp) < 2)
						break;
					sed_f = f_append;
					appendline = ++cp;
					/* fprintf(stderr, "append '%s'\n", appendline); */
					break;
				}

				stopNow = TRUE;
				break;

			default:
				usage(sed_usage);
			}
		}
	}

	if (argc == 0) {
		switch (sed_f) {
		case f_none:
			break;
		case f_replace:
			do_sed_repl(stdin, needle, newNeedle, ignoreCase, printFlag,
						quietFlag);
			break;
		case f_append:
			do_sed_append(stdin, appendline, quietFlag);
			break;
		}
	} else {
		while (argc-- > 0) {
			name = *argv++;

			fp = fopen(name, "r");
			if (fp == NULL) {
				perror(name);
				continue;
			}

			switch (sed_f) {
			case f_none:
				break;
			case f_replace:
				do_sed_repl(fp, needle, newNeedle, ignoreCase, printFlag,
							quietFlag);
				break;
			case f_append:
				do_sed_append(fp, appendline, quietFlag);
				break;
			}

			if (ferror(fp))
				perror(name);

			fclose(fp);
		}
	}
	exit(TRUE);
}


/* END CODE */
