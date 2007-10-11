/* vi: set sw=4 ts=4: */
/*
 * Mini rmmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <sys/syscall.h>

#if ENABLE_FEATURE_2_6_MODULES
static inline void filename2modname(char *modname, const char *afterslash)
{
	unsigned int i;
	int kr_chk = 1;

	if (ENABLE_FEATURE_2_4_MODULES
			&& get_linux_version_code() <= KERNEL_VERSION(2,6,0))
				kr_chk = 0;

	/* Convert to underscores, stop at first . */
	for (i = 0; afterslash[i] && afterslash[i] != '.'; i++) {
		if (kr_chk && (afterslash[i] == '-'))
			modname[i] = '_';
		else
			modname[i] = afterslash[i];
	}
	modname[i] = '\0';
}
#else
void filename2modname(char *modname, const char *afterslash);
#endif

// There really should be a header file for this...

int query_module(const char *name, int which, void *buf,
			size_t bufsize, size_t *ret);

int rmmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int rmmod_main(int argc, char **argv)
{
	int n, ret = EXIT_SUCCESS;
	unsigned int flags = O_NONBLOCK|O_EXCL;

#define misc_buf bb_common_bufsiz1

	/* Parse command line. */
	n = getopt32(argv, "wfa");
	if (n & 1)	// --wait
		flags &= ~O_NONBLOCK;
	if (n & 2)	// --force
		flags |= O_TRUNC;
	if (n & 4) {
		/* Unload _all_ unused modules via NULL delete_module() call */
		/* until the number of modules does not change */
		size_t nmod = 0; /* number of modules */
		size_t pnmod = -1; /* previous number of modules */

		while (nmod != pnmod) {
			if (syscall(__NR_delete_module, NULL, flags) != 0) {
				if (errno == EFAULT)
					return ret;
				bb_perror_msg_and_die("rmmod");
			}
			pnmod = nmod;
			// the 1 here is QM_MODULES.
			if (ENABLE_FEATURE_QUERY_MODULE_INTERFACE && query_module(NULL,
					1, misc_buf, sizeof(misc_buf),
					&nmod))
			{
				bb_perror_msg_and_die("QM_MODULES");
			}
		}
		return EXIT_SUCCESS;
	}

	if (optind == argc)
		bb_show_usage();

	for (n = optind; n < argc; n++) {
		if (ENABLE_FEATURE_2_6_MODULES) {
			filename2modname(misc_buf, bb_basename(argv[n]));
		}

		if (syscall(__NR_delete_module, ENABLE_FEATURE_2_6_MODULES ? misc_buf : argv[n], flags)) {
			bb_simple_perror_msg(argv[n]);
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}
