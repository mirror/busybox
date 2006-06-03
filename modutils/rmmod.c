/* vi: set sw=4 ts=4: */
/*
 * Mini rmmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/syscall.h>

#ifdef CONFIG_FEATURE_2_6_MODULES
static inline void filename2modname(char *modname, const char *afterslash)
{
	unsigned int i;

#if ENABLE_FEATURE_2_4_MODULES
	int kr_chk = 1;
	if (get_linux_version_code() <= KERNEL_VERSION(2,6,0))
		kr_chk = 0;
#else
#define kr_chk 1
#endif

	/* Convert to underscores, stop at first . */
	for (i = 0; afterslash[i] && afterslash[i] != '.'; i++) {
		if (kr_chk && (afterslash[i] == '-'))
			modname[i] = '_';
		else
			modname[i] = afterslash[i];
	}
	modname[i] = '\0';
}
#endif

int rmmod_main(int argc, char **argv)
{
	int n, ret = EXIT_SUCCESS;
	unsigned int flags = O_NONBLOCK|O_EXCL;
#ifdef CONFIG_FEATURE_QUERY_MODULE_INTERFACE
	/* bb_common_bufsiz1 hold the module names which we ignore
	   but must get */
	size_t bufsize = sizeof(bb_common_bufsiz1);
#endif

	/* Parse command line. */
	n = bb_getopt_ulflags(argc, argv, "wfa");
	if((n & 1))	// --wait
		flags &= ~O_NONBLOCK;
	if((n & 2))	// --force
		flags |= O_TRUNC;
	if((n & 4)) {
		/* Unload _all_ unused modules via NULL delete_module() call */
		/* until the number of modules does not change */
		size_t nmod = 0; /* number of modules */
		size_t pnmod = -1; /* previous number of modules */

		while (nmod != pnmod) {
			if (syscall(__NR_delete_module, NULL, flags) != 0) {
				if (errno==EFAULT)
					return(ret);
				bb_perror_msg_and_die("rmmod");
			}
			pnmod = nmod;
#ifdef CONFIG_FEATURE_QUERY_MODULE_INTERFACE
			/* 1 == QM_MODULES */
			if (my_query_module(NULL, 1, &bb_common_bufsiz1, &bufsize, &nmod)) {
				bb_perror_msg_and_die("QM_MODULES");
			}
#endif
		}
		return EXIT_SUCCESS;
	}

	if (optind == argc)
		bb_show_usage();

	for (n = optind; n < argc; n++) {
#ifdef CONFIG_FEATURE_2_6_MODULES
		const char *afterslash;
		char *module_name;

		afterslash = strrchr(argv[n], '/');
		if (!afterslash)
			afterslash = argv[n];
		else
			afterslash++;
		module_name = alloca(strlen(afterslash) + 1);
		filename2modname(module_name, afterslash);
#else
#define module_name		argv[n]
#endif
		if (syscall(__NR_delete_module, module_name, flags) != 0) {
			bb_perror_msg("%s", argv[n]);
			ret = EXIT_FAILURE;
		}
	}

	return(ret);
}
