/* vi: set sw=4 ts=4: */
/*
 * Report CPU and I/O stats, based on sysstat version 9.1.2 by Sebastien Godard
 *
 * Copyright (C) 2010 Marek Polacek <mmpolacek@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//applet:IF_IOSTAT(APPLET(iostat, BB_DIR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_IOSTAT) += iostat.o

//config:config IOSTAT
//config:	bool "iostat"
//config:	default y
//config:	help
//config:	  Report CPU and I/O statistics

#include "libbb.h"
#include <sys/utsname.h>  /* Need struct utsname */

//#define debug(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)
#define debug(fmt, ...) ((void)0)

#define MAX_DEVICE_NAME 12

#if 1
typedef unsigned long long cputime_t;
typedef long long icputime_t;
# define FMT_DATA "ll"
# define CPUTIME_MAX (~0ULL)
#else
typedef unsigned long cputime_t;
typedef long icputime_t;
# define FMT_DATA "l"
# define CPUTIME_MAX (~0UL)
#endif

enum {
	STATS_CPU_USER,
	STATS_CPU_NICE,
	STATS_CPU_SYSTEM,
	STATS_CPU_IDLE,
	STATS_CPU_IOWAIT,
	STATS_CPU_IRQ,
	STATS_CPU_SOFTIRQ,
	STATS_CPU_STEAL,
	STATS_CPU_GUEST,

	GLOBAL_UPTIME,
	SMP_UPTIME,

	N_STATS_CPU,
};

typedef struct {
	cputime_t vector[N_STATS_CPU];
} stats_cpu_t;

typedef struct {
	stats_cpu_t *prev;
	stats_cpu_t *curr;
	cputime_t itv;
} stats_cpu_pair_t;

struct stats_dev {
	char dname[MAX_DEVICE_NAME];
	unsigned long long rd_sectors;
	unsigned long long wr_sectors;
	unsigned long rd_ops;
	unsigned long wr_ops;
};

/* Globals. Sort by size and access frequency. */
struct globals {
	smallint show_all;
	unsigned total_cpus;            /* Number of CPUs */
	unsigned clk_tck;               /* Number of clock ticks per second */
	llist_t *dev_list;              /* List of devices entered on the command line */
	struct stats_dev *saved_stats_dev;
	struct tm tmtime;
	struct {
		const char *str;
		unsigned div;
	} unit;
};
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	G.unit.str = "Blk"; \
	G.unit.div = 1; \
} while (0)

/* Must match option string! */
enum {
	OPT_c = 1 << 0,
	OPT_d = 1 << 1,
	OPT_t = 1 << 2,
	OPT_z = 1 << 3,
	OPT_k = 1 << 4,
	OPT_m = 1 << 5,
};

static ALWAYS_INLINE unsigned get_user_hz(void)
{
	return sysconf(_SC_CLK_TCK);
}

static ALWAYS_INLINE int this_is_smp(void)
{
	return (G.total_cpus > 1);
}

static void print_header(void)
{
	char buf[16];
	struct utsname uts;

	uname(&uts); /* never fails */

	/* Date representation for the current locale */
	strftime(buf, sizeof(buf), "%x", &G.tmtime);

	printf("%s %s (%s) \t%s \t_%s_\t(%u CPU)\n\n",
			uts.sysname, uts.release, uts.nodename,
			buf, uts.machine, G.total_cpus);
}

static void get_localtime(struct tm *ptm)
{
	time_t timer;
	time(&timer);
	localtime_r(&timer, ptm);
}

static void print_timestamp(void)
{
	char buf[20];
	/* %x: date representation for the current locale */
	/* %X: time representation for the current locale */
	strftime(buf, sizeof(buf), "%x %X", &G.tmtime);
	printf("%s\n", buf);
}

static cputime_t get_smp_uptime(void)
{
	FILE *fp;
	unsigned long sec, dec;

	fp = xfopen_for_read("/proc/uptime");

	if (fscanf(fp, "%lu.%lu", &sec, &dec) != 2)
		bb_error_msg_and_die("can't read '%s'", "/proc/uptime");

	fclose(fp);

	return (cputime_t)sec * G.clk_tck + dec * G.clk_tck / 100;
}

