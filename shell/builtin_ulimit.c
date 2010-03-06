/* vi: set sw=4 ts=4: */
/*
 * ulimit builtin
 *
 * Adapted from ash applet code
 *
 * This code, originally by Doug Gwyn, Doug Kingston, Eric Gisin, and
 * Michael Rendell was ripped from pdksh 5.0.8 and hacked for use with
 * ash by J.T. Conklin.
 *
 * Public domain.
 *
 * Copyright (c) 2010 Tobias Klauser
 * Split from ash.c and slightly adapted.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
#include "libbb.h"
#include "builtin_ulimit.h"


struct limits {
	uint8_t cmd;            /* RLIMIT_xxx fit into it */
	uint8_t factor_shift;   /* shift by to get rlim_{cur,max} values */
	char option;
	const char *name;
};

static const struct limits limits_tbl[] = {
#ifdef RLIMIT_FSIZE
	{ RLIMIT_FSIZE,		9,	'f',	"file size (blocks)" },
#endif
#ifdef RLIMIT_CPU
	{ RLIMIT_CPU,		0,	't',	"cpu time (seconds)" },
#endif
#ifdef RLIMIT_DATA
	{ RLIMIT_DATA,		10,	'd',	"data seg size (kb)" },
#endif
#ifdef RLIMIT_STACK
	{ RLIMIT_STACK,		10,	's',	"stack size (kb)" },
#endif
#ifdef RLIMIT_CORE
	{ RLIMIT_CORE,		9,	'c',	"core file size (blocks)" },
#endif
#ifdef RLIMIT_RSS
	{ RLIMIT_RSS,		10,	'm',	"resident set size (kb)" },
#endif
#ifdef RLIMIT_MEMLOCK
	{ RLIMIT_MEMLOCK,	10,	'l',	"locked memory (kb)" },
#endif
#ifdef RLIMIT_NPROC
	{ RLIMIT_NPROC,		0,	'p',	"processes" },
#endif
#ifdef RLIMIT_NOFILE
	{ RLIMIT_NOFILE,	0,	'n',	"file descriptors" },
#endif
#ifdef RLIMIT_AS
	{ RLIMIT_AS,		10,	'v',	"address space (kb)" },
#endif
#ifdef RLIMIT_LOCKS
	{ RLIMIT_LOCKS,		0,	'w',	"locks" },
#endif
};

enum {
	OPT_hard = (1 << 0),
	OPT_soft = (1 << 1),
};

/* "-": treat args as parameters of option with ASCII code 1 */
static const char ulimit_opt_string[] = "-HSa"
#ifdef RLIMIT_FSIZE
			"f::"
#endif
#ifdef RLIMIT_CPU
			"t::"
#endif
#ifdef RLIMIT_DATA
			"d::"
#endif
#ifdef RLIMIT_STACK
			"s::"
#endif
#ifdef RLIMIT_CORE
			"c::"
#endif
#ifdef RLIMIT_RSS
			"m::"
#endif
#ifdef RLIMIT_MEMLOCK
			"l::"
#endif
#ifdef RLIMIT_NPROC
			"p::"
#endif
#ifdef RLIMIT_NOFILE
			"n::"
#endif
#ifdef RLIMIT_AS
			"v::"
#endif
#ifdef RLIMIT_LOCKS
			"w::"
#endif
			;

static void printlim(unsigned opts, const struct rlimit *limit,
			const struct limits *l)
{
	rlim_t val;

	val = limit->rlim_max;
	if (!(opts & OPT_hard))
		val = limit->rlim_cur;

	if (val == RLIM_INFINITY)
		printf("unlimited\n");
	else {
		val >>= l->factor_shift;
		printf("%llu\n", (long long) val);
	}
}

int FAST_FUNC shell_builtin_ulimit(char **argv)
{
	unsigned opts;
	unsigned argc;

	/* We can't use getopt32: need to handle commands like
	 * ulimit 123 -c2 -l 456
	 */

	/* In case getopt was already called:
	 * reset the libc getopt() function, which keeps internal state.
	 */
#ifdef __GLIBC__
	optind = 0;
#else /* BSD style */
	optind = 1;
	/* optreset = 1; */
#endif
	/* optarg = NULL; opterr = 0; optopt = 0; - do we need this?? */

        argc = 1;
        while (argv[argc])
                argc++;

	opts = 0;
	while (1) {
		struct rlimit limit;
		const struct limits *l;
		int opt_char = getopt(argc, argv, ulimit_opt_string);

		if (opt_char == -1)
			break;
		if (opt_char == 'H') {
			opts |= OPT_hard;
			continue;
		}
		if (opt_char == 'S') {
			opts |= OPT_soft;
			continue;
		}

		if (opt_char == 'a') {
			for (l = limits_tbl; l != &limits_tbl[ARRAY_SIZE(limits_tbl)]; l++) {
				getrlimit(l->cmd, &limit);
				printf("-%c: %-30s ", l->option, l->name);
				printlim(opts, &limit, l);
			}
			continue;
		}

		if (opt_char == 1)
			opt_char = 'f';
		for (l = limits_tbl; l != &limits_tbl[ARRAY_SIZE(limits_tbl)]; l++) {
			if (opt_char == l->option) {
				char *val_str = optarg ? optarg : (argv[optind] && argv[optind][0] != '-' ? argv[optind] : NULL);

				getrlimit(l->cmd, &limit);
				if (val_str) {
					rlim_t val;

					if (!optarg) /* -c NNN: make getopt skip NNN */
						optind++;

					if (strcmp(val_str, "unlimited") == 0)
						val = RLIM_INFINITY;
					else {
						if (sizeof(val) == sizeof(int))
							val = bb_strtou(val_str, NULL, 10);
						else if (sizeof(val) == sizeof(long))
							val = bb_strtoul(val_str, NULL, 10);
						else
							val = bb_strtoull(val_str, NULL, 10);
						if (errno) {
							bb_error_msg("bad number");
							return EXIT_FAILURE;
						}
						val <<= l->factor_shift;
					}
//bb_error_msg("opt %c val_str:'%s' val:%lld", opt_char, val_str, (long long)val);
					if (opts & OPT_hard)
						limit.rlim_max = val;
					if ((opts & OPT_soft) || opts == 0)
						limit.rlim_cur = val;
//bb_error_msg("setrlimit(%d, %lld, %lld)", l->cmd, (long long)limit.rlim_cur, (long long)limit.rlim_max);
					if (setrlimit(l->cmd, &limit) < 0) {
						bb_perror_msg("error setting limit");
						return EXIT_FAILURE;
					}
				} else {
					printlim(opts, &limit, l);
				}
				break;
			}
		} /* for (every possible opt) */

		if (l == &limits_tbl[ARRAY_SIZE(limits_tbl)]) {
			/* bad option. getopt already complained. */
			break;
		}

	} /* while (there are options) */

	return 0;
}
