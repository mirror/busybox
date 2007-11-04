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

#include "libbb.h"
#include "xregex.h"

/* options */
#define OPTSTR_GREP \
	"lnqvscFiHhe:f:Lorm:" \
	USE_FEATURE_GREP_CONTEXT("A:B:C:") \
	USE_FEATURE_GREP_EGREP_ALIAS("E") \
	USE_DESKTOP("w") \
	"aI"
/* ignored: -a "assume all files to be text" */
/* ignored: -I "assume binary files have no matches" */

enum {
	OPTBIT_l, /* list matched file names only */
	OPTBIT_n, /* print line# */
	OPTBIT_q, /* quiet - exit(0) of first match */
	OPTBIT_v, /* invert the match, to select non-matching lines */
	OPTBIT_s, /* suppress errors about file open errors */
	OPTBIT_c, /* count matches per file (suppresses normal output) */
	OPTBIT_F, /* literal match */
	OPTBIT_i, /* case-insensitive */
	OPTBIT_H, /* force filename display */
	OPTBIT_h, /* inhibit filename display */
	OPTBIT_e, /* -e PATTERN */
	OPTBIT_f, /* -f FILE_WITH_PATTERNS */
	OPTBIT_L, /* list unmatched file names only */
	OPTBIT_o, /* show only matching parts of lines */
	OPTBIT_r, /* recurse dirs */
	OPTBIT_m, /* -m MAX_MATCHES */
	USE_FEATURE_GREP_CONTEXT(    OPTBIT_A ,) /* -A NUM: after-match context */
	USE_FEATURE_GREP_CONTEXT(    OPTBIT_B ,) /* -B NUM: before-match context */
	USE_FEATURE_GREP_CONTEXT(    OPTBIT_C ,) /* -C NUM: -A and -B combined */
	USE_FEATURE_GREP_EGREP_ALIAS(OPTBIT_E ,) /* extended regexp */
	USE_DESKTOP(                 OPTBIT_w ,) /* whole word match */
	OPT_l = 1 << OPTBIT_l,
	OPT_n = 1 << OPTBIT_n,
	OPT_q = 1 << OPTBIT_q,
	OPT_v = 1 << OPTBIT_v,
	OPT_s = 1 << OPTBIT_s,
	OPT_c = 1 << OPTBIT_c,
	OPT_F = 1 << OPTBIT_F,
	OPT_i = 1 << OPTBIT_i,
	OPT_H = 1 << OPTBIT_H,
	OPT_h = 1 << OPTBIT_h,
	OPT_e = 1 << OPTBIT_e,
	OPT_f = 1 << OPTBIT_f,
	OPT_L = 1 << OPTBIT_L,
	OPT_o = 1 << OPTBIT_o,
	OPT_r = 1 << OPTBIT_r,
	OPT_m = 1 << OPTBIT_m,
	OPT_A = USE_FEATURE_GREP_CONTEXT(    (1 << OPTBIT_A)) + 0,
	OPT_B = USE_FEATURE_GREP_CONTEXT(    (1 << OPTBIT_B)) + 0,
	OPT_C = USE_FEATURE_GREP_CONTEXT(    (1 << OPTBIT_C)) + 0,
	OPT_E = USE_FEATURE_GREP_EGREP_ALIAS((1 << OPTBIT_E)) + 0,
	OPT_w = USE_DESKTOP(                 (1 << OPTBIT_w)) + 0,
};

#define PRINT_FILES_WITH_MATCHES    (option_mask32 & OPT_l)
#define PRINT_LINE_NUM              (option_mask32 & OPT_n)
#define BE_QUIET                    (option_mask32 & OPT_q)
#define SUPPRESS_ERR_MSGS           (option_mask32 & OPT_s)
#define PRINT_MATCH_COUNTS          (option_mask32 & OPT_c)
#define FGREP_FLAG                  (option_mask32 & OPT_F)
#define PRINT_FILES_WITHOUT_MATCHES (option_mask32 & OPT_L)

