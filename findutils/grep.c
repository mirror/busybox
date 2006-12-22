/* vi: set sw=4 ts=4: */
/*
 * Mini grep implementation for busybox using libc regex.
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc. and Mark Whitley
 * Copyright (C) 1999,2000,2001 by Mark Whitley <markw@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
/* BB_AUDIT SUSv3 defects - unsupported option -x.  */
/* BB_AUDIT GNU defects - always acts as -a.  */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/grep.html */
/*
 * 2004,2006 (C) Vladimir Oleynik <dzo@simtreas.ru> -
 * correction "-e pattern1 -e pattern2" logic and more optimizations.
 * precompiled regex
 */
/*
 * (C) 2006 Jac Goudsmit added -o option
 */

#include "busybox.h"
#include "xregex.h"


/* options */
#define GREP_OPTS "lnqvscFiHhe:f:Lor"
#define GREP_OPT_l (1<<0)
#define PRINT_FILES_WITH_MATCHES (option_mask32 & GREP_OPT_l)
#define GREP_OPT_n (1<<1)
#define PRINT_LINE_NUM (option_mask32 & GREP_OPT_n)
#define GREP_OPT_q (1<<2)
#define BE_QUIET (option_mask32 & GREP_OPT_q)
#define GREP_OPT_v (1<<3)
#define GREP_OPT_s (1<<4)
#define SUPPRESS_ERR_MSGS (option_mask32 & GREP_OPT_s)
#define GREP_OPT_c (1<<5)
#define PRINT_MATCH_COUNTS (option_mask32 & GREP_OPT_c)
#define GREP_OPT_F (1<<6)
#define FGREP_FLAG (option_mask32 & GREP_OPT_F)
#define GREP_OPT_i (1<<7)
#define GREP_OPT_H (1<<8)
#define GREP_OPT_h (1<<9)
#define GREP_OPT_e (1<<10)
#define GREP_OPT_f (1<<11)
#define GREP_OPT_L (1<<12)
#define PRINT_FILES_WITHOUT_MATCHES (option_mask32 & GREP_OPT_L)
#define GREP_OPT_o (1<<13)
#define GREP_OPT_r (1<<14)
#if ENABLE_FEATURE_GREP_CONTEXT
# define GREP_OPT_CONTEXT "A:B:C:"
# define GREP_OPT_A (1<<15)
# define GREP_OPT_B (1<<16)
# define GREP_OPT_C (1<<17)
# define GREP_OPT_E (1<<18)
#else
# define GREP_OPT_CONTEXT ""
# define GREP_OPT_A 0
# define GREP_OPT_B 0
# define GREP_OPT_C 0
# define GREP_OPT_E (1<<15)
#endif
#if ENABLE_FEATURE_GREP_EGREP_ALIAS
# define OPT_EGREP "E"
#else
# define OPT_EGREP ""
#endif

typedef unsigned char byte_t;

static int reflags;
static byte_t invert_search;
static byte_t print_filename;
static byte_t open_errors;

#if ENABLE_FEATURE_GREP_CONTEXT
static int lines_before;
static int lines_after;
static char **before_buf;
static int last_line_printed;
#endif /* ENABLE_FEATURE_GREP_CONTEXT */

/* globals used internally */
static llist_t *pattern_head;   /* growable list of patterns to match */
static const char *cur_file;    /* the current file we are reading */

typedef struct GREP_LIST_DATA {
	char *pattern;
	regex_t preg;
#define PATTERN_MEM_A 1
#define COMPILED 2
	int flg_mem_alocated_compiled;
} grep_list_data_t;

