/*
 * Mini grep implementation for busybox using libc regex.
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Mark Whitley <markw@lineo.com>, <markw@codepoet.org>
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
#include <regex.h>
#include <string.h> /* for strerror() */
#include <errno.h>
#include "busybox.h"


extern int optind; /* in unistd.h */
extern int errno;  /* for use with strerror() */
extern void xregcomp(regex_t *preg, const char *regex, int cflags); /* in busybox.h */

/* options */
static int ignore_case       = 0;
static int print_filename    = 0;
static int print_line_num    = 0;
static int print_count_only  = 0;
static int be_quiet          = 0;
static int invert_search     = 0;
static int suppress_err_msgs = 0;

#ifdef BB_FEATURE_GREP_CONTEXT
extern char *optarg; /* in getopt.h */
static int lines_before      = 0;
static int lines_after       = 0;
static char **before_buf     = NULL;
static int last_line_printed = 0;
#endif /* BB_FEATURE_GREP_CONTEXT */

/* globals used internally */
static regex_t regex; /* storage space for compiled regular expression */
static int matched; /* keeps track of whether we ever matched */
static char *cur_file = NULL; /* the current file we are reading */

static void print_line(const char *line, int linenum, char decoration)
{
#ifdef BB_FEATURE_GREP_CONTEXT
	/* possibly print the little '--' seperator */
	if (last_line_printed && last_line_printed < linenum - 1) {
		puts("--");
	}
	last_line_printed = linenum;
#endif
	if (print_filename)
		printf("%s%c", cur_file, decoration);
	if (print_line_num)
		printf("%i%c", linenum, decoration);
	puts(line);
}

static void grep_file(FILE *file)
{
	char *line = NULL;
	int ret;
	int linenum = 0;
	int nmatches = 0;
#ifdef BB_FEATURE_GREP_CONTEXT
	int print_n_lines_after = 0;
	int curpos = 0; /* track where we are in the circular 'before' buffer */
	int idx = 0; /* used for iteration through the circular buffer */
#endif /* BB_FEATURE_GREP_CONTEXT */ 

	while ((line = get_line_from_file(file)) != NULL) {
		chomp(line);
		linenum++;

		/*
		 * test for a postitive-assertion match (regexec returns success (0)
		 * and the user did not specify invert search), or a negative-assertion
		 * match (regexec returns failure (REG_NOMATCH) and the user specified
		 * invert search)
		 */
		ret = regexec(&regex, line, 0, NULL, 0);
		if ((ret == 0 && !invert_search) || (ret == REG_NOMATCH && invert_search)) {

			/* if we found a match but were told to be quiet, stop here and
			 * return success */
			if (be_quiet) {
				regfree(&regex);
				exit(0);
			}

			/* otherwise, keep track of matches and print the matched line */
			nmatches++;
			if (!print_count_only) {
#ifdef BB_FEATURE_GREP_CONTEXT
				int prevpos = (curpos == 0) ? lines_before - 1 : curpos - 1;

				/* if we were told to print 'before' lines and there is at least
				 * one line in the circular buffer, print them */
				if (lines_before && before_buf[prevpos] != NULL)
				{
					int first_buf_entry_line_num = linenum - lines_before;

					/* advance to the first entry in the circular buffer, and
					 * figure out the line number is of the first line in the
					 * buffer */
					idx = curpos;
					while (before_buf[idx] == NULL) {
						idx = (idx + 1) % lines_before;
						first_buf_entry_line_num++;
					}

					/* now print each line in the buffer, clearing them as we go */
					while (before_buf[idx] != NULL) {
						print_line(before_buf[idx], first_buf_entry_line_num, '-');
						free(before_buf[idx]);
						before_buf[idx] = NULL;
						idx = (idx + 1) % lines_before;
						first_buf_entry_line_num++;
					}

				}

				/* make a note that we need to print 'after' lines */
				print_n_lines_after = lines_after;
#endif /* BB_FEATURE_GREP_CONTEXT */ 
				print_line(line, linenum, ':');
			}
		}
#ifdef BB_FEATURE_GREP_CONTEXT
		else { /* no match */
			/* Add the line to the circular 'before' buffer */
			if(lines_before)
			{
				if(before_buf[curpos])
					free(before_buf[curpos]);
				before_buf[curpos] = strdup(line);
				curpos = (curpos + 1) % lines_before;
			}
		}

		/* if we need to print some context lines after the last match, do so */
		if (print_n_lines_after && (last_line_printed != linenum)) {
			print_line(line, linenum, '-');
			print_n_lines_after--;
		}
#endif /* BB_FEATURE_GREP_CONTEXT */ 
		free(line);
	}

	/* special-case post processing */
	if (print_count_only) {
		if (print_filename)
			printf("%s:", cur_file);
		printf("%i\n", nmatches);
	}

	/* remember if we matched */
	if (nmatches != 0)
		matched = 1;
}

