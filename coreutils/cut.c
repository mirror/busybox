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

#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */


/* option vars */
static const char optstring[] ALIGN1 = "b:c:f:d:sn";
#define CUT_OPT_BYTE_FLGS     (1 << 0)
#define CUT_OPT_CHAR_FLGS     (1 << 1)
#define CUT_OPT_FIELDS_FLGS   (1 << 2)
#define CUT_OPT_DELIM_FLGS    (1 << 3)
#define CUT_OPT_SUPPRESS_FLGS (1 << 4)

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

static void cut_file(FILE *file, char delim)
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

int cut_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cut_main(int argc, char **argv)
{
	char delim = '\t';	/* delimiter, default is tab */
	char *sopt, *ltok;

	opt_complementary = "b--bcf:c--bcf:f--bcf";
	getopt32(argv, optstring, &sopt, &sopt, &sopt, &ltok);
//	argc -= optind;
	argv += optind;
	if (!(option_mask32 & (CUT_OPT_BYTE_FLGS | CUT_OPT_CHAR_FLGS | CUT_OPT_FIELDS_FLGS)))
		bb_error_msg_and_die("expected a list of bytes, characters, or fields");

	if (option_mask32 & CUT_OPT_DELIM_FLGS) {
		if (ltok[0] && ltok[1]) { /* more than 1 char? */
			bb_error_msg_and_die("the delimiter must be a single character");
		}
		delim = ltok[0];
	}

	/*  non-field (char or byte) cutting has some special handling */
	if (!(option_mask32 & CUT_OPT_FIELDS_FLGS)) {
		static const char _op_on_field[] ALIGN1 = " only when operating on fields";

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
			if (!ltok[0])
				continue;

			/* get the start pos */
			ntok = strsep(&ltok, "-");
			if (!ntok[0]) {
				s = BOL;
			} else {
				s = xatoi_u(ntok);
				/* account for the fact that arrays are zero based, while
				 * the user expects the first char on the line to be char #1 */
				if (s != 0)
					s--;
			}

			/* get the end pos */
			if (ltok == NULL) {
				e = NON_RANGE;
			} else if (!ltok[0]) {
				e = EOL;
			} else {
				e = xatoi_u(ltok);
				/* if the user specified and end position of 0, that means "til the
				 * end of the line */
				if (e == 0)
					e = EOL;
				e--;	/* again, arrays are zero based, lines are 1 based */
				if (e == s)
					e = NON_RANGE;
			}

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

	{
		int retval = EXIT_SUCCESS;
		FILE *file = stdin;

		if (!*argv) {
			argv--;
			goto jump_in;
		}

		do {
			file = stdin;
			if (NOT_LONE_DASH(*argv))
				file = fopen_or_warn(*argv, "r");
			if (!file) {
				retval = EXIT_FAILURE;
				continue;
			}
 jump_in:
			cut_file(file, delim);
			if (NOT_LONE_DASH(*argv))
				fclose(file);
		} while (*++argv);

		if (ENABLE_FEATURE_CLEAN_UP)
			free(cut_lists);
		fflush_stdout_and_exit(retval);
	}
}
