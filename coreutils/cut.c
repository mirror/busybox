/* vi: set sw=4 ts=4: */
/*
 * cut.c - minimalist version of cut
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Mark Whitley <markw@codepoet.org>
 * debloated by Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config CUT
//config:	bool "cut (6.7 kb)"
//config:	default y
//config:	help
//config:	cut is used to print selected parts of lines from
//config:	each file to stdout.
//config:
//config:config FEATURE_CUT_REGEX
//config:	bool "cut -F"
//config:	default y
//config:	depends on CUT
//config:	help
//config:	Allow regex based delimiters.

//applet:IF_CUT(APPLET_NOEXEC(cut, cut, BB_DIR_USR_BIN, BB_SUID_DROP, cut))

//kbuild:lib-$(CONFIG_CUT) += cut.o

//usage:#define cut_trivial_usage
//usage:       "{-b|c LIST | -f"IF_FEATURE_CUT_REGEX("|F")" LIST [-d SEP] [-s]} [-D] [-O SEP] [FILE]..."
//  --output-delimiter SEP is too long to fit into 80 char-wide help ----------------^^^^^^^^
//usage:#define cut_full_usage "\n\n"
//usage:       "Print selected fields from FILEs to stdout\n"
//usage:     "\n	-b LIST	Output only bytes from LIST"
//usage:     "\n	-c LIST	Output only characters from LIST"
//usage:     IF_FEATURE_CUT_REGEX(
//usage:     "\n	-d SEP	Input field delimiter (default -f TAB, -F run of whitespace)"
//usage:     ) IF_NOT_FEATURE_CUT_REGEX(
//usage:     "\n	-d SEP	Input field delimiter (default TAB)"
//usage:     )
//usage:     "\n	-f LIST	Print only these fields (-d is single char)"
//usage:     IF_FEATURE_CUT_REGEX(
//usage:     "\n	-F LIST	Print only these fields (-d is regex)"
//usage:     )
//usage:     "\n	-s	Drop lines with no delimiter (else print them in full)"
//usage:     "\n	-D	Don't sort ranges; line without delimiters has one field"
//usage:     IF_LONG_OPTS(
//usage:     "\n	--output-delimiter SEP	Output field delimeter"
//usage:     ) IF_NOT_LONG_OPTS(
//usage:     IF_FEATURE_CUT_REGEX(
//usage:     "\n	-O SEP	Output field delimeter (default = -d for -f, one space for -F)"
//usage:     ) IF_NOT_FEATURE_CUT_REGEX(
//usage:     "\n	-O SEP	Output field delimeter (default = -d)"
//usage:     )
//usage:     )
//usage:     "\n	-n	Ignored"
//(manpage:-n with -b: don't split multibyte characters)
//usage:
//usage:#define cut_example_usage
//usage:       "$ echo \"Hello world\" | cut -f 1 -d ' '\n"
//usage:       "Hello\n"
//usage:       "$ echo \"Hello world\" | cut -f 2 -d ' '\n"
//usage:       "world\n"

#include "libbb.h"

#if ENABLE_FEATURE_CUT_REGEX
#include "xregex.h"
#endif

/* This is a NOEXEC applet. Be very careful! */


/* option vars */
#define OPT_STR "b:c:f:d:O:sD"IF_FEATURE_CUT_REGEX("F:")"n"
#define OPT_BYTE     (1 << 0)
#define OPT_CHAR     (1 << 1)
#define OPT_FIELDS   (1 << 2)
#define OPT_DELIM    (1 << 3)
#define OPT_ODELIM   (1 << 4)
#define OPT_SUPPRESS (1 << 5)
#define OPT_NOSORT   (1 << 6)
#define OPT_REGEX    ((1 << 7) * ENABLE_FEATURE_CUT_REGEX)

#define opt_REGEX (option_mask32 & OPT_REGEX)

struct cut_range {
	unsigned startpos;
	unsigned endpos;
};

static int cmpfunc(const void *a, const void *b)
{
	const struct cut_range *aa = a;
	const struct cut_range *bb = b;
	return aa->startpos - bb->startpos;
}

