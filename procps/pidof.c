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
static llist_t *omits; /* list of pids to omit */
#if ENABLE_FEATURE_PIDOF_SINGLE
#define OMIT (1<<1)
#else
#define OMIT (1<<0)
#endif
#else
#define _OMIT_COMPL(a) ""
#define _OMIT(a)
#define OMIT (0)
#endif

extern int pidof_main(int argc, char **argv)
{
	int n = 0;
	int fail = 1;
	unsigned long int opt;
#if ENABLE_FEATURE_PIDOF_OMIT
	bb_opt_complementally = _OMIT_COMPL("o*");
#endif

	/* do option parsing */
	if ((opt = bb_getopt_ulflags(argc, argv,
					_SINGLE_COMPL("s") _OMIT_COMPL("o:")
					_OMIT(&omits)))
			> 0) {
#if ENABLE_FEATURE_PIDOF_SINGLE
		if (!(opt & SINGLE))
#endif
#if ENABLE_FEATURE_PIDOF_OMIT
		if (!(opt & OMIT))
#endif
		bb_show_usage();
	}

#if ENABLE_FEATURE_PIDOF_OMIT
	/* fill omit list.  */
	{
	RESERVE_CONFIG_BUFFER(getppid_str, 32);
	llist_t * omits_p = omits;
	while (omits_p) {
		/* are we asked to exclude the parent's process ID?  */
		if (omits_p->data[0] == '%') {
			if (!strncmp(omits_p->data, "%PPID", 5)) {
				/* yes, exclude ppid */
				snprintf(getppid_str, sizeof(getppid_str), "%ld", getppid());
//				omits_p->data = getppid_str;
#if 0
			} else {
				bb_error_msg_and_die("illegal omit pid value (%s)!\n",
						omits_p->data);
#endif
			}
		} else {
		/* no, not talking about ppid but deal with usual case (pid).  */
			snprintf(getppid_str, sizeof(getppid_str), "%ld",
					strtol(omits_p->data, NULL, 10));
		}
		omits_p->data = getppid_str;
		omits_p = omits_p->link;
	}
	RELEASE_CONFIG_BUFFER(getppid_str);
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
			unsigned short omitted = 0;
			if (opt & OMIT) {
				llist_t *omits_p = omits;
				while (omits_p)
					if (strtol(omits_p->data, NULL, 10) == *pl) {
						omitted = 1; break;
					} else
						omits_p = omits_p->link;
			}
			if (!omitted)
#endif
			printf("%s%ld", (n++ ? " " : ""), *pl);
#if ENABLE_FEATURE_PIDOF_OMIT
			fail = omitted;
#else
			fail = 0;
#endif
#if ENABLE_FEATURE_PIDOF_SINGLE
			if (opt & SINGLE)
				break;
#endif
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
