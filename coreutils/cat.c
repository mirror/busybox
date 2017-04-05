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

//applet:IF_CAT(APPLET_NOFORK(cat, cat, BB_DIR_BIN, BB_SUID_DROP, cat))

//kbuild:lib-$(CONFIG_CAT) += cat.o
// For -n:
//kbuild:lib-$(CONFIG_CAT) += nl.o

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cat.html */

//usage:#define cat_trivial_usage
//usage:       "[-n] [FILE]..."
//usage:#define cat_full_usage "\n\n"
//usage:       "Concatenate FILEs and print them to stdout"
//usage:     "\n	-n	Number output lines"
/*
  Longopts not implemented yet:
     --number-nonblank    number nonempty output lines, overrides -n
     --number             number all output lines
  Not implemented yet:
  -A, --show-all           equivalent to -vET
  -e                       equivalent to -vE
  -E, --show-ends          display $ at end of each line
  -s, --squeeze-blank      suppress repeated empty output lines
  -t                       equivalent to -vT
  -T, --show-tabs          display TAB characters as ^I
  -v, --show-nonprinting   use ^ and M- notation, except for LFD and TAB
*/
//usage:
//usage:#define cat_example_usage
//usage:       "$ cat /proc/uptime\n"
//usage:       "110716.72 17.67"

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */


int bb_cat(char **argv)
{
	int fd;
	int retval = EXIT_SUCCESS;

	if (!*argv)
		argv = (char**) &bb_argv_dash;

	do {
		fd = open_or_warn_stdin(*argv);
		if (fd >= 0) {
			/* This is not a xfunc - never exits */
			off_t r = bb_copyfd_eof(fd, STDOUT_FILENO);
			if (fd != STDIN_FILENO)
				close(fd);
			if (r >= 0)
				continue;
		}
		retval = EXIT_FAILURE;
	} while (*++argv);

	return retval;
}

int cat_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cat_main(int argc UNUSED_PARAM, char **argv)
{
	struct number_state ns;
	unsigned opt;

	/* -u is ignored */
	opt = getopt32(argv, "nbu");
	argv += optind;
	if (!(opt & 3)) /* no -n or -b */
		return bb_cat(argv);

	if (!*argv)
		*--argv = (char*)"-";
	ns.width = 6;
	ns.start = 1;
	ns.inc = 1;
	ns.sep = "\t";
	ns.empty_str = "\n";
	ns.all = !(opt & 2); /* -n without -b */
	ns.nonempty = (opt & 2); /* -b (with or without -n) */
	do {
		print_numbered_lines(&ns, *argv);
	} while (*++argv);
	fflush_stdout_and_exit(EXIT_SUCCESS);
}
