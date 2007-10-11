/* vi: set sw=4 ts=4: */
/*
 * chrt - manipulate real-time attributes of a process
 * Copyright (c) 2006-2007 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <sched.h>
#include <getopt.h> /* optind */
#include "libbb.h"
#ifndef _POSIX_PRIORITY_SCHEDULING
#warning your system may be foobared
#endif
static const struct {
	const int policy;
	const char const name[12];
} policies[] = {
	{SCHED_OTHER, "SCHED_OTHER"},
	{SCHED_FIFO, "SCHED_FIFO"},
	{SCHED_RR, "SCHED_RR"}
};

static void show_min_max(int pol)
{
	const char *fmt = "%s min/max priority\t: %d/%d\n\0%s not supported?\n";
	int max, min;
	max = sched_get_priority_max(pol);
	min = sched_get_priority_min(pol);
	if (max >= 0 && min >= 0)
		printf(fmt, policies[pol].name, min, max);
	else {
		fmt += 29;
		printf(fmt, policies[pol].name);
	}
}

#define OPT_m (1<<0)
#define OPT_p (1<<1)
#define OPT_r (1<<2)
#define OPT_f (1<<3)
#define OPT_o (1<<4)

int chrt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chrt_main(int argc, char **argv)
{
	pid_t pid = 0;
	unsigned opt;
	struct sched_param sp;
	char *p_opt = NULL, *priority = NULL;
	const char *state = "current\0new";
	int prio = 0, policy = SCHED_RR;

	opt_complementary = "r--fo:f--ro:r--fo"; /* only one policy accepted */
	opt = getopt32(argv, "+mp:rfo", &p_opt);
	if (opt & OPT_r)
		policy = SCHED_RR;
	if (opt & OPT_f)
		policy = SCHED_FIFO;
	if (opt & OPT_o)
		policy = SCHED_OTHER;

	if (opt & OPT_m) { /* print min/max */
		show_min_max(SCHED_FIFO);
		show_min_max(SCHED_RR);
		show_min_max(SCHED_OTHER);
		fflush_stdout_and_exit(EXIT_SUCCESS);
	}
	if (opt & OPT_p) {
		if (argc == optind+1) { /* -p <priority> <pid> */
			priority = p_opt;
			p_opt = argv[optind];
		}
		argv += optind; /* me -p <arg> */
		pid = xatoul_range(p_opt, 1, ULONG_MAX); /* -p <pid> */
	} else {
		argv += optind; /* me -p <arg> */
		priority = *argv;
	}
	if (priority) {
		/* from the manpage of sched_getscheduler:
		   [...] sched_priority can have a value
		   in the range 0 to 99.
		   [...] SCHED_OTHER or SCHED_BATCH  must  be  assigned
		   the  static  priority  0. [...] SCHED_FIFO  or
		   SCHED_RR can have a static priority in the range 1 to 99.
		 */
		prio = xstrtol_range(priority, 0, policy == SCHED_OTHER
													 ? 0 : 1, 99);
	}

	if (opt & OPT_p) {
		int pol = 0;
print_rt_info:
		pol = sched_getscheduler(pid);
		if (pol < 0)
			bb_perror_msg_and_die("failed to %cet pid %d's policy", 'g', pid);
		printf("pid %d's %s scheduling policy: %s\n",
				pid, state, policies[pol].name);
		if (sched_getparam(pid, &sp))
			bb_perror_msg_and_die("failed to get pid %d's attributes", pid);
		printf("pid %d's %s scheduling priority: %d\n",
				pid, state, sp.sched_priority);
		if (!*argv) /* no new prio given or we did print already, done. */
			return EXIT_SUCCESS;
	}

	sp.sched_priority = prio;
	if (sched_setscheduler(pid, policy, &sp) < 0)
		bb_perror_msg_and_die("failed to %cet pid %d's policy", 's', pid);
	if (opt & OPT_p) {
		state += 8;
		++argv;
		goto print_rt_info;
	}
	++argv;
	BB_EXECVP(*argv, argv);
	bb_simple_perror_msg_and_die(*argv);
}
#undef OPT_p
#undef OPT_r
#undef OPT_f
#undef OPT_o
#undef OPT_m
