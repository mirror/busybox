/* vi: set sw=8 ts=8: */
/*
 * cut.c - minimalist version of cut
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Mark Whitley <markw@codepoet.org>
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
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "busybox.h"


/* option vars */
static const char optstring[] = "b:c:f:d:sn";
#define OPT_BYTE_FLGS    1
#define OPT_CHAR_FLGS    2
#define OPT_FIELDS_FLGS  4
#define OPT_DELIM_FLGS   8
#define OPT_SUPRESS_FLGS 16
static char part; /* (b)yte, (c)har, (f)ields */
static unsigned int supress_non_delimited_lines;
static char delim = '\t'; /* delimiter, default is tab */

struct cut_list {
	int startpos;
	int endpos;
};

static const int BOL = 0;
static const int EOL = INT_MAX;
static const int NON_RANGE = -1;

static struct cut_list *cut_lists = NULL; /* growable array holding a series of lists */
static unsigned int nlists = 0; /* number of elements in above list */


static int cmpfunc(const void *a, const void *b)
{
	struct cut_list *la = (struct cut_list *)a;
	struct cut_list *lb = (struct cut_list *)b;

	if (la->startpos > lb->startpos)
		return 1;
	if (la->startpos < lb->startpos)
		return -1;
	return 0;
}


/*
 * parse_lists() - parses a list and puts values into startpos and endpos.
 * valid list formats: N, N-, N-M, -M
 * more than one list can be seperated by commas
 */
static void parse_lists(char *lists)
{
	char *ltok = NULL;
	char *ntok = NULL;
	char *junk;
	int s = 0, e = 0;

	/* take apart the lists, one by one (they are seperated with commas */
	while ((ltok = strsep(&lists, ",")) != NULL) {

		/* it's actually legal to pass an empty list */
		if (strlen(ltok) == 0)
			continue;

		/* get the start pos */
		ntok = strsep(&ltok, "-");
		if (ntok == NULL) {
			fprintf(stderr, "Help ntok is null for starting position! What do I do?\n");
		} else if (strlen(ntok) == 0) {
			s = BOL;
		} else {
			s = strtoul(ntok, &junk, 10);
			if(*junk != '\0' || s < 0)
				bb_error_msg_and_die("invalid byte or field list");

			/* account for the fact that arrays are zero based, while the user
			 * expects the first char on the line to be char # 1 */
			if (s != 0)
				s--;
		}

		/* get the end pos */
		ntok = strsep(&ltok, "-");
		if (ntok == NULL) {
			e = NON_RANGE;
		} else if (strlen(ntok) == 0) {
			e = EOL;
		} else {
			e = strtoul(ntok, &junk, 10);
			if(*junk != '\0' || e < 0)
				bb_error_msg_and_die("invalid byte or field list");
			/* if the user specified and end position of 0, that means "til the
			 * end of the line */
			if (e == 0)
				e = INT_MAX;
			e--; /* again, arrays are zero based, lines are 1 based */
			if (e == s)
				e = NON_RANGE;
		}

		/* if there's something left to tokenize, the user past an invalid list */
		if (ltok)
			bb_error_msg_and_die("invalid byte or field list");

		/* add the new list */
		cut_lists = xrealloc(cut_lists, sizeof(struct cut_list) * (++nlists));
		cut_lists[nlists-1].startpos = s;
		cut_lists[nlists-1].endpos = e;
	}

	/* make sure we got some cut positions out of all that */
	if (nlists == 0)
		bb_error_msg_and_die("missing list of positions");

	/* now that the lists are parsed, we need to sort them to make life easier
	 * on us when it comes time to print the chars / fields / lines */
	qsort(cut_lists, nlists, sizeof(struct cut_list), cmpfunc);

}


static void cut_line_by_chars(const char *line)
{
	int c, l;
	/* set up a list so we can keep track of what's been printed */
	char *printed = xcalloc(strlen(line), sizeof(char));

	/* print the chars specified in each cut list */
	for (c = 0; c < nlists; c++) {
		l = cut_lists[c].startpos;
		while (l < strlen(line)) {
			if (!printed[l]) {
				putchar(line[l]);
				printed[l] = 'X';
			}
			l++;
			if (cut_lists[c].endpos == NON_RANGE || l > cut_lists[c].endpos)
				break;
		}
	}
	putchar('\n'); /* cuz we were handed a chomped line */
	free(printed);
}