#define END_OF_LIST(list_elem)     ((list_elem).startpos == UINT_MAX)
#define NOT_END_OF_LIST(list_elem) ((list_elem).startpos != UINT_MAX)

static void cut_file(FILE *file, const char *delim, const char *odelim,
		const struct cut_range *cut_list)
{
	char *line;
	unsigned linenum = 0;	/* keep these zero-based to be consistent */
	int first_print = 1;

	/* go through every line in the file */
	while ((line = xmalloc_fgetline(file)) != NULL) {

		/* set up a list so we can keep track of what's been printed */
		unsigned linelen = strlen(line);
		unsigned cl_pos = 0;

		/* Cut based on chars/bytes XXX: only works when sizeof(char) == byte */
		if (option_mask32 & (OPT_CHAR | OPT_BYTE)) {
			char *printed = xzalloc(linelen + 1);
			int need_odelim = 0;

			/* print the chars specified in each cut list */
			for (; NOT_END_OF_LIST(cut_list[cl_pos]); cl_pos++) {
				unsigned spos = cut_list[cl_pos].startpos;
				while (spos < linelen) {
					if (!printed[spos]) {
						printed[spos] = 'X';
						if (need_odelim && spos != 0 && !printed[spos-1]) {
							need_odelim = 0;
							fputs_stdout(odelim);
						}
						putchar(line[spos]);
					}
					spos++;
					if (spos > cut_list[cl_pos].endpos) {
						/* will print OSEP (if not empty) */
						need_odelim = (odelim && odelim[0]);
						break;
					}
				}
			}
			free(printed);
		/* Cut by lines */
		} else if (!opt_REGEX && *delim == '\n') {
			unsigned spos = cut_list[cl_pos].startpos;

			linenum++;
			/* get out if we have no more ranges to process or if the lines
			 * are lower than what we're interested in */
			if (linenum <= spos || END_OF_LIST(cut_list[cl_pos]))
				goto next_line;

			/* if the line we're looking for is lower than the one we were
			 * passed, it means we displayed it already, so move on */
			while (++spos < linenum) {
				/* go to the next list if we're at the end of this one */
				if (spos > cut_list[cl_pos].endpos) {
					cl_pos++;
					/* get out if there's no more ranges to process */
					if (END_OF_LIST(cut_list[cl_pos]))
						goto next_line;
					spos = cut_list[cl_pos].startpos;
					/* get out if the current line is lower than the one
					 * we just became interested in */
					if (linenum <= spos)
						goto next_line;
				}
			}

			/* If we made it here, it means we've found the line we're
			 * looking for, so print it */
			if (first_print) {
				first_print = 0;
				fputs_stdout(line);
			} else
				printf("%s%s", odelim, line);
			goto next_line;
		/* Cut by fields */
		} else {
			unsigned next = 0, start = 0, end = 0;
			unsigned dcount = 0; /* we saw Nth delimiter (0 - didn't see any yet) */

			/* Blank line? Check -s (later check for -s does not catch empty lines) */
			if (linelen == 0) {
				if (option_mask32 & OPT_SUPPRESS)
					goto next_line;
			}

			if (!odelim)
				odelim = "\t";
			first_print = 1;

			/* Loop through bytes, finding next delimiter */
			for (;;) {
				/* End of current range? */
				if (end == linelen || dcount > cut_list[cl_pos].endpos) {
 end_of_range:
					cl_pos++;
					if (END_OF_LIST(cut_list[cl_pos]))
						break;
					if (option_mask32 & OPT_NOSORT)
						start = dcount = next = 0;
					end = 0; /* (why?) */
					//bb_error_msg("End of current range");
				}
				/* End of current line? */
				if (next == linelen) {
					end = linelen; /* print up to end */
					/* If we've seen no delimiters, and no -D, check -s */
					if (!(option_mask32 & OPT_NOSORT) && cl_pos == 0 && dcount == 0) {
						if (option_mask32 & OPT_SUPPRESS)
							goto next_line;
						/* else: will print entire line */
					} else if (dcount < cut_list[cl_pos].startpos) {
						/* echo 1.2 | cut -d. -f1,3: prints "1", not "1." */
						//break;
						/* ^^^ this fails a case with -D:
						 * echo 1 2 | cut -DF 1,3,2:
						 * do not end line processing when didn't find field#3
						 */
						//if (option_mask32 & OPT_NOSORT) - no, just do it always
						goto end_of_range;
					}
					//bb_error_msg("End of current line: s:%d e:%d", start, end);
				} else {
					/* Find next delimiter */
#if ENABLE_FEATURE_CUT_REGEX
					if (opt_REGEX) {
						regmatch_t rr;
						regex_t *reg = (void*) delim;

						if (regexec(reg, line + next, 1, &rr, REG_NOTBOL|REG_NOTEOL) != 0) {
							/* not found, go to "end of line" logic */
							next = linelen;
							continue;
						}
						end = next + rr.rm_so;
						next += (rr.rm_eo ? rr.rm_eo : 1);
						/* ^^^ advancing by at least 1 prevents infinite loops */
						/* testcase: echo "no at sign" | cut -d'@*' -F 1- */
					} else
#endif
					{
						end = next++;
						if (line[end] != *delim)
							continue;
					}
					/* Got delimiter */
					dcount++;
					if (dcount <= cut_list[cl_pos].startpos) {
						/* Not yet within range - loop */
						start = next;
						continue;
					}
					/* -F N-M preserves intermediate delimiters: */
					//printf "1 2  3  4  5  6  7\n" | toybox cut -O: -F2,4-6,7
					//2:4  5  6:7
					if (opt_REGEX && dcount <= cut_list[cl_pos].endpos)
						continue;
// NB: toybox does the above for -f too, but it's a compatibility bug:
//printf "1 2 3 4 5 6 7 8\n" | toybox cut -d' ' -O: -f2,4-6,7
//2:4 5 6:7  // WRONG!
//printf "1 2 3 4 5 6 7 8\n" | cut -d' ' --output-delimiter=: -f2,4-6,7
//2:4:5:6:7  // GNU coreutils 9.1
				}
#if ENABLE_FEATURE_CUT_REGEX
				if (end != start || !opt_REGEX)
#endif
				{
					if (first_print) {
						first_print = 0;
						printf("%.*s", end - start, line + start);
					} else
						printf("%s%.*s", odelim, end - start, line + start);
				}
				start = next;
				//if (dcount == 0)
				//	break; - why?
			} /* byte loop */
		}
		/* if we printed anything, finish with newline */
		putchar('\n');
 next_line:
		free(line);
	} /* while (got line) */

	/* For -d$'\n' --output-delimiter=^, the overall output is still terminated with \n, not ^ */
	if (!opt_REGEX && *delim == '\n' && !first_print)
		putchar('\n');
}