struct globals {
	int max_matches;
	int reflags;
	smalluint invert_search;
	smalluint print_filename;
	smalluint open_errors;
#if ENABLE_FEATURE_GREP_CONTEXT
	smalluint did_print_line;
	int lines_before;
	int lines_after;
	char **before_buf;
	int last_line_printed;
#endif
	/* globals used internally */
	llist_t *pattern_head;   /* growable list of patterns to match */
	const char *cur_file;    /* the current file we are reading */
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() \
	do { \
		struct G_sizecheck { \
			char G_sizecheck[sizeof(G) > COMMON_BUFSIZE ? -1 : 1]; \
		}; \
	} while (0)
#define max_matches       (G.max_matches         )
#define reflags           (G.reflags             )
#define invert_search     (G.invert_search       )
#define print_filename    (G.print_filename      )
#define open_errors       (G.open_errors         )
#define did_print_line    (G.did_print_line      )
#define lines_before      (G.lines_before        )
#define lines_after       (G.lines_after         )
#define before_buf        (G.before_buf          )
#define last_line_printed (G.last_line_printed   )
#define pattern_head      (G.pattern_head        )
#define cur_file          (G.cur_file            )


typedef struct grep_list_data_t {
	char *pattern;
	regex_t preg;
#define PATTERN_MEM_A 1
#define COMPILED 2
	int flg_mem_alocated_compiled;
} grep_list_data_t;


static void print_line(const char *line, int linenum, char decoration)
{
#if ENABLE_FEATURE_GREP_CONTEXT
	/* Happens when we go to next file, immediately hit match
	 * and try to print prev context... from prev file! Don't do it */
	if (linenum < 1)
		return;
	/* possibly print the little '--' separator */
	if ((lines_before || lines_after) && did_print_line &&
			last_line_printed != linenum - 1) {
		puts("--");
	}
	/* guard against printing "--" before first line of first file */
	did_print_line = 1;
	last_line_printed = linenum;
#endif
	if (print_filename)
		printf("%s%c", cur_file, decoration);
	if (PRINT_LINE_NUM)
		printf("%i%c", linenum, decoration);
	/* Emulate weird GNU grep behavior with -ov */
	if ((option_mask32 & (OPT_v|OPT_o)) != (OPT_v|OPT_o))
		puts(line);
}

static int grep_file(FILE *file)
{
	char *line;
	smalluint found;
	int linenum = 0;
	int nmatches = 0;
	regmatch_t regmatch;
#if ENABLE_FEATURE_GREP_CONTEXT
	int print_n_lines_after = 0;
	int curpos = 0; /* track where we are in the circular 'before' buffer */
	int idx = 0; /* used for iteration through the circular buffer */
#else
	enum { print_n_lines_after = 0 };
#endif /* ENABLE_FEATURE_GREP_CONTEXT */

	while ((line = xmalloc_getline(file)) != NULL) {
		llist_t *pattern_ptr = pattern_head;
		grep_list_data_t *gl = gl; /* for gcc */

		linenum++;
		found = 0;
		while (pattern_ptr) {
			gl = (grep_list_data_t *)pattern_ptr->data;
			if (FGREP_FLAG) {
				found |= (strstr(line, gl->pattern) != NULL);
			} else {
				if (!(gl->flg_mem_alocated_compiled & COMPILED)) {
					gl->flg_mem_alocated_compiled |= COMPILED;
					xregcomp(&(gl->preg), gl->pattern, reflags);
				}
				regmatch.rm_so = 0;
				regmatch.rm_eo = 0;
				if (regexec(&(gl->preg), line, 1, &regmatch, 0) == 0) {
					if (!(option_mask32 & OPT_w))
						found = 1;
					else {
						char c = ' ';
						if (regmatch.rm_so)
							c = line[regmatch.rm_so - 1];
						if (!isalnum(c) && c != '_') {
							c = line[regmatch.rm_eo];
							if (!c || (!isalnum(c) && c != '_'))
								found = 1;
						}
					}
				}
			}
			/* If it's non-inverted search, we can stop
			 * at first match */
			if (found && !invert_search)
				goto do_found;
			pattern_ptr = pattern_ptr->link;
		} /* while (pattern_ptr) */

		if (found ^ invert_search) {
 do_found:
			/* keep track of matches */
			nmatches++;

			/* quiet/print (non)matching file names only? */
			if (option_mask32 & (OPT_q|OPT_l|OPT_L)) {
				free(line); /* we don't need line anymore */
				if (BE_QUIET) {
					/* manpage says about -q:
					 * "exit immediately with zero status
					 * if any match is found,
					 * even if errors were detected" */
					exit(0);
				}
				/* if we're just printing filenames, we stop after the first match */
				if (PRINT_FILES_WITH_MATCHES) {
					puts(cur_file);
					/* fall thru to "return 1" */
				}
				/* OPT_L aka PRINT_FILES_WITHOUT_MATCHES: return early */
				return 1; /* one match */
			}

#if ENABLE_FEATURE_GREP_CONTEXT
			/* Were we printing context and saw next (unwanted) match? */
			if ((option_mask32 & OPT_m) && nmatches > max_matches)
				break;
#endif

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
				if (option_mask32 & OPT_o) {
					if (FGREP_FLAG) {
						/* -Fo just prints the pattern
						 * (unless -v: -Fov doesnt print anything at all) */
						if (found)
							print_line(gl->pattern, linenum, ':');
					} else {
						line[regmatch.rm_eo] = '\0';
						print_line(line + regmatch.rm_so, linenum, ':');
					}
				} else {
					print_line(line, linenum, ':');
				}
			}
		}
#if ENABLE_FEATURE_GREP_CONTEXT
		else { /* no match */
			/* if we need to print some context lines after the last match, do so */
			if (print_n_lines_after) {
				print_line(line, linenum, '-');
				print_n_lines_after--;
			} else if (lines_before) {
				/* Add the line to the circular 'before' buffer */
				free(before_buf[curpos]);
				before_buf[curpos] = line;
				curpos = (curpos + 1) % lines_before;
				/* avoid free(line) - we took line */
				line = NULL;
			}
		}

#endif /* ENABLE_FEATURE_GREP_CONTEXT */
		free(line);

		/* Did we print all context after last requested match? */
		if ((option_mask32 & OPT_m)
		 && !print_n_lines_after && nmatches == max_matches)
			break;
	}

