/* vi: set sw=4 ts=4: */
/*
 * Report CPU and I/O stats, based on sysstat version 9.1.2 by Sebastien Godard
 *
 * Copyright (C) 2010 Marek Polacek <mmpolacek@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//applet:IF_IOSTAT(APPLET(iostat, _BB_DIR_BIN, _BB_SUID_DROP))

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
#define CURRENT          0
#define LAST             1

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

struct stats_cpu {
	cputime_t cpu_user;
	cputime_t cpu_nice;
	cputime_t cpu_system;
	cputime_t cpu_idle;
	cputime_t cpu_iowait;
	cputime_t cpu_steal;
	cputime_t cpu_irq;
	cputime_t cpu_softirq;
	cputime_t cpu_guest;
};

struct stats_dev {
	char dname[MAX_DEVICE_NAME];
	unsigned long long rd_sectors;
	unsigned long long wr_sectors;
	unsigned long rd_ops;
	unsigned long wr_ops;
};

/* List of devices entered on the command line */
struct device_list {
	char dname[MAX_DEVICE_NAME];
};

/* Globals. Sort by size and access frequency. */
struct globals {
	smallint show_all;
	unsigned devlist_i;             /* Index to the list of devices */
	unsigned total_cpus;            /* Number of CPUs */
	unsigned clk_tck;               /* Number of clock ticks per second */
	struct device_list *dlist;
	struct stats_dev *saved_stats_dev;
	struct tm tmtime;
};
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
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

	if (uname(&uts) < 0)
		bb_perror_msg_and_die("uname");

	strftime(buf, sizeof(buf), "%x", &G.tmtime);

	printf("%s %s (%s) \t%s \t_%s_\t(%d CPU)\n\n",
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
	strftime(buf, sizeof(buf), "%x %X", &G.tmtime);
	printf("%s\n", buf);
}

/* Fetch CPU statistics from /proc/stat */
static void get_cpu_statistics(struct stats_cpu *sc)
{
	FILE *fp;
	char buf[1024];

	fp = xfopen_for_read("/proc/stat");

	memset(sc, 0, sizeof(*sc));

	while (fgets(buf, sizeof(buf), fp)) {
		/* Does the line starts with "cpu "? */
		if (starts_with_cpu(buf) && buf[3] == ' ') {
			sscanf(buf + 4 + 1,
				"%"FMT_DATA"u %"FMT_DATA"u %"FMT_DATA"u %"FMT_DATA"u %"
				FMT_DATA"u %"FMT_DATA"u %"FMT_DATA"u %"FMT_DATA"u %"FMT_DATA"u",
				&sc->cpu_user, &sc->cpu_nice, &sc->cpu_system,
				&sc->cpu_idle, &sc->cpu_iowait, &sc->cpu_irq,
				&sc->cpu_softirq, &sc->cpu_steal, &sc->cpu_guest);
		}
	}

	fclose(fp);
}

static cputime_t get_smp_uptime(void)
{
	FILE *fp;
	char buf[sizeof(long)*3 * 2 + 4];
	unsigned long sec, dec;

	fp = xfopen_for_read("/proc/uptime");

	if (fgets(buf, sizeof(buf), fp))
		if (sscanf(buf, "%lu.%lu", &sec, &dec) != 2)
			bb_error_msg_and_die("can't read /proc/uptime");

	fclose(fp);

	return (cputime_t)sec * G.clk_tck + dec * G.clk_tck / 100;
}

/*
 * Obtain current uptime in jiffies.
 * Uptime is sum of individual CPUs' uptimes.
 */
