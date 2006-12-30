/* vi: set sw=4 ts=4: */
/*
 * taskset - retrieve or set a processes' CPU affinity
 * Copyright (c) 2006 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <sched.h>
#include <getopt.h> /* optind */

#if ENABLE_FEATURE_TASKSET_FANCY
#define TASKSET_PRINTF_MASK "%s"
#define from_cpuset(x) __from_cpuset(&x)
/* craft a string from the mask */
static char *__from_cpuset(cpu_set_t *mask)
{
	int i;
	char *ret = 0, *str = xzalloc(9);

	for (i = CPU_SETSIZE - 4; i >= 0; i -= 4) {
		char val = 0;
		int off;
		for (off = 0; off <= 3; ++off)
			if (CPU_ISSET(i+off, mask))
				val |= 1<<off;

		if (!ret && val)
			ret = str;
		*str++ = (val-'0'<=9) ? (val+48) : (val+87);
	}
	return ret;
}
#else
#define TASKSET_PRINTF_MASK "%x"
/* (void*) cast is for battling gcc: */
/* "dereferencing type-punned pointer will break strict-aliasing rules" */
#define from_cpuset(mask) (*(unsigned*)(void*)&(mask))
#endif

#define OPT_p 1

int taskset_main(int argc, char** argv)
{
	cpu_set_t mask, new_mask;
	pid_t pid = 0;
	unsigned opt;
	const char *state = "current\0new";
	char *p_opt = NULL, *aff = NULL;

	opt = getopt32(argc, argv, "+p:", &p_opt);

	if (opt & OPT_p) {
		if (argc == optind+1) { /* -p <aff> <pid> */
			aff = p_opt;
			p_opt = argv[optind];
		}
		argv += optind; /* me -p <arg> */
		pid = xatoul_range(p_opt, 1, ULONG_MAX); /* -p <pid> */
	} else
		aff = *++argv; /* <aff> <cmd...> */
	if (aff) {
		unsigned i = 0;
		unsigned long l = xstrtol_range(aff, 16, 1, ULONG_MAX);

		CPU_ZERO(&new_mask);
		while (i < CPU_SETSIZE && l >= (1<<i)) {
			if ((1<<i) & l)
				CPU_SET(i, &new_mask);
			++i;
		}
	}

	if (opt & OPT_p) {
 print_aff:
		if (sched_getaffinity(pid, sizeof(mask), &mask) < 0)
			bb_perror_msg_and_die("failed to %cet pid %d's affinity", 'g', pid);
		printf("pid %d's %s affinity mask: "TASKSET_PRINTF_MASK"\n",
				pid, state, from_cpuset(mask));
		if (!*argv) /* no new affinity given or we did print already, done. */
			return EXIT_SUCCESS;
	}

	if (sched_setaffinity(pid, sizeof(new_mask), &new_mask))
		bb_perror_msg_and_die("failed to %cet pid %d's affinity", 's', pid);
	if (opt & OPT_p) {
		state += 8;
		++argv;
		goto print_aff;
	}
	++argv;
	execvp(*argv, argv);
	bb_perror_msg_and_die("%s", *argv);
}
#undef OPT_p
#undef TASKSET_PRINTF_MASK
#undef from_cpuset
