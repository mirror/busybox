/*
 * Mini grep implementation for busybox using libc regex.
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc. and Mark Whitley
 * Copyright (C) 1999,2000,2001 by Mark Whitley <markw@codepoet.org> 
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
/*
 * Jun 2003 by Vladimir Oleynik <dzo@simtreas.ru> -
 * correction "-e pattern1 -e -e pattern2" logic and more optimizations.
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <regex.h>
#include <string.h>
#include <errno.h>
#include "busybox.h"


/* options */
#define GREP_OPTS "lnqvscFiHhe:f:"
#define GREP_OPT_l 1
static char print_files_with_matches;
#define GREP_OPT_n 2
static char print_line_num;
#define GREP_OPT_q 4
static char be_quiet;
#define GREP_OPT_v 8
typedef char invert_search_t;
static invert_search_t invert_search;
#define GREP_OPT_s 16
static char suppress_err_msgs;
#define GREP_OPT_c 32
static char print_match_counts;
#define GREP_OPT_F 64
static char fgrep_flag;
#define GREP_OPT_i 128
#define GREP_OPT_H 256
#define GREP_OPT_h 512
#define GREP_OPT_e 1024
#define GREP_OPT_f 2048
#ifdef CONFIG_FEATURE_GREP_CONTEXT
#define GREP_OPT_CONTEXT "A:B:C"
#define GREP_OPT_A 4096
#define GREP_OPT_B 8192
#define GREP_OPT_C 16384
#define GREP_OPT_E 32768U
#else
#define GREP_OPT_CONTEXT ""
#define GREP_OPT_E 4096
#endif
#ifdef CONFIG_FEATURE_GREP_EGREP_ALIAS
# define OPT_EGREP "E"
#else
# define OPT_EGREP ""
#endif

static int reflags;
static int print_filename;

#ifdef CONFIG_FEATURE_GREP_CONTEXT
static int lines_before;
static int lines_after;
static char **before_buf;
static int last_line_printed;
#endif /* CONFIG_FEATURE_GREP_CONTEXT */

/* globals used internally */
static llist_t *pattern_head;   /* growable list of patterns to match */
static char *cur_file;          /* the current file we are reading */


