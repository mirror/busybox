/* vi: set sw=4 ts=4: */
/*
 * cut.c - minimalist version of cut
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Mark Whitley <markw@codepoet.org>
 * debloated by Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

/* option vars */
static const char optstring[] = "b:c:f:d:sn";
#define CUT_OPT_BYTE_FLGS	(1<<0)
#define CUT_OPT_CHAR_FLGS	(1<<1)
#define CUT_OPT_FIELDS_FLGS	(1<<2)
#define CUT_OPT_DELIM_FLGS	(1<<3)
#define CUT_OPT_SUPPRESS_FLGS (1<<4)

static char delim = '\t';	/* delimiter, default is tab */

struct cut_list {
	int startpos;
	int endpos;
};

enum {
	BOL = 0,
	EOL = INT_MAX,
	NON_RANGE = -1
};

/* growable array holding a series of lists */
static struct cut_list *cut_lists;
static unsigned int nlists;	/* number of elements in above list */


static int cmpfunc(const void *a, const void *b)
{
	return (((struct cut_list *) a)->startpos -
			((struct cut_list *) b)->startpos);

}

static void cut_file(FILE * file)
{
	char *line = NULL;
	unsigned int linenum = 0;	/* keep these zero-based to be consistent */

	/* go through every line in the file */
	while ((line = xmalloc_getline(file)) != NULL) {

		/* set up a list so we can keep track of what's been printed */
		char * printed = xzalloc(strlen(line) * sizeof(char));
		char * orig_line = line;
		unsigned int cl_pos = 0;
		int spos;

		/* cut based on chars/bytes XXX: only works when sizeof(char) == byte */
		if (option_mask32 & (CUT_OPT_CHAR_FLGS | CUT_OPT_BYTE_FLGS)) {
			/* print the chars specified in each cut list */
			for (; cl_pos < nlists; cl_pos++) {
				spos = cut_lists[cl_pos].startpos;
				while (spos < strlen(line)) {
					if (!printed[spos]) {
						printed[spos] = 'X';
						putchar(line[spos]);
					}
					spos++;
					if (spos > cut_lists[cl_pos].endpos
						|| cut_lists[cl_pos].endpos == NON_RANGE)
						break;
				}
			}
		} else if (delim == '\n') {	/* cut by lines */
			spos = cut_lists[cl_pos].startpos;

			/* get out if we have no more lists to process or if the lines
			 * are lower than what we're interested in */
			if (linenum < spos || cl_pos >= nlists)
				goto next_line;

			/* if the line we're looking for is lower than the one we were
			 * passed, it means we displayed it already, so move on */
			while (spos < linenum) {
				spos++;
				/* go to the next list if we're at the end of this one */
				if (spos > cut_lists[cl_pos].endpos
					|| cut_lists[cl_pos].endpos == NON_RANGE) {
					cl_pos++;
					/* get out if there's no more lists to process */
					if (cl_pos >= nlists)
						goto next_line;
					spos = cut_lists[cl_pos].startpos;
					/* get out if the current line is lower than the one
					 * we just became interested in */
					if (linenum < spos)
						goto next_line;
				}
			}

			/* If we made it here, it means we've found the line we're
			 * looking for, so print it */
			puts(line);
			goto next_line;
		} else {		/* cut by fields */
			int ndelim = -1;	/* zero-based / one-based problem */
			int nfields_printed = 0;
			char *field = NULL;
			const char delimiter[2] = { delim, 0 };

			/* does this line contain any delimiters? */
			if (strchr(line, delim) == NULL) {
				if (!(option_mask32 & CUT_OPT_SUPPRESS_FLGS))
					puts(line);
				goto next_line;
			}

			/* process each list on this line, for as long as we've got
			 * a line to process */
			for (; cl_pos < nlists && line; cl_pos++) {
				spos = cut_lists[cl_pos].startpos;
				do {
					/* find the field we're looking for */
					while (line && ndelim < spos) {
						field = strsep(&line, delimiter);
						ndelim++;
					}

					/* we found it, and it hasn't been printed yet */
					if (field && ndelim == spos && !printed[ndelim]) {
						/* if this isn't our first time through, we need to
						 * print the delimiter after the last field that was
						 * printed */
						if (nfields_printed > 0)
							putchar(delim);
						fputs(field, stdout);
						printed[ndelim] = 'X';
						nfields_printed++;	/* shouldn't overflow.. */
					}

					spos++;

					/* keep going as long as we have a line to work with,
					 * this is a list, and we're not at the end of that
					 * list */
				} while (spos <= cut_lists[cl_pos].endpos && line
						 && cut_lists[cl_pos].endpos != NON_RANGE);
			}
		}
		/* if we printed anything at all, we need to finish it with a
		 * newline cuz we were handed a chomped line */
		putchar('\n');
 next_line:
		linenum++;
		free(printed);
		free(orig_line);
	}
}