	/* special-case file post-processing for options where we don't print line
	 * matches, just filenames and possibly match counts */

	/* grep -c: print [filename:]count, even if count is zero */
	if (PRINT_MATCH_COUNTS) {
		if (print_filename)
			printf("%s:", cur_file);
		printf("%d\n", nmatches);
	}

	/* grep -L: print just the filename */
	if (PRINT_FILES_WITHOUT_MATCHES) {
		/* nmatches is zero, no need to check it:
		 * we return 1 early if we detected a match
		 * and PRINT_FILES_WITHOUT_MATCHES is set */
		puts(cur_file);
	}

	return nmatches;
}

#if ENABLE_FEATURE_CLEAN_UP
#define new_grep_list_data(p, m) add_grep_list_data(p, m)
static char *add_grep_list_data(char *pattern, int flg_used_mem)
#else
#define new_grep_list_data(p, m) add_grep_list_data(p)
static char *add_grep_list_data(char *pattern)
#endif
{
	grep_list_data_t *gl = xzalloc(sizeof(*gl));
	gl->pattern = pattern;
#if ENABLE_FEATURE_CLEAN_UP
	gl->flg_mem_alocated_compiled = flg_used_mem;
#else
	/*gl->flg_mem_alocated_compiled = 0;*/
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
			bb_simple_perror_msg(cur_file);
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
		/* recurse=yes */ ACTION_RECURSE |
		/* followLinks=no */
		/* depthFirst=yes */ ACTION_DEPTHFIRST,
		/* fileAction= */ file_action_grep,
		/* dirAction= */ NULL,
		/* userData= */ &matched,
		/* depth= */ 0);
	return matched;
}

