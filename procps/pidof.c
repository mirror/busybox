/* vi: set sw=4 ts=4: */
/*
 * pidof implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "busybox.h"

#if ENABLE_FEATURE_PIDOF_SINGLE
#define _SINGLE_COMPL(a) a
#define SINGLE (1<<0)
#else
#define _SINGLE_COMPL(a)
#define SINGLE (0)
#endif

#if ENABLE_FEATURE_PIDOF_OMIT
#define _OMIT_COMPL(a) a
#define _OMIT(a) ,a
#if ENABLE_FEATURE_PIDOF_SINGLE
#define OMIT (1<<1)
#else
#define OMIT (1<<0)
#endif
#else
#define _OMIT_COMPL(a) ""
#define _OMIT(a)
#define OMIT (0)
#define omitted (0)
#endif

int pidof_main(int argc, char **argv)
{
	unsigned n = 0;
	unsigned fail = 1;
	unsigned long int opt;
#if ENABLE_FEATURE_PIDOF_OMIT
	llist_t *omits = NULL; /* list of pids to omit */
	bb_opt_complementally = _OMIT_COMPL("o::");
#endif

	/* do unconditional option parsing */
	opt = bb_getopt_ulflags(argc, argv,
					_SINGLE_COMPL("s") _OMIT_COMPL("o:")
					_OMIT(&omits));

#if ENABLE_FEATURE_PIDOF_OMIT
	/* fill omit list.  */
	{
		char getppid_str[32];
		llist_t * omits_p = omits;
		while (omits_p) {
			/* are we asked to exclude the parent's process ID?  */
			if (!strncmp(omits_p->data, "%PPID", 5)) {
				omits_p = llist_free_one(omits_p);
				snprintf(getppid_str, sizeof(getppid_str), "%d", getppid());
				omits_p = llist_add_to(omits_p, getppid_str);
#if 0
			} else {
				bb_error_msg_and_die("illegal omit pid value (%s)!\n",
						omits_p->data);
#endif
			}
			omits_p = omits_p->link;
		}
	}
#endif
	/* Looks like everything is set to go.  */
	while(optind < argc) {
		long *pidList;
		long *pl;

		/* reverse the pidlist like GNU pidof does.  */
		pidList = pidlist_reverse(find_pid_by_name(argv[optind]));
		for(pl = pidList; *pl > 0; pl++) {
#if ENABLE_FEATURE_PIDOF_OMIT
			unsigned omitted = 0;
			if (opt & OMIT) {
				llist_t *omits_p = omits;
				while (omits_p)
					if (strtol(omits_p->data, NULL, 10) == *pl) {
						omitted = 1; break;
					} else
						omits_p = omits_p->link;
			}
#endif
			if (!omitted) {
				if (n) {
					putchar(' ');
				} else {
					n = 1;
				}
				printf("%ld", *pl);
			}
			fail = (!ENABLE_FEATURE_PIDOF_OMIT && omitted);

			if (ENABLE_FEATURE_PIDOF_SINGLE && (opt & SINGLE))
				break;
		}
		free(pidList);
		optind++;
	}
	putchar('\n');

#if ENABLE_FEATURE_PIDOF_OMIT
	if (ENABLE_FEATURE_CLEAN_UP)
		llist_free(omits);
#endif
	return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