static const char _op_on_field[] = " only when operating on fields";

int cut_main(int argc, char **argv);
int cut_main(int argc, char **argv)
{
	char *sopt, *ltok;

	opt_complementary = "b--bcf:c--bcf:f--bcf";
	getopt32(argc, argv, optstring, &sopt, &sopt, &sopt, &ltok);
//	argc -= optind;
	argv += optind;
	if (!(option_mask32 & (CUT_OPT_BYTE_FLGS | CUT_OPT_CHAR_FLGS | CUT_OPT_FIELDS_FLGS)))
		bb_error_msg_and_die("expected a list of bytes, characters, or fields");
	if (option_mask32 & BB_GETOPT_ERROR)
		bb_error_msg_and_die("only one type of list may be specified");

	if (option_mask32 & CUT_OPT_DELIM_FLGS) {
		if (strlen(ltok) > 1) {
			bb_error_msg_and_die("the delimiter must be a single character");
		}
		delim = ltok[0];
	}

	/*  non-field (char or byte) cutting has some special handling */
	if (!(option_mask32 & CUT_OPT_FIELDS_FLGS)) {
		if (option_mask32 & CUT_OPT_SUPPRESS_FLGS) {
			bb_error_msg_and_die
				("suppressing non-delimited lines makes sense%s",
				 _op_on_field);
		}
		if (delim != '\t') {
			bb_error_msg_and_die
				("a delimiter may be specified%s", _op_on_field);
		}
	}

	/*
	 * parse list and put values into startpos and endpos.
	 * valid list formats: N, N-, N-M, -M
	 * more than one list can be separated by commas
	 */
	{
		char *ntok;
		int s = 0, e = 0;

		/* take apart the lists, one by one (they are separated with commas */
		while ((ltok = strsep(&sopt, ",")) != NULL) {

			/* it's actually legal to pass an empty list */
			if (strlen(ltok) == 0)
				continue;

			/* get the start pos */
			ntok = strsep(&ltok, "-");
			if (ntok == NULL) {
				bb_error_msg
					("internal error: ntok is null for start pos!?\n");
			} else if (strlen(ntok) == 0) {
				s = BOL;
			} else {
				s = xatoi_u(ntok);
				/* account for the fact that arrays are zero based, while
				 * the user expects the first char on the line to be char #1 */
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
				e = xatoi_u(ntok);
				/* if the user specified and end position of 0, that means "til the
				 * end of the line */
				if (e == 0)
					e = EOL;
				e--;	/* again, arrays are zero based, lines are 1 based */
				if (e == s)
					e = NON_RANGE;
			}

			/* if there's something left to tokenize, the user passed
			 * an invalid list */
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

		/* now that the lists are parsed, we need to sort them to make life
		 * easier on us when it comes time to print the chars / fields / lines
		 */
		qsort(cut_lists, nlists, sizeof(struct cut_list), cmpfunc);
	}

	/* argv[0..argc-1] should be names of file to process. If no
	 * files were specified or '-' was specified, take input from stdin.
	 * Otherwise, we process all the files specified. */
	if (argv[0] == NULL || LONE_DASH(argv[0])) {
		cut_file(stdin);
	} else {
		FILE *file;

		do {
			file = fopen_or_warn(argv[0], "r");
			if (file) {
				cut_file(file);
				fclose(file);
			}
		} while (*++argv);
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		free(cut_lists);
	return EXIT_SUCCESS;
}
