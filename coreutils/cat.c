/* vi: set sw=4 ts=4: */
/*
 * cat implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config CAT
//config:	bool "cat"
//config:	default y
//config:	help
//config:	  cat is used to concatenate files and print them to the standard
//config:	  output. Enable this option if you wish to enable the 'cat' utility.
//config:
//config:config FEATURE_CATV
//config:	bool "cat -v[etA]"
//config:	default y
//config:	depends on CAT
//config:	help
//config:	  Display nonprinting characters as escape sequences

//applet:IF_CAT(APPLET(cat, BB_DIR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_CAT) += cat.o

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cat.html */

//usage:#define cat_trivial_usage
//usage:       "[-nb"IF_FEATURE_CATV("vteA")"] [FILE]..."
//usage:#define cat_full_usage "\n\n"
//usage:       "Print FILEs to stdout\n"
//usage:     "\n	-n	Number output lines"
//usage:     "\n	-b	Number nonempty lines"
//usage:	IF_FEATURE_CATV(
//usage:     "\n	-v	Show nonprinting characters as ^x or M-x"
//usage:     "\n	-t	...and tabs as ^I"
//usage:     "\n	-e	...and end lines with $"
//usage:     "\n	-A	Same as -vte"
//usage:	)
/*
  Longopts not implemented yet:
      --number-nonblank    number nonempty output lines, overrides -n
      --number             number all output lines
      --show-nonprinting   use ^ and M- notation, except for LFD and TAB
      --show-all           equivalent to -vet
  Not implemented yet:
  -E, --show-ends          display $ at end of each line (-e sans -v)
  -T, --show-tabs          display TAB characters as ^I (-t sans -v)
  -s, --squeeze-blank      suppress repeated empty output lines
*/
//usage:
//usage:#define cat_example_usage
//usage:       "$ cat /proc/uptime\n"
//usage:       "110716.72 17.67"

#include "libbb.h"
#include "common_bufsiz.h"

#if ENABLE_FEATURE_CATV
/*
 * cat -v implementation for busybox
 *
 * Copyright (C) 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
/* Rob had "cat -v" implemented as a separate applet, catv.
 * See "cat -v considered harmful" at
 * http://cm.bell-labs.com/cm/cs/doc/84/kp.ps.gz
 * From USENIX Summer Conference Proceedings, 1983
 * """
 * The talk reviews reasons for UNIX's popularity and shows, using UCB cat
 * as a primary example, how UNIX has grown fat. cat isn't for printing
 * files with line numbers, it isn't for compressing multiple blank lines,
 * it's not for looking at non-printing ASCII characters, it's for
 * concatenating files.
 * We are reminded that ls isn't the place for code to break a single column
 * into multiple ones, and that mailnews shouldn't have its own more
 * processing or joke encryption code.
 * """
 *
 * I agree with the argument. Unfortunately, this ship has sailed (1983...).
 * There are dozens of Linux distros and each of them has "cat" which supports -v.
 * It's unrealistic for us to "reeducate" them to use our, incompatible way
 * to achieve "cat -v" effect. The actuall effect would be "users pissed off
 * by gratuitous incompatibility".
 */
#define CATV_OPT_e (1<<0)
#define CATV_OPT_t (1<<1)
#define CATV_OPT_v (1<<2)
static int catv(unsigned opts, char **argv)
{
	int retval = EXIT_SUCCESS;
	int fd;

	BUILD_BUG_ON(CATV_OPT_e != VISIBLE_ENDLINE);
	BUILD_BUG_ON(CATV_OPT_t != VISIBLE_SHOW_TABS);
#if 0 /* These consts match, we can just pass "opts" to visible() */
	if (opts & CATV_OPT_e)
		flags |= VISIBLE_ENDLINE;
	if (opts & CATV_OPT_t)
		flags |= VISIBLE_SHOW_TABS;
#endif

	/* Read from stdin if there's nothing else to do. */
	if (!argv[0])
		*--argv = (char*)"-";

#define read_buf bb_common_bufsiz1
	setup_common_bufsiz();
	do {
		fd = open_or_warn_stdin(*argv);
		if (fd < 0) {
			retval = EXIT_FAILURE;
			continue;
		}
		for (;;) {
			int i, res;

			res = read(fd, read_buf, COMMON_BUFSIZE);
			if (res < 0)
				retval = EXIT_FAILURE;
			if (res <= 0)
				break;
			for (i = 0; i < res; i++) {
				unsigned char c = read_buf[i];
				char buf[sizeof("M-^c")];
				visible(c, buf, opts);
				fputs(buf, stdout);
			}
		}
		if (ENABLE_FEATURE_CLEAN_UP && fd)
			close(fd);
	} while (*++argv);

	fflush_stdout_and_exit(retval);
}
#endif

int cat_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cat_main(int argc UNUSED_PARAM, char **argv)
{
	struct number_state ns;
	unsigned opts;

	IF_FEATURE_CATV(opt_complementary = "Aetv"; /* -A == -vet */)
	/* -u is ignored ("unbuffered") */
	opts = getopt32(argv, IF_FEATURE_CATV("etvA")"nbu");
	argv += optind;

#if ENABLE_FEATURE_CATV
	if (opts & 7)
		return catv(opts, argv);
//BUG: -v,-e,-t,-A ignore -nb
	opts >>= 4;
#endif

#define CAT_OPT_n (1<<0)
#define CAT_OPT_b (1<<1)
#define CAT_OPT_u (1<<2)
	if (!(opts & (CAT_OPT_n|CAT_OPT_b))) /* no -n or -b */
		return bb_cat(argv);

	if (!*argv)
		*--argv = (char*)"-";
	ns.width = 6;
	ns.start = 1;
	ns.inc = 1;
	ns.sep = "\t";
	ns.empty_str = "\n";
	ns.all = !(opts & CAT_OPT_b); /* -n without -b */
	ns.nonempty = (opts & CAT_OPT_b); /* -b (with or without -n) */
	do {
		print_numbered_lines(&ns, *argv);
	} while (*++argv);
	fflush_stdout_and_exit(EXIT_SUCCESS);
}
