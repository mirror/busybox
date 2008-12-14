/* expand - convert tabs to spaces
 * unexpand - convert spaces to tabs
 *
 * Copyright (C) 89, 91, 1995-2006 Free Software Foundation, Inc.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * David MacKenzie <djm@gnu.ai.mit.edu>
 *
 * Options for expand:
 * -t num  --tabs=NUM      Convert tabs to num spaces (default 8 spaces).
 * -i      --initial       Only convert initial tabs on each line to spaces.
 *
 * Options for unexpand:
 * -a      --all           Convert all blanks, instead of just initial blanks.
 * -f      --first-only    Convert only leading sequences of blanks (default).
 * -t num  --tabs=NUM      Have tabs num characters apart instead of 8.
 *
 *  Busybox version (C) 2007 by Tito Ragusa <farmatito@tiscali.it>
 *
 *  Caveat: this versions of expand and unexpand don't accept tab lists.
 */

#include "libbb.h"

enum {
	OPT_INITIAL     = 1 << 0,
	OPT_TABS        = 1 << 1,
	OPT_ALL         = 1 << 2,
};

#if ENABLE_EXPAND
static void expand(FILE *file, int tab_size, unsigned opt)
{
	char *line;

	tab_size = -tab_size;

	while ((line = xmalloc_fgets(file)) != NULL) {
		int pos;
		unsigned char c;
		char *ptr = line;

		goto start;
		while ((c = *ptr) != '\0') {
			if ((opt & OPT_INITIAL) && !isblank(c)) {
				fputs(ptr, stdout);
				break;
			}
			ptr++;
			if (c == '\t') {
				c = ' ';
				while (++pos < 0)
					bb_putchar(c);
			}
			bb_putchar(c);
			if (++pos >= 0) {
 start:
				pos = tab_size;
			}
		}
		free(line);
	}
}
#endif

#if ENABLE_UNEXPAND
static void unexpand(FILE *file, unsigned tab_size, unsigned opt)
{
	char *line;
	char *ptr;
	int convert;
	int pos;
	int i = 0;
	unsigned column = 0;

	while ((line = xmalloc_fgets(file)) != NULL) {
		convert = 1;
		pos = 0;
		ptr = line;
		while (*line) {
			while ((*line == ' ' || *line == '\t') && convert) {
				pos += (*line == ' ') ? 1 : tab_size;
				line++;
				column++;
				if ((opt & OPT_ALL) && column == tab_size) {
					column = 0;
					goto put_tab;
				}
			}
			if (pos) {
				i = pos / tab_size;
				if (i) {
					for (; i > 0; i--) {
 put_tab:
						bb_putchar('\t');
					}
				} else {
					for (i = pos % tab_size; i > 0; i--) {
						bb_putchar(' ');
					}
				}
				pos = 0;
			} else {
				if (opt & OPT_INITIAL) {
					convert = 0;
				}
				if (opt & OPT_ALL) {
					column++;
				}
				bb_putchar(*line);
				line++;
			}
		}
		free(ptr);
	}
}
#endif

int expand_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int expand_main(int argc UNUSED_PARAM, char **argv)
{
	/* Default 8 spaces for 1 tab */
	const char *opt_t = "8";
	FILE *file;
	unsigned tab_size;
	unsigned opt;
	int exit_status = EXIT_SUCCESS;

#if ENABLE_FEATURE_EXPAND_LONG_OPTIONS
	static const char expand_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"initial\0"          No_argument       "i"
		"tabs\0"             Required_argument "t"
	;
#endif
#if ENABLE_FEATURE_UNEXPAND_LONG_OPTIONS
	static const char unexpand_longopts[] ALIGN1 =
		/* name, has_arg, val */
		"first-only\0"       No_argument       "i"
		"tabs\0"             Required_argument "t"
		"all\0"              No_argument       "a"
	;
#endif

	if (ENABLE_EXPAND && (!ENABLE_UNEXPAND || applet_name[0] == 'e')) {
		USE_FEATURE_EXPAND_LONG_OPTIONS(applet_long_options = expand_longopts);
		opt = getopt32(argv, "it:", &opt_t);
	} else {
		USE_FEATURE_UNEXPAND_LONG_OPTIONS(applet_long_options = unexpand_longopts);
		/* -t NUM sets also -a */
		opt_complementary = "ta";
		opt = getopt32(argv, "ft:a", &opt_t);
		/* -f --first-only is the default */
		if (!(opt & OPT_ALL)) opt |= OPT_INITIAL;
	}
	tab_size = xatou_range(opt_t, 1, UINT_MAX);

	argv += optind;

	if (!*argv) {
		*--argv = (char*)bb_msg_standard_input;
	}
	do {
		file = fopen_or_warn_stdin(*argv);
		if (!file) {
			exit_status = EXIT_FAILURE;
			continue;
		}

		if (ENABLE_EXPAND && (!ENABLE_UNEXPAND || applet_name[0] == 'e'))
			USE_EXPAND(expand(file, tab_size, opt));
		else
			USE_UNEXPAND(unexpand(file, tab_size, opt));

		/* Check and close the file */
		if (fclose_if_not_stdin(file)) {
			bb_simple_perror_msg(*argv);
			exit_status = EXIT_FAILURE;
		}
		/* If stdin also clear EOF */
		if (file == stdin)
			clearerr(file);
	} while (*++argv);

	/* Now close stdin also */
	/* (if we didn't read from it, it's a no-op) */
	if (fclose(stdin))
		bb_perror_msg_and_die(bb_msg_standard_input);

	fflush_stdout_and_exit(exit_status);
}