/* Fetch CPU statistics from /proc/stat */
static void get_cpu_statistics(stats_cpu_t *sc)
{
	FILE *fp;
	char buf[1024], *ibuf = buf + 4;

	fp = xfopen_for_read("/proc/stat");

	memset(sc, 0, sizeof(*sc));

	while (fgets(buf, sizeof(buf), fp)) {
		/* Does the line starts with "cpu "? */
		if (!starts_with_cpu(buf) || buf[3] != ' ') {
			continue;
		}
		for (int i = STATS_CPU_USER; i <= STATS_CPU_GUEST; i++) {
			ibuf = skip_whitespace(ibuf);
			sscanf(ibuf, "%"FMT_DATA"u", &sc->vector[i]);
			if (i != STATS_CPU_GUEST) {
				sc->vector[GLOBAL_UPTIME] += sc->vector[i];
			}
			ibuf = skip_non_whitespace(ibuf);
		}
		break;
	}

	if (this_is_smp()) {
		sc->vector[SMP_UPTIME] = get_smp_uptime();
	}

	fclose(fp);
}

static ALWAYS_INLINE cputime_t get_interval(cputime_t old, cputime_t new)
{
	cputime_t itv = new - old;

	return (itv == 0) ? 1 : itv;
}

#if CPUTIME_MAX > 0xffffffff
/*
 * Handle overflow conditions properly for counters which can have
 * less bits than cputime_t, depending on the kernel version.
 */
/* Surprisingly, on 32bit inlining is a size win */
static ALWAYS_INLINE cputime_t overflow_safe_sub(cputime_t prev, cputime_t curr)
{
	cputime_t v = curr - prev;

	if ((icputime_t)v < 0     /* curr < prev - counter overflow? */
	 && prev <= 0xffffffff /* kernel uses 32bit value for the counter? */
	) {
		/* Add 33th bit set to 1 to curr, compensating for the overflow */
		/* double shift defeats "warning: left shift count >= width of type" */
		v += ((cputime_t)1 << 16) << 16;
	}
	return v;
}
#else
static ALWAYS_INLINE cputime_t overflow_safe_sub(cputime_t prev, cputime_t curr)
{
	return curr - prev;
}
#endif

static double percent_value(cputime_t prev, cputime_t curr, cputime_t itv)
{
	return ((double)overflow_safe_sub(prev, curr)) / itv * 100;
}

static void print_stats_cpu_struct(stats_cpu_pair_t *stats)
{
	cputime_t *p = stats->prev->vector;
	cputime_t *c = stats->curr->vector;
	printf("         %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f\n",
		percent_value(p[STATS_CPU_USER]  , c[STATS_CPU_USER]  , stats->itv),
		percent_value(p[STATS_CPU_NICE]  , c[STATS_CPU_NICE]  , stats->itv),
		percent_value(p[STATS_CPU_SYSTEM] + p[STATS_CPU_SOFTIRQ] + p[STATS_CPU_IRQ],
			c[STATS_CPU_SYSTEM] + c[STATS_CPU_SOFTIRQ] + c[STATS_CPU_IRQ], stats->itv),
		percent_value(p[STATS_CPU_IOWAIT], c[STATS_CPU_IOWAIT], stats->itv),
		percent_value(p[STATS_CPU_STEAL] , c[STATS_CPU_STEAL] , stats->itv),
		percent_value(p[STATS_CPU_IDLE]  , c[STATS_CPU_IDLE]  , stats->itv)
	);
}

static void print_stats_dev_struct(const struct stats_dev *p,
		const struct stats_dev *c, cputime_t itv)
{
	if (option_mask32 & OPT_z)
		if (p->rd_ops == c->rd_ops && p->wr_ops == c->wr_ops)
			return;

	printf("%-13s %8.2f %12.2f %12.2f %10llu %10llu \n", c->dname,
		percent_value(p->rd_ops + p->wr_ops ,
		/**/		  c->rd_ops + c->wr_ops , itv),
		percent_value(p->rd_sectors, c->rd_sectors, itv) / G.unit.div,
		percent_value(p->wr_sectors, c->wr_sectors, itv) / G.unit.div,
		(c->rd_sectors - p->rd_sectors) / G.unit.div,
		(c->wr_sectors - p->wr_sectors) / G.unit.div);
}