int grep_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int grep_main(int argc, char **argv)
{
	FILE *file;
	int matched;
	char *mopt;
	llist_t *fopt = NULL;

	/* do normal option parsing */
#if ENABLE_FEATURE_GREP_CONTEXT
	char *slines_after;
	char *slines_before;
	char *Copt;

	opt_complementary = "H-h:e::f::C-AB";
	getopt32(argv,
		OPTSTR_GREP,
		&pattern_head, &fopt, &mopt,
		&slines_after, &slines_before, &Copt);

	if (option_mask32 & OPT_C) {
		/* -C unsets prev -A and -B, but following -A or -B
		   may override it */
		if (!(option_mask32 & OPT_A)) /* not overridden */
			slines_after = Copt;
		if (!(option_mask32 & OPT_B)) /* not overridden */
			slines_before = Copt;
		option_mask32 |= OPT_A|OPT_B; /* for parser */
	}
	if (option_mask32 & OPT_A) {
		lines_after = xatoi_u(slines_after);
	}
	if (option_mask32 & OPT_B) {
		lines_before = xatoi_u(slines_before);
	}
	/* sanity checks */
	if (option_mask32 & (OPT_c|OPT_q|OPT_l|OPT_L)) {
		option_mask32 &= ~OPT_n;
		lines_before = 0;
		lines_after = 0;
	} else if (lines_before > 0)
		before_buf = xzalloc(lines_before * sizeof(char *));
#else
	/* with auto sanity checks */
	opt_complementary = "H-h:e::f::c-n:q-n:l-n";
	getopt32(argv, OPTSTR_GREP,
		&pattern_head, &fopt, &mopt);
#endif
	if (option_mask32 & OPT_m) {
		max_matches = xatoi_u(mopt);
	}
	invert_search = ((option_mask32 & OPT_v) != 0); /* 0 | 1 */

	if (pattern_head != NULL) {
		/* convert char **argv to grep_list_data_t */
		llist_t *cur;

		for (cur = pattern_head; cur; cur = cur->link)
			cur->data = new_grep_list_data(cur->data, 0);
	}
	if (option_mask32 & OPT_f)
		load_regexes_from_file(fopt);

	if (ENABLE_FEATURE_GREP_FGREP_ALIAS && applet_name[0] == 'f')
		option_mask32 |= OPT_F;

	if (!(option_mask32 & (OPT_o | OPT_w)))
		reflags = REG_NOSUB;

	if (ENABLE_FEATURE_GREP_EGREP_ALIAS
	 && (applet_name[0] == 'e' || (option_mask32 & OPT_E))
	) {
		reflags |= REG_EXTENDED;
	}

	if (option_mask32 & OPT_i)
		reflags |= REG_ICASE;

	argv += optind;
	argc -= optind;

	/* if we didn't get a pattern from a -e and no command file was specified,
	 * argv[optind] should be the pattern. no pattern, no worky */
	if (pattern_head == NULL) {
		char *pattern;
		if (*argv == NULL)
			bb_show_usage();
		pattern = new_grep_list_data(*argv++, 0);
		llist_add_to(&pattern_head, pattern);
		argc--;
	}

	/* argv[(optind)..(argc-1)] should be names of file to grep through. If
	 * there is more than one file to grep, we will print the filenames. */
	if (argc > 1)
		print_filename = 1;
	/* -H / -h of course override */
	if (option_mask32 & OPT_H)
		print_filename = 1;
	if (option_mask32 & OPT_h)
		print_filename = 0;

	/* If no files were specified, or '-' was specified, take input from
	 * stdin. Otherwise, we grep through all the files specified. */
	matched = 0;
	do {
		cur_file = *argv++;
		file = stdin;
		if (!cur_file || (*cur_file == '-' && !cur_file[1])) {
			cur_file = "(standard input)";
		} else {
			if (option_mask32 & OPT_r) {
				struct stat st;
				if (stat(cur_file, &st) == 0 && S_ISDIR(st.st_mode)) {
					if (!(option_mask32 & OPT_h))
						print_filename = 1;
					matched += grep_dir(cur_file);
					goto grep_done;
				}
			}
			/* else: fopen(dir) will succeed, but reading won't */
			file = fopen(cur_file, "r");
			if (file == NULL) {
				if (!SUPPRESS_ERR_MSGS)
					bb_simple_perror_msg(cur_file);
				open_errors = 1;
				continue;
			}
		}
		matched += grep_file(file);
		fclose_if_not_stdin(file);
 grep_done: ;
	} while (--argc > 0);

	/* destroy all the elments in the pattern list */
	if (ENABLE_FEATURE_CLEAN_UP) {
		while (pattern_head) {
			llist_t *pattern_head_ptr = pattern_head;
			grep_list_data_t *gl = (grep_list_data_t *)pattern_head_ptr->data;

			pattern_head = pattern_head->link;
			if ((gl->flg_mem_alocated_compiled & PATTERN_MEM_A))
				free(gl->pattern);
			if ((gl->flg_mem_alocated_compiled & COMPILED))
				regfree(&(gl->preg));
			free(gl);
			free(pattern_head_ptr);
		}
	}
	/* 0 = success, 1 = failed, 2 = error */
	if (open_errors)
		return 2;
	return !matched; /* invert return value: 0 = success, 1 = failed */
}