static void cut_line_by_fields(char *line)
{
	int c, f;
	int ndelim = -1; /* zero-based / one-based problem */
	int nfields_printed = 0;
	char *field = NULL;
	char d[2] = { delim, 0 };
	char *printed;

	/* test the easy case first: does this line contain any delimiters? */
	if (strchr(line, delim) == NULL) {
		if (!supress_non_delimited_lines)
			puts(line);
		return;
	}

	/* set up a list so we can keep track of what's been printed */
	printed = xcalloc(strlen(line), sizeof(char));

	/* process each list on this line, for as long as we've got a line to process */
	for (c = 0; c < nlists && line; c++) {
		f = cut_lists[c].startpos;
		do {

			/* find the field we're looking for */
			while (line && ndelim < f) {
				field = strsep(&line, d);
				ndelim++;
			}

			/* we found it, and it hasn't been printed yet */
			if (field && ndelim == f && !printed[ndelim]) {
				/* if this isn't our first time through, we need to print the
				 * delimiter after the last field that was printed */
				if (nfields_printed > 0)
					putchar(delim);
				fputs(field, stdout);
				printed[ndelim] = 'X';
				nfields_printed++;
			}

			f++;

			/* keep going as long as we have a line to work with, this is a
			 * list, and we're not at the end of that list */
		} while (line && cut_lists[c].endpos != NON_RANGE && f <= cut_lists[c].endpos);
	}

	/* if we printed anything at all, we need to finish it with a newline cuz
	 * we were handed a chomped line */
	putchar('\n');

	free(printed);
}


static void cut_file_by_lines(const char *line, unsigned int linenum)
{
	static int c = 0;
	static int l = -1;

	/* I can't initialize this above cuz the "initializer isn't
	 * constant" *sigh* */
	if (l == -1)
		l = cut_lists[c].startpos;

	/* get out if we have no more lists to process or if the lines are lower
	 * than what we're interested in */
	if (c >= nlists || linenum < l)
		return;

	/* if the line we're looking for is lower than the one we were passed, it
	 * means we displayed it already, so move on */
	while (l < linenum) {
		l++;
		/* move on to the next list if we're at the end of this one */
		if (cut_lists[c].endpos == NON_RANGE || l > cut_lists[c].endpos) {
			c++;
			/* get out if there's no more lists to process */
			if (c >= nlists)
				return;
			l = cut_lists[c].startpos;
			/* get out if the current line is lower than the one we just became
			 * interested in */
			if (linenum < l)
				return;
		}
	}

	/* If we made it here, it means we've found the line we're looking for, so print it */
	puts(line);
}


/*
 * snippy-snip
 */
static void cut_file(FILE *file)
{
	char *line = NULL;
	unsigned int linenum = 0; /* keep these zero-based to be consistent */

	/* go through every line in the file */
	while ((line = bb_get_chomped_line_from_file(file)) != NULL) {

		/* cut based on chars/bytes XXX: only works when sizeof(char) == byte */
		if ((part & (OPT_CHAR_FLGS | OPT_BYTE_FLGS)))
			cut_line_by_chars(line);

		/* cut based on fields */
		else {
			if (delim == '\n')
				cut_file_by_lines(line, linenum);
			else
				cut_line_by_fields(line);
		}

		linenum++;
		free(line);
	}
}


extern int cut_main(int argc, char **argv)
{
	unsigned long opt;
	char *sopt, *sdopt;

	bb_opt_complementaly = "b~bcf:c~bcf:f~bcf";
	opt = bb_getopt_ulflags(argc, argv, optstring, &sopt, &sopt, &sopt, &sdopt);
	part = opt & (OPT_BYTE_FLGS|OPT_CHAR_FLGS|OPT_FIELDS_FLGS);
	if(part == 0)
		bb_error_msg_and_die("you must specify a list of bytes, characters, or fields");
	if(opt & 0x80000000UL)
		bb_error_msg_and_die("only one type of list may be specified");
	parse_lists(sopt);
	if((opt & (OPT_DELIM_FLGS))) {
		if (strlen(sdopt) > 1) {
			bb_error_msg_and_die("the delimiter must be a single character");
		}
		delim = sdopt[0];
	}
	supress_non_delimited_lines = opt & OPT_SUPRESS_FLGS;

	/*  non-field (char or byte) cutting has some special handling */
	if (part != OPT_FIELDS_FLGS) {
		if (supress_non_delimited_lines) {
			bb_error_msg_and_die("suppressing non-delimited lines makes sense"
					" only when operating on fields");
		}
		if (delim != '\t') {
			bb_error_msg_and_die("a delimiter may be specified only when operating on fields");
		}
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
			file = bb_wfopen(argv[i], "r");
			if(file) {
				cut_file(file);
				fclose(file);
			}
		}
	}

	return EXIT_SUCCESS;
}