static void cpu_report(stats_cpu_pair_t *stats)
{
	/* Always print a header */
	puts("avg-cpu:  %user   %nice %system %iowait  %steal   %idle");

	/* Print current statistics */
	print_stats_cpu_struct(stats);
}

static void print_devstat_header(void)
{
	printf("Device:%15s%6s%s/s%6s%s/s%6s%s%6s%s\n", "tps",
		G.unit.str, "_read", G.unit.str, "_wrtn",
		G.unit.str, "_read", G.unit.str, "_wrtn");
}

/*
 * Is input partition of format [sdaN]?
 */
static int is_partition(const char *dev)
{
	/* Ok, this is naive... */
	return ((dev[0] - 's') | (dev[1] - 'd') | (dev[2] - 'a')) == 0 && isdigit(dev[3]);
}

static void do_disk_statistics(cputime_t itv)
{
	FILE *fp;
	int rc;
	int i = 0;
	char buf[128];
	unsigned major, minor;
	unsigned long wr_ops, dummy; /* %*lu for suppress the conversion wouldn't work */
	unsigned long long rd_sec_or_wr_ops;
	unsigned long long rd_sec_or_dummy, wr_sec_or_dummy, wr_sec;
	struct stats_dev sd;

	fp = xfopen_for_read("/proc/diskstats");

	/* Read and possibly print stats from /proc/diskstats */
	while (fgets(buf, sizeof(buf), fp)) {
		rc = sscanf(buf, "%u %u %s %lu %llu %llu %llu %lu %lu %llu %lu %lu %lu %lu",
			&major, &minor, sd.dname, &sd.rd_ops,
			&rd_sec_or_dummy, &rd_sec_or_wr_ops, &wr_sec_or_dummy,
			&wr_ops, &dummy, &wr_sec, &dummy, &dummy, &dummy, &dummy);

		switch (rc) {
		case 14:
			sd.wr_ops = wr_ops;
			sd.rd_sectors = rd_sec_or_wr_ops;
			sd.wr_sectors = wr_sec;
			break;
		case 7:
			sd.rd_sectors = rd_sec_or_dummy;
			sd.wr_ops = (unsigned long)rd_sec_or_wr_ops;
			sd.wr_sectors = wr_sec_or_dummy;
			break;
		default:
			break;
		}

		if (!G.dev_list && !is_partition(sd.dname)) {
			/* User didn't specify device */
			if (!G.show_all && !sd.rd_ops && !sd.wr_ops) {
				/* Don't print unused device */
				continue;
			}
			print_stats_dev_struct(&G.saved_stats_dev[i], &sd, itv);
			G.saved_stats_dev[i] = sd;
			i++;
		} else {
			/* Is device in device list? */
			if (llist_find_str(G.dev_list, sd.dname)) {
				/* Print current statistics */
				print_stats_dev_struct(&G.saved_stats_dev[i], &sd, itv);
				G.saved_stats_dev[i] = sd;
				i++;
			} else
				continue;
		}
	}

	fclose(fp);
}

static void dev_report(cputime_t itv)
{
	/* Always print a header */
	print_devstat_header();

	/* Fetch current disk statistics */
	do_disk_statistics(itv);
}

static unsigned get_number_of_devices(void)
{
	FILE *fp;
	char buf[128];
	int rv;
	unsigned n = 0;
	unsigned long rd_ops, wr_ops;
	char dname[MAX_DEVICE_NAME];

	fp = xfopen_for_read("/proc/diskstats");

	while (fgets(buf, sizeof(buf), fp)) {
		rv = sscanf(buf, "%*d %*d %s %lu %*u %*u %*u %lu",
				dname, &rd_ops, &wr_ops);
		if (rv == 2 || is_partition(dname))
			/* A partition */
			continue;
		if (!rd_ops && !wr_ops) {
			/* Unused device */
			if (!G.show_all)
				continue;
		}
		n++;
	}

	fclose(fp);
	return n;
}

