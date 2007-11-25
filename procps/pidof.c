/* vi: set sw=4 ts=4: */
/*
 * pidof implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */

#include "libbb.h"

enum {
	USE_FEATURE_PIDOF_SINGLE(OPTBIT_SINGLE,)
	USE_FEATURE_PIDOF_OMIT(  OPTBIT_OMIT  ,)
	OPT_SINGLE = USE_FEATURE_PIDOF_SINGLE((1<<OPTBIT_SINGLE)) + 0,
	OPT_OMIT   = USE_FEATURE_PIDOF_OMIT(  (1<<OPTBIT_OMIT  )) + 0,
};

int pidof_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int pidof_main(int argc, char **argv)
{
	unsigned first = 1;
	unsigned opt;
#if ENABLE_FEATURE_PIDOF_OMIT
	char ppid_str[sizeof(int)*3 + 1];
	llist_t *omits = NULL; /* list of pids to omit */
	opt_complementary = "o::";
#endif

	/* do unconditional option parsing */
	opt = getopt32(argv, ""
			USE_FEATURE_PIDOF_SINGLE ("s")
			USE_FEATURE_PIDOF_OMIT("o:", &omits));

#if ENABLE_FEATURE_PIDOF_OMIT
	/* fill omit list.  */
	{
		llist_t *omits_p = omits;
		while (omits_p) {
			/* are we asked to exclude the parent's process ID?  */
			if (strcmp(omits_p->data, "%PPID") == 0) {
				sprintf(ppid_str, "%u", (unsigned)getppid());
				omits_p->data = ppid_str;
			}
			omits_p = omits_p->link;
		}
	}
#endif
	/* Looks like everything is set to go.  */
	argv += optind;
	while (*argv) {
		pid_t *pidList;
		pid_t *pl;

		/* reverse the pidlist like GNU pidof does.  */
		pidList = pidlist_reverse(find_pid_by_name(*argv));
		for (pl = pidList; *pl; pl++) {
#if ENABLE_FEATURE_PIDOF_OMIT
			if (opt & OPT_OMIT) {
				llist_t *omits_p = omits;
				while (omits_p) {
					if (xatoul(omits_p->data) == *pl) {
						goto omitting;
					}
					omits_p = omits_p->link;
				}
			}
#endif
			printf(" %u" + first, (unsigned)*pl);
			first = 0;
			if (ENABLE_FEATURE_PIDOF_SINGLE && (opt & OPT_SINGLE))
				break;
#if ENABLE_FEATURE_PIDOF_OMIT
 omitting: ;
#endif
		}
		free(pidList);
		argv++;
	}
	if (!first)
		bb_putchar('\n');

#if ENABLE_FEATURE_PIDOF_OMIT
	if (ENABLE_FEATURE_CLEAN_UP)
		llist_free(omits, NULL);
#endif
	return first; /* 1 (failure) - no processes found */
}