static void print_line(const char *line, int linenum, char decoration)
{
#if ENABLE_FEATURE_GREP_CONTEXT
	/* possibly print the little '--' separator */
	if ((lines_before || lines_after) && last_line_printed &&
			last_line_printed < linenum - 1) {
		puts("--");
	}
	last_line_printed = linenum;
#endif
	if (print_filename)
		printf("%s%c", cur_file, decoration);
	if (PRINT_LINE_NUM)
		printf("%i%c", linenum, decoration);
	/* Emulate weird GNU grep behavior with -ov */
	if ((option_mask32 & (GREP_OPT_v+GREP_OPT_o)) != (GREP_OPT_v+GREP_OPT_o))
		puts(line);
}


static int grep_file(FILE *file)
{
	char *line;
	byte_t ret;
	int linenum = 0;
	int nmatches = 0;
	regmatch_t regmatch;
#if ENABLE_FEATURE_GREP_CONTEXT
	int print_n_lines_after = 0;
	int curpos = 0; /* track where we are in the circular 'before' buffer */
	int idx = 0; /* used for iteration through the circular buffer */
#endif /* ENABLE_FEATURE_GREP_CONTEXT */

	while ((line = xmalloc_getline(file)) != NULL) {
		llist_t *pattern_ptr = pattern_head;
		grep_list_data_t * gl;

		linenum++;
		ret = 0;
		while (pattern_ptr) {
			gl = (grep_list_data_t *)pattern_ptr->data;
			if (FGREP_FLAG) {
				ret = strstr(line, gl->pattern) != NULL;
			} else {
				/*
				 * test for a postitive-assertion match (regexec returns success (0)
				 * and the user did not specify invert search), or a negative-assertion
				 * match (regexec returns failure (REG_NOMATCH) and the user specified
				 * invert search)
				 */
				if (!(gl->flg_mem_alocated_compiled & COMPILED)) {
					gl->flg_mem_alocated_compiled |= COMPILED;
					xregcomp(&(gl->preg), gl->pattern, reflags);
				}
				regmatch.rm_so = 0;
				regmatch.rm_eo = 0;
				ret |= regexec(&(gl->preg), line, 1, &regmatch, 0) == 0;
			}
			pattern_ptr = pattern_ptr->link;
		} /* while (pattern_ptr) */

		if (ret ^ invert_search) {

			if (PRINT_FILES_WITH_MATCHES || BE_QUIET)
				free(line);

			/* if we found a match but were told to be quiet, stop here */
			if (BE_QUIET || PRINT_FILES_WITHOUT_MATCHES)
				return -1;

				/* keep track of matches */
				nmatches++;

				/* if we're just printing filenames, we stop after the first match */
				if (PRINT_FILES_WITH_MATCHES)
					break;

				/* print the matched line */
				if (PRINT_MATCH_COUNTS == 0) {
#if ENABLE_FEATURE_GREP_CONTEXT
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
#endif
					if (option_mask32 & GREP_OPT_o) {
						line[regmatch.rm_eo] = '\0';
						print_line(line + regmatch.rm_so, linenum, ':');
					} else {
						print_line(line, linenum, ':');
					}
				}
			}
#if ENABLE_FEATURE_GREP_CONTEXT
			else { /* no match */
				/* Add the line to the circular 'before' buffer */
				if (lines_before) {
					free(before_buf[curpos]);
					before_buf[curpos] = xstrdup(line);
					curpos = (curpos + 1) % lines_before;
				}
			}

			/* if we need to print some context lines after the last match, do so */
			if (print_n_lines_after && (last_line_printed != linenum)) {
				print_line(line, linenum, '-');
				print_n_lines_after--;
			}
#endif /* ENABLE_FEATURE_GREP_CONTEXT */
		free(line);
	}

	/* special-case file post-processing for options where we don't print line
	 * matches, just filenames and possibly match counts */

	/* grep -c: print [filename:]count, even if count is zero */
	if (PRINT_MATCH_COUNTS) {
		if (print_filename)
			printf("%s:", cur_file);
		printf("%d\n", nmatches);
	}

	/* grep -l: print just the filename, but only if we grepped the line in the file  */
	if (PRINT_FILES_WITH_MATCHES && nmatches > 0) {
		puts(cur_file);
	}

	/* grep -L: print just the filename, but only if we didn't grep the line in the file  */
	if (PRINT_FILES_WITHOUT_MATCHES && nmatches == 0) {
		puts(cur_file);
	}

	return nmatches;
}