static cputime_t get_uptime(const struct stats_cpu *sc)
{
	/* NB: Don't include cpu_guest, it is already in cpu_user */
	return sc->cpu_user + sc->cpu_nice + sc->cpu_system + sc->cpu_idle +
		+ sc->cpu_iowait + sc->cpu_irq + sc->cpu_steal + sc->cpu_softirq;
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

static void print_stats_cpu_struct(const struct stats_cpu *p,
		const struct stats_cpu *c, cputime_t itv)
{
	printf("         %6.2f  %6.2f  %6.2f  %6.2f  %6.2f  %6.2f\n",
		percent_value(p->cpu_user   , c->cpu_user   , itv),
		percent_value(p->cpu_nice   , c->cpu_nice   , itv),
		percent_value(p->cpu_system + p->cpu_softirq + p->cpu_irq,
			c->cpu_system + c->cpu_softirq + c->cpu_irq, itv),
		percent_value(p->cpu_iowait , c->cpu_iowait , itv),
		percent_value(p->cpu_steal  , c->cpu_steal  , itv),
		percent_value(p->cpu_idle   , c->cpu_idle   , itv)
	);
}

static void print_stats_dev_struct(const struct stats_dev *p,
		const struct stats_dev *c, cputime_t itv)
{
	int unit = 1;

	if (option_mask32 & OPT_k)
		unit = 2;
	else if (option_mask32 & OPT_m)
		unit = 2048;

	if (option_mask32 & OPT_z)
		if (p->rd_ops == c->rd_ops && p->wr_ops == c->wr_ops)
			return;

	printf("%-13s", c->dname);
	printf(" %8.2f %12.2f %12.2f %10llu %10llu \n",
		percent_value(p->rd_ops + p->wr_ops ,
		/**/		  c->rd_ops + c->wr_ops , itv),
		percent_value(p->rd_sectors, c->rd_sectors, itv) / unit,
		percent_value(p->wr_sectors, c->wr_sectors, itv) / unit,
		(c->rd_sectors - p->rd_sectors) / unit,
		(c->wr_sectors - p->wr_sectors) / unit);
}

static void cpu_report(const struct stats_cpu *last,
		const struct stats_cpu *cur,
		cputime_t itv)
{
	/* Always print a header */
	puts("avg-cpu:  %user   %nice %system %iowait  %steal   %idle");

	/* Print current statistics */
	print_stats_cpu_struct(last, cur, itv);
}

static void print_devstat_header(void)
{
	printf("Device:            tps");

	if (option_mask32 & OPT_m)
		puts("    MB_read/s    MB_wrtn/s    MB_read    MB_wrtn");
	else if (option_mask32 & OPT_k)
		puts("    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn");
	else
		puts("   Blk_read/s   Blk_wrtn/s   Blk_read   Blk_wrtn");
}

/*
 * Is input partition of format [sdaN]?
 */
static int is_partition(const char *dev)
{
	/* Ok, this is naive... */
	return ((dev[0] - 's') | (dev[1] - 'd') | (dev[2] - 'a')) == 0 && isdigit(dev[3]);
}

/*
 * Return number of numbers on cmdline.
 * Reasonable values are only 0 (no interval/count specified),
 * 1 (interval specified) and 2 (both interval and count specified)
 */
static int numbers_on_cmdline(int argc, char *argv[])
{
	int sum = 0;

	if (isdigit(argv[argc-1][0]))
		sum++;
	if (argc > 2 && isdigit(argv[argc-2][0]))
		sum++;

	return sum;
}

static int is_dev_in_dlist(const char *dev)
{
	int i;

	/* Go through the device list */
	for (i = 0; i < G.devlist_i; i++)
		if (strcmp(G.dlist[i].dname, dev) == 0)
			/* Found a match */
			return 1;

	/* No match found */
	return 0;
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

		if (!G.devlist_i && !is_partition(sd.dname)) {
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
			if (is_dev_in_dlist(sd.dname)) {
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

static void save_to_devlist(const char *dname)
{
	int i;
	struct device_list *tmp = G.dlist;

	if (strncmp(dname, "/dev/", 5) == 0)
		/* We'll ignore prefix '/dev/' */
		dname += 5;

	/* Go through the list */
	for (i = 0; i < G.devlist_i; i++, tmp++)
		if (strcmp(tmp->dname, dname) == 0)
			/* Already in the list */
			return;

	/* Add device name to the list */
	strncpy(tmp->dname, dname, MAX_DEVICE_NAME - 1);

	/* Update device list index */
	G.devlist_i++;
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

static int number_of_ALL_on_cmdline(char **argv)
{
	int alls = 0;

	/* Iterate over cmd line arguments, count "ALL" */
	while (*argv)
		if (strcmp(*argv++, "ALL") == 0)
			alls++;

	return alls;
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
int iostat_main(int argc, char **argv)
{
	int opt, dev_num;
	unsigned interval = 0;
	int count;
	cputime_t global_uptime[2] = { 0 };
	cputime_t smp_uptime[2] = { 0 };
	cputime_t itv;
	struct stats_cpu stats_cur, stats_last;

	INIT_G();

	memset(&stats_last, 0, sizeof(stats_last));

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
	argc -= optind;

	dev_num = argc - numbers_on_cmdline(argc, argv);
	/* We don't want to allocate space for 'ALL' */
	dev_num -= number_of_ALL_on_cmdline(argv);
	if (dev_num > 0)
		/* Make space for device list */
		G.dlist = xzalloc(sizeof(G.dlist[0]) * dev_num);

	/* Store device names into device list */
	while (*argv && !isdigit(*argv[0])) {
		if (strcmp(*argv, "ALL") != 0)
			/* If not ALL, save device name */
			save_to_devlist(*argv);
		else
			G.show_all = 1;
		argv++;
	}

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

	/* Display header */
	print_header();

	/* Main loop */
	for (;;) {
		/* Fill the time structure */
		get_localtime(&G.tmtime);

		/* Fetch current CPU statistics */
		get_cpu_statistics(&stats_cur);

		/* Fetch current uptime */
		global_uptime[CURRENT] = get_uptime(&stats_cur);

		/* Get interval */
		itv = get_interval(global_uptime[LAST], global_uptime[CURRENT]);

		if (opt & OPT_t)
			print_timestamp();

		if (opt & OPT_c) {
			cpu_report(&stats_last, &stats_cur, itv);
			if (opt & OPT_d)
				/* Separate outputs by a newline */
				bb_putchar('\n');
		}

		if (opt & OPT_d) {
			if (this_is_smp()) {
				smp_uptime[CURRENT] = get_smp_uptime();
				itv = get_interval(smp_uptime[LAST], smp_uptime[CURRENT]);
				smp_uptime[LAST] = smp_uptime[CURRENT];
			}
			dev_report(itv);
		}

		if (count > 0) {
			if (--count == 0)
				break;
		}

		/* Backup current stats */
		global_uptime[LAST] = global_uptime[CURRENT];
		stats_last = stats_cur;

		bb_putchar('\n');
		sleep(interval);
	}

	bb_putchar('\n');

	if (ENABLE_FEATURE_CLEAN_UP) {
		free(&G);
		free(G.dlist);
		free(G.saved_stats_dev);
	}

	return EXIT_SUCCESS;
}