extern int grep_main(int argc, char **argv)
{
	int opt;
	int reflags;
#ifdef BB_FEATURE_GREP_CONTEXT
	char *junk;
#endif

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "iHhnqvsc"
#ifdef BB_FEATURE_GREP_CONTEXT
"A:B:C:"
#endif
)) > 0) {
		switch (opt) {
			case 'i':
				ignore_case++;
				break;
			case 'H':
				print_filename++;
				break;
			case 'h':
				print_filename--;
				break;
			case 'n':
				print_line_num++;
				break;
			case 'q':
				be_quiet++;
				break;
			case 'v':
				invert_search++;
				break;
			case 's':
				suppress_err_msgs++;
				break;
			case 'c':
				print_count_only++;
				break;
#ifdef BB_FEATURE_GREP_CONTEXT
			case 'A':
				lines_after = strtoul(optarg, &junk, 10);
				if(*junk != '\0')
					error_msg_and_die("invalid context length argument");
				break;
			case 'B':
				lines_before = strtoul(optarg, &junk, 10);
				if(*junk != '\0')
					error_msg_and_die("invalid context length argument");
				before_buf = (char **)calloc(lines_before, sizeof(char *));
				break;
			case 'C':
				lines_after = lines_before = strtoul(optarg, &junk, 10);
				if(*junk != '\0')
					error_msg_and_die("invalid context length argument");
				before_buf = (char **)calloc(lines_before, sizeof(char *));
				break;
#endif /* BB_FEATURE_GREP_CONTEXT */
			default:
				show_usage();
		}
	}

	/* argv[optind] should be the regex pattern; no pattern, no worky */
	if (argv[optind] == NULL)
		show_usage();

	/* sanity check */
	if (print_count_only || be_quiet) {
		print_line_num = 0;
#ifdef BB_FEATURE_GREP_CONTEXT
		lines_before = 0;
		lines_after = 0;
#endif
	}

	/* compile the regular expression
	 * we're not going to mess with sub-expressions, and we need to
	 * treat newlines right. */
	reflags = REG_NOSUB; 
	if (ignore_case)
		reflags |= REG_ICASE;
	xregcomp(&regex, argv[optind], reflags);

	/* argv[(optind+1)..(argc-1)] should be names of file to grep through. If
	 * there is more than one file to grep, we will print the filenames */
	if ((argc-1) - (optind+1) > 0)
		print_filename++;

	/* If no files were specified, or '-' was specified, take input from
	 * stdin. Otherwise, we grep through all the files specified. */
	if (argv[optind+1] == NULL || (strcmp(argv[optind+1], "-") == 0)) {
		grep_file(stdin);
	}
	else {
		int i;
		FILE *file;
		for (i = optind + 1; i < argc; i++) {
			cur_file = argv[i];
			file = fopen(cur_file, "r");
			if (file == NULL) {
				if (!suppress_err_msgs)
					perror_msg("%s", cur_file);
			}
			else {
				grep_file(file);
				fclose(file);
			}
		}
	}

	regfree(&regex);

	return !matched; /* invert return value 0 = success, 1 = failed */
}
