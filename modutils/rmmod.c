/* vi: set sw=4 ts=4: */
/*
 * Mini rmmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include "busybox.h"

#ifdef CONFIG_FEATURE_2_6_MODULES
static inline void filename2modname(char *modname, const char *afterslash)
{
	unsigned int i;

#if ENABLE_FEATURE_2_4_MODULES
	int kr_chk = 1;
	if (get_kernel_revision() <= 2*65536+6*256)
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