int cut_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cut_main(int argc UNUSED_PARAM, char **argv)
{
	/* growable array holding a series of ranges */
	struct cut_range *cut_list = NULL;
	unsigned nranges = 0;	/* number of elements in above list */
	char *LIST, *ltok;
	const char *delim = NULL;
	const char *odelim = NULL;
	unsigned opt;
#if ENABLE_FEATURE_CUT_REGEX
	regex_t reg;
#endif
#if ENABLE_LONG_OPTS
	static const char cut_longopts[] ALIGN1 =
		"output-delimiter\0" Required_argument "O"
		;
#endif

#define ARG "bcf"IF_FEATURE_CUT_REGEX("F")
#if ENABLE_LONG_OPTS
	opt = getopt32long
#else
	opt = getopt32
#endif
		(argv, "^"
			OPT_STR  // = "b:c:f:d:O:sD"IF_FEATURE_CUT_REGEX("F:")"n"
			"\0" "b:c:f:" IF_FEATURE_CUT_REGEX("F:") /* one of -bcfF is required */
			"b--"ARG":c--"ARG":f--"ARG IF_FEATURE_CUT_REGEX(":F--"ARG), /* they are mutually exclusive */
		IF_LONG_OPTS(cut_longopts,)
			&LIST, &LIST, &LIST, &delim, &odelim IF_FEATURE_CUT_REGEX(, &LIST)
		);
	if (!odelim)
		odelim = (opt & OPT_REGEX) ? " " : delim;
	if (!delim)
		delim = (opt & OPT_REGEX) ? "[[:space:]]+" : "\t";

//	argc -= optind;
	argv += optind;
	//if (!(opt & (OPT_BYTE | OPT_CHAR | OPT_FIELDS | OPT_REGEX)))
	//	bb_simple_error_msg_and_die("expected a list of bytes, characters, or fields");
	//^^^ handled by getopt32

	/* non-field (char or byte) cutting has some special handling */
	if (!(opt & (OPT_FIELDS|OPT_REGEX))) {
		static const char requires_f[] ALIGN1 = " requires -f"
					IF_FEATURE_CUT_REGEX(" or -F");
		if (opt & OPT_SUPPRESS)
			bb_error_msg_and_die("-s%s", requires_f);
		if (opt & OPT_DELIM)
			bb_error_msg_and_die("-d DELIM%s", requires_f);
	}

	/*
	 * parse list and put values into startpos and endpos.
	 * valid range formats: N, N-, N-M, -M
	 * more than one range can be separated by commas
	 */
	/* take apart the ranges, one by one (separated with commas) */
	while ((ltok = strsep(&LIST, ",")) != NULL) {
		char *ntok;
		int s, e;

		/* it's actually legal to pass an empty list */
		//if (!ltok[0])
		//	continue;
		//^^^ testcase?

		/* get the start pos */
		ntok = strsep(&ltok, "-");
		if (!ntok[0]) {
			if (!ltok) /* testcase: -f '' */
				bb_show_usage();
			if (!ltok[0]) /* testcase: -f - */
				bb_show_usage();
			s = 0; /* "-M" means "1-M" */
		} else {
			/* "N" or "N-[M]" */
			/* arrays are zero based, while the user expects
			 * the first field/char on the line to be char #1 */
			s = xatoi_positive(ntok) - 1;
		}

		/* get the end pos */
		if (!ltok) {
			e = s; /* "N" means "N-N" */
		} else if (!ltok[0]) {
			/* "N-" means "until the end of the line" */
			e = INT_MAX;
		} else {
			/* again, arrays are zero based, fields are 1 based */
			e = xatoi_positive(ltok) - 1;
		}

		if (s < 0 || e < s)
			bb_error_msg_and_die("invalid range %s-%s", ntok, ltok ?: ntok);

		/* add the new range */
		cut_list = xrealloc_vector(cut_list, 4, nranges);
		/* NB: s is always >= 0 */
		cut_list[nranges].startpos = s;
		cut_list[nranges].endpos = e;
		nranges++;
	}
	cut_list[nranges].startpos = UINT_MAX; /* end indicator */

	/* make sure we got some cut positions out of all that */
	//if (nranges == 0)
	//	bb_simple_error_msg_and_die("missing list of positions");
	//^^^ this is impossible since one of -bcfF is required,
	// they populate LIST with non-NULL string and when it is parsed,
	// cut_list[] gets at least one element.

	/* now that the lists are parsed, we need to sort them to make life
	 * easier on us when it comes time to print the chars / fields / lines
	 */
	if (!(opt & OPT_NOSORT))
		qsort(cut_list, nranges, sizeof(cut_list[0]), cmpfunc);

#if ENABLE_FEATURE_CUT_REGEX
	if (opt & OPT_REGEX) {
		xregcomp(&reg, delim, REG_EXTENDED);
		delim = (void*) &reg;
	}
#endif

	{
		exitcode_t retval = EXIT_SUCCESS;

		if (!*argv)
			*--argv = (char *)"-";

		do {
			FILE *file = fopen_or_warn_stdin(*argv);
			if (!file) {
				retval = EXIT_FAILURE;
				continue;
			}
			cut_file(file, delim, odelim, cut_list);
			fclose_if_not_stdin(file);
		} while (*++argv);

		if (ENABLE_FEATURE_CLEAN_UP)
			free(cut_list);
		fflush_stdout_and_exit(retval);
	}
}