#if ENABLE_FEATURE_CLEAN_UP
#define new_grep_list_data(p, m) add_grep_list_data(p, m)
static char * add_grep_list_data(char *pattern, int flg_used_mem)
#else
#define new_grep_list_data(p, m) add_grep_list_data(p)
static char * add_grep_list_data(char *pattern)
#endif
{
	grep_list_data_t *gl = xmalloc(sizeof(grep_list_data_t));
	gl->pattern = pattern;
#if ENABLE_FEATURE_CLEAN_UP
	gl->flg_mem_alocated_compiled = flg_used_mem;
#else
	gl->flg_mem_alocated_compiled = 0;
#endif
	return (char *)gl;
}


static void load_regexes_from_file(llist_t *fopt)
{
	char *line;
	FILE *f;

	while (fopt) {
		llist_t *cur = fopt;
		char *ffile = cur->data;

		fopt = cur->link;
		free(cur);
		f = xfopen(ffile, "r");
		while ((line = xmalloc_getline(f)) != NULL) {
			llist_add_to(&pattern_head,
				new_grep_list_data(line, PATTERN_MEM_A));
		}
	}
}


static int file_action_grep(const char *filename, struct stat *statbuf, void* matched, int depth)
{
	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		if (!SUPPRESS_ERR_MSGS)
			bb_perror_msg("%s", cur_file);
		open_errors = 1;
		return 0;
	}
	cur_file = filename;
	*(int*)matched += grep_file(file);
	fclose(file);
	return 1;
}


static int grep_dir(const char *dir)
{
	int matched = 0;
	recursive_action(dir,
		/* recurse= */ 1,
		/* followLinks= */ 0,
		/* depthFirst= */ 1,
		/* fileAction= */ file_action_grep,
		/* dirAction= */ NULL,
		/* userData= */ &matched,
		/* depth= */ 0);
	return matched;
}