static void print_line(const char *line, int linenum, char decoration)
{
#ifdef CONFIG_FEATURE_GREP_CONTEXT
	/* possibly print the little '--' seperator */
	if ((lines_before || lines_after) && last_line_printed &&
			last_line_printed < linenum - 1) {
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

extern void xregcomp(regex_t *preg, const char *regex, int cflags);


static int grep_file(FILE *file)
{
	char *line;
	invert_search_t ret;
	int linenum = 0;
	int nmatches = 0;
#ifdef CONFIG_FEATURE_GREP_CONTEXT
	int print_n_lines_after = 0;
	int curpos = 0; /* track where we are in the circular 'before' buffer */
	int idx = 0; /* used for iteration through the circular buffer */
#endif /* CONFIG_FEATURE_GREP_CONTEXT */ 

	while ((line = bb_get_chomped_line_from_file(file)) != NULL) {
		llist_t *pattern_ptr = pattern_head;

		linenum++;
		ret = 0;
		while (pattern_ptr) {
			if (fgrep_flag) {
				ret = strstr(line, pattern_ptr->data) != NULL;
			} else {
				/*
				 * test for a postitive-assertion match (regexec returns success (0)
				 * and the user did not specify invert search), or a negative-assertion
				 * match (regexec returns failure (REG_NOMATCH) and the user specified
				 * invert search)
				 */
				regex_t regex;
				xregcomp(&regex, pattern_ptr->data, reflags);
				ret = regexec(&regex, line, 0, NULL, 0) == 0;
				regfree(&regex);
			}
			if (!ret)
				break;
			pattern_ptr = pattern_ptr->link;
		} /* while (pattern_ptr) */

		if ((ret ^ invert_search)) {

			if (print_files_with_matches || be_quiet)
				free(line);

			/* if we found a match but were told to be quiet, stop here */
				if (be_quiet)
				return -1;

				/* keep track of matches */
				nmatches++;

				/* if we're just printing filenames, we stop after the first match */
				if (print_files_with_matches)
					break;

				/* print the matched line */
				if (print_match_counts == 0) {
#ifdef CONFIG_FEATURE_GREP_CONTEXT
					int prevpos = (curpos == 0) ? lines_before - 1 : curpos - 1;

					/* if we were told to print 'before' lines and there is at least
					 * one line in the circular buffer, print them */
					if (lines_before && before_buf[prevpos] != NULL) {
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
#endif /* CONFIG_FEATURE_GREP_CONTEXT */ 
					print_line(line, linenum, ':');
				}
			}
#ifdef CONFIG_FEATURE_GREP_CONTEXT
			else { /* no match */
				/* Add the line to the circular 'before' buffer */
				if(lines_before) {
					free(before_buf[curpos]);
					before_buf[curpos] = bb_xstrdup(line);
					curpos = (curpos + 1) % lines_before;
				}
			}

			/* if we need to print some context lines after the last match, do so */
			if (print_n_lines_after && (last_line_printed != linenum)) {
				print_line(line, linenum, '-');
				print_n_lines_after--;
			}
#endif /* CONFIG_FEATURE_GREP_CONTEXT */ 
		free(line);
	}


	/* special-case file post-processing for options where we don't print line
	 * matches, just filenames and possibly match counts */

	/* grep -c: print [filename:]count, even if count is zero */
	if (print_match_counts) {
		if (print_filename)
			printf("%s:", cur_file);
		    printf("%d\n", nmatches);
	}

	/* grep -l: print just the filename, but only if we grepped the line in the file  */
	if (print_files_with_matches && nmatches > 0) {
		puts(cur_file);
	}

	return nmatches;
}

static void load_regexes_from_file(llist_t *fopt)
{
	char *line;
	FILE *f;

	while(fopt) {
		llist_t *cur = fopt;
		char *ffile = cur->data;

		fopt = cur->link;
		free(cur);
		f = bb_xfopen(ffile, "r");
	while ((line = bb_get_chomped_line_from_file(f)) != NULL) {
		pattern_head = llist_add_to(pattern_head, line);
	}
	}
}


extern int grep_main(int argc, char **argv)
{
	FILE *file;
	int matched;
	unsigned long opt;
	llist_t *fopt;

	/* do normal option parsing */
#ifdef CONFIG_FEATURE_GREP_CONTEXT
  {
	char *junk;
	char *slines_after;
	char *slines_before;
	char *Copt;

	bb_opt_complementaly = "H-h:e*:f*:C-AB";
	opt = bb_getopt_ulflags(argc, argv,
		GREP_OPTS GREP_OPT_CONTEXT OPT_EGREP,
		&pattern_head, &fopt,
		&slines_after, &slines_before, &Copt);

	if(opt & GREP_OPT_C) {
		/* C option unseted A and B options, but next -A or -B
		   may be ovewrite own option */
		if(!(opt & GREP_OPT_A))         /* not overwtited */
			slines_after = Copt;
		if(!(opt & GREP_OPT_B))         /* not overwtited */
			slines_before = Copt;
		opt |= GREP_OPT_A|GREP_OPT_B;   /* set for parse now */
	}
	if(opt & GREP_OPT_A) {
		lines_after = strtoul(slines_after, &junk, 10);
				if(*junk != '\0')
					bb_error_msg_and_die("invalid context length argument");
	}
	if(opt & GREP_OPT_B) {
		lines_before = strtoul(slines_before, &junk, 10);
				if(*junk != '\0')
					bb_error_msg_and_die("invalid context length argument");
		}
	/* sanity checks after parse may be invalid numbers ;-) */
	if ((opt & (GREP_OPT_c|GREP_OPT_q|GREP_OPT_l))) {
		opt &= ~GREP_OPT_n;
		lines_before = 0;
		lines_after = 0;
	} else if(lines_before > 0)
		before_buf = (char **)xcalloc(lines_before, sizeof(char *));
	}
#else
	/* with auto sanity checks */
	bb_opt_complementaly = "H-h:e*:f*:c-n:q-n:l-n";
	opt = bb_getopt_ulflags(argc, argv, GREP_OPTS OPT_EGREP,
		&pattern_head, &fopt);

#endif
	print_files_with_matches = opt & GREP_OPT_l;
	print_line_num = opt & GREP_OPT_n;
	be_quiet = opt & GREP_OPT_q;
	invert_search = (opt & GREP_OPT_v) != 0;        /* 0 | 1 */
	suppress_err_msgs = opt & GREP_OPT_s;
	print_match_counts = opt & GREP_OPT_c;
	fgrep_flag = opt & GREP_OPT_F;
	if(opt & GREP_OPT_H)
		print_filename++;
	if(opt & GREP_OPT_h)
		print_filename--;
	if(opt & GREP_OPT_f)
		load_regexes_from_file(fopt);

#ifdef CONFIG_FEATURE_GREP_EGREP_ALIAS
	if(bb_applet_name[0] == 'e' || (opt & GREP_OPT_E))
		reflags = REG_EXTENDED | REG_NOSUB;
	else
#endif
		reflags = REG_NOSUB;

	if(opt & GREP_OPT_i)
		reflags |= REG_ICASE;

	argv += optind;
	argc -= optind;

	/* if we didn't get a pattern from a -e and no command file was specified,
	 * argv[optind] should be the pattern. no pattern, no worky */
	if (pattern_head == NULL) {
		if (*argv == NULL)
			bb_show_usage();
		else {
			pattern_head = llist_add_to(pattern_head, *argv++);
			argc--;
		}
	}

	/* argv[(optind)..(argc-1)] should be names of file to grep through. If
	 * there is more than one file to grep, we will print the filenames */
	if (argc > 1) {
		print_filename++;

	/* If no files were specified, or '-' was specified, take input from
	 * stdin. Otherwise, we grep through all the files specified. */
	} else if (argc == 0) {
		argc++;
	}
	matched = 0;
	while (argc--) {
		cur_file = *argv++;
		if(!cur_file || (*cur_file == '-' && !cur_file[1])) {
			cur_file = "-";
			file = stdin;
		} else {
			file = fopen(cur_file, "r");
		}
			if (file == NULL) {
				if (!suppress_err_msgs)
					bb_perror_msg("%s", cur_file);
		} else {
			matched += grep_file(file);
			if(matched < 0) {
				/* we found a match but were told to be quiet, stop here and
				* return success */
				break;
			}
				fclose(file);
			}
		}

#ifdef CONFIG_FEATURE_CLEAN_UP
	/* destroy all the elments in the pattern list */
	while (pattern_head) {
		llist_t *pattern_head_ptr = pattern_head;

		pattern_head = pattern_head->link;
		free(pattern_head_ptr);
	}
#endif

	return !matched; /* invert return value 0 = success, 1 = failed */
}
