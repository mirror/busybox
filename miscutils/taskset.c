/* vi: set sw=4 ts=4: */
/*
 * taskset - retrieve or set a processes's CPU affinity
 * Copyright (c) 2006 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <sched.h>
#include <unistd.h>
#include <getopt.h> /* optind */

int taskset_main(int argc, char** argv)
{
	cpu_set_t mask, new_mask;
	pid_t pid = 0;
	unsigned long ul;
	const char *state = "current\0new";
	char *p_opt = NULL, *aff = NULL;

	ul = bb_getopt_ulflags(argc, argv, "+p:", &p_opt);
#define TASKSET_OPT_p (1)

	if (ul & TASKSET_OPT_p) {
		if (argc == optind+1) { /* -p <aff> <pid> */
			aff = p_opt;
			p_opt = argv[optind];
		}
		argv += optind; /* me -p <arg> */
		pid = bb_xgetularg10_bnd(p_opt, 1, ULONG_MAX); /* -p <pid> */
	} else
		aff = *++argv; /* <aff> <cmd...> */
	if (aff) {
/*		to_cpuset(bb_xgetularg_bnd(aff, 16, 1, ULONG_MAX), &new_mask); */
		unsigned i = 0;
		unsigned long l = bb_xgetularg_bnd(aff, 16, 1, ULONG_MAX);

		CPU_ZERO(&new_mask);
		while (i < CPU_SETSIZE && l >= (1<<i)) {
			if ((1<<i) & l)
				CPU_SET(i, &new_mask);
			++i;
		}
	}

	if (ul & TASKSET_OPT_p) {
print_aff:
		if (sched_getaffinity(pid, sizeof (mask), &mask) < 0)
			bb_perror_msg_and_die("Failed to %cet pid %d's affinity", 'g', pid);
		bb_printf("pid %d's %s affinity mask: %x\n", /* %x .. perhaps _FANCY */
				pid, state, mask);
		if (!*argv) /* no new affinity given or we did print already, done. */
			return EXIT_SUCCESS;
	}

	if (sched_setaffinity(pid, sizeof (new_mask), &new_mask))
		bb_perror_msg_and_die("Failed to %cet pid %d's affinity", 's', pid);
	if (ul & TASKSET_OPT_p) {
		state += 8;
		++argv;
		goto print_aff;
	}
	++argv;
	execvp(*argv, argv);
	bb_perror_msg_and_die("%s", *argv);
}