int grep_main(int argc, char **argv)
{
	FILE *file;
	int matched;
	llist_t *fopt = NULL;

	/* do normal option parsing */
#if ENABLE_FEATURE_GREP_CONTEXT
	char *slines_after;
	char *slines_before;
	char *Copt;

	opt_complementary = "H-h:e::f::C-AB";
	getopt32(argc, argv,
		GREP_OPTS GREP_OPT_CONTEXT OPT_EGREP,
		&pattern_head, &fopt,
		&slines_after, &slines_before, &Copt);

	if (option_mask32 & GREP_OPT_C) {
		/* -C unsets prev -A and -B, but following -A or -B
		   may override it */
		if (!(option_mask32 & GREP_OPT_A)) /* not overridden */
			slines_after = Copt;
		if (!(option_mask32 & GREP_OPT_B)) /* not overridden */
			slines_before = Copt;
		option_mask32 |= GREP_OPT_A|GREP_OPT_B; /* for parser */
	}
	if (option_mask32 & GREP_OPT_A) {
		lines_after = xatoi_u(slines_after);
	}
	if (option_mask32 & GREP_OPT_B) {
		lines_before = xatoi_u(slines_before);
	}
	/* sanity checks */
	if (option_mask32 & (GREP_OPT_c|GREP_OPT_q|GREP_OPT_l|GREP_OPT_L)) {
		option_mask32 &= ~GREP_OPT_n;
		lines_before = 0;
		lines_after = 0;
	} else if (lines_before > 0)
		before_buf = xzalloc(lines_before * sizeof(char *));
#else
	/* with auto sanity checks */
	opt_complementary = "H-h:e::f::c-n:q-n:l-n";
	getopt32(argc, argv, GREP_OPTS OPT_EGREP,
		&pattern_head, &fopt);
#endif
	invert_search = ((option_mask32 & GREP_OPT_v) != 0); /* 0 | 1 */

	if (pattern_head != NULL) {
		/* convert char *argv[] to grep_list_data_t */
		llist_t *cur;

		for (cur = pattern_head; cur; cur = cur->link)
			cur->data = new_grep_list_data(cur->data, 0);
	}
	if (option_mask32 & GREP_OPT_f)
		load_regexes_from_file(fopt);

	if (ENABLE_FEATURE_GREP_FGREP_ALIAS && applet_name[0] == 'f')
		option_mask32 |= GREP_OPT_F;

	if (!(option_mask32 & GREP_OPT_o))
		reflags = REG_NOSUB;

	if (ENABLE_FEATURE_GREP_EGREP_ALIAS &&
			(applet_name[0] == 'e' || (option_mask32 & GREP_OPT_E)))
		reflags |= REG_EXTENDED;

	if (option_mask32 & GREP_OPT_i)
		reflags |= REG_ICASE;

	argv += optind;
	argc -= optind;

	/* if we didn't get a pattern from a -e and no command file was specified,
	 * argv[optind] should be the pattern. no pattern, no worky */
	if (pattern_head == NULL) {
		if (*argv == NULL)
			bb_show_usage();
		else {
			char *pattern = new_grep_list_data(*argv++, 0);

			llist_add_to(&pattern_head, pattern);
			argc--;
		}
	}

	/* argv[(optind)..(argc-1)] should be names of file to grep through. If
	 * there is more than one file to grep, we will print the filenames. */
	if (argc > 1)
		print_filename = 1;
	/* -H / -h of course override */
	if (option_mask32 & GREP_OPT_H)
		print_filename = 1;
	if (option_mask32 & GREP_OPT_h)
		print_filename = 0;

	/* If no files were specified, or '-' was specified, take input from
	 * stdin. Otherwise, we grep through all the files specified. */
	if (argc == 0)
		argc++;
	matched = 0;
	while (argc--) {
		cur_file = *argv++;
		file = stdin;
		if (!cur_file || (*cur_file == '-' && !cur_file[1])) {
			cur_file = "(standard input)";
		} else {
			if (option_mask32 & GREP_OPT_r) {
				struct stat st;
				if (stat(cur_file, &st) == 0 && S_ISDIR(st.st_mode)) {
					if (!(option_mask32 & GREP_OPT_h))
						print_filename = 1;
					matched += grep_dir(cur_file);
					goto grep_done;
				}
			}
			/* else: fopen(dir) will succeed, but reading won't */
			file = fopen(cur_file, "r");
			if (file == NULL) {
				if (!SUPPRESS_ERR_MSGS)
					bb_perror_msg("%s", cur_file);
				open_errors = 1;
				continue;
			}
		}
		matched += grep_file(file);
		fclose_if_not_stdin(file);
 grep_done:
		if (matched < 0) {
			/* we found a match but were told to be quiet, stop here and
			* return success */
			break;
		}
	}

	/* destroy all the elments in the pattern list */
	if (ENABLE_FEATURE_CLEAN_UP) {
		while (pattern_head) {
			llist_t *pattern_head_ptr = pattern_head;
			grep_list_data_t *gl =
				(grep_list_data_t *)pattern_head_ptr->data;

			pattern_head = pattern_head->link;
			if ((gl->flg_mem_alocated_compiled & PATTERN_MEM_A))
				free(gl->pattern);
			if ((gl->flg_mem_alocated_compiled & COMPILED))
				regfree(&(gl->preg));
			free(pattern_head_ptr);
		}
	}
	/* 0 = success, 1 = failed, 2 = error */
	/* If the -q option is specified, the exit status shall be zero
	 * if an input line is selected, even if an error was detected.  */
	if (BE_QUIET && matched)
		return 0;
	if (open_errors)
		return 2;
	return !matched; /* invert return value 0 = success, 1 = failed */
}