//usage:#define iostat_trivial_usage
//usage:       "[-c] [-d] [-t] [-z] [-k|-m] [ALL|BLOCKDEV...] [INTERVAL [COUNT]]"
//usage:#define iostat_full_usage "\n\n"
//usage:       "Report CPU and I/O statistics\n"
//usage:     "\nOptions:"
//usage:     "\n	-c	Show CPU utilization"
//usage:     "\n	-d	Show device utilization"
//usage:     "\n	-t	Print current time"
//usage:     "\n	-z	Omit devices with no activity"
//usage:     "\n	-k	Use kb/s"
//usage:     "\n	-m	Use Mb/s"

int iostat_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int iostat_main(int argc UNUSED_PARAM, char **argv)
{
	int opt, dev_num;
	unsigned interval;
	int count;
	stats_cpu_t stats_data[2];
	smallint current_stats;

	INIT_G();

	memset(&stats_data, 0, sizeof(stats_data));

	/* Get number of clock ticks per sec */
	G.clk_tck = get_user_hz();

	/* Determine number of CPUs */
	G.total_cpus = get_cpu_count();
	if (G.total_cpus == 0)
		G.total_cpus = 1;

	/* Parse and process arguments */
	/* -k and -m are mutually exclusive */
	opt_complementary = "k--m:m--k";
	opt = getopt32(argv, "cdtzkm");
	if (!(opt & (OPT_c + OPT_d)))
		/* Default is -cd */
		opt |= OPT_c + OPT_d;

	argv += optind;

	/* Store device names into device list */
	dev_num = 0;
	while (*argv && !isdigit(*argv[0])) {
		if (strcmp(*argv, "ALL") != 0) {
			/* If not ALL, save device name */
			char *dev_name = skip_dev_pfx(*argv);
			if (!llist_find_str(G.dev_list, dev_name)) {
				llist_add_to(&G.dev_list, dev_name);
				dev_num++;
			}
		} else {
			G.show_all = 1;
		}
		argv++;
	}

	interval = 0;
	count = 1;
	if (*argv) {
		/* Get interval */
		interval = xatoi_positive(*argv);
		count = (interval != 0 ? -1 : 1);
		argv++;
		if (*argv)
			/* Get count value */
			count = xatoi_positive(*argv);
	}

	/* Allocate space for device stats */
	if (opt & OPT_d) {
		G.saved_stats_dev = xzalloc(sizeof(G.saved_stats_dev[0]) *
				(dev_num ? dev_num : get_number_of_devices())
		);
	}

	if (opt & OPT_m) {
		G.unit.str = " MB";
		G.unit.div = 2048;
	}

	if (opt & OPT_k) {
		G.unit.str = " kB";
		G.unit.div = 2;
	}

	get_localtime(&G.tmtime);

	/* Display header */
	print_header();

	current_stats = 0;
	/* Main loop */
	for (;;) {
		stats_cpu_pair_t stats;

		stats.prev = &stats_data[current_stats ^ 1];
		stats.curr = &stats_data[current_stats];

		/* Fill the time structure */
		get_localtime(&G.tmtime);

		/* Fetch current CPU statistics */
		get_cpu_statistics(stats.curr);

		/* Get interval */
		stats.itv = get_interval(
			stats.prev->vector[GLOBAL_UPTIME],
			stats.curr->vector[GLOBAL_UPTIME]
		);

		if (opt & OPT_t)
			print_timestamp();

		if (opt & OPT_c) {
			cpu_report(&stats);
			if (opt & OPT_d)
				/* Separate outputs by a newline */
				bb_putchar('\n');
		}

		if (opt & OPT_d) {
			if (this_is_smp()) {
				stats.itv = get_interval(
					stats.prev->vector[SMP_UPTIME],
					stats.curr->vector[SMP_UPTIME]
				);
			}
			dev_report(stats.itv);
		}

		bb_putchar('\n');

		if (count > 0) {
			if (--count == 0)
				break;
		}

		/* Swap stats */
		current_stats ^= 1;

		sleep(interval);
	}

	if (ENABLE_FEATURE_CLEAN_UP) {
		llist_free(G.dev_list, NULL);
		free(G.saved_stats_dev);
		free(&G);
	}

	return EXIT_SUCCESS;
}
