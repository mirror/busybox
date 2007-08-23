/* vi: set sw=4 ts=4: */
/*
 * A tiny 'top' utility.
 *
 * This is written specifically for the linux /proc/<PID>/stat(m)
 * files format.

 * This reads the PIDs of all processes and their status and shows
 * the status of processes (first ones that fit to screen) at given
 * intervals.
 *
 * NOTES:
 * - At startup this changes to /proc, all the reads are then
 *   relative to that.
 *
 * (C) Eero Tamminen <oak at welho dot com>
 *
 * Rewritten by Vladimir Oleynik (C) 2002 <dzo@simtreas.ru>
 */

/* Original code Copyrights */
/*
 * Copyright (c) 1992 Branko Lankester
 * Copyright (c) 1992 Roger Binns
 * Copyright (C) 1994-1996 Charles L. Blake.
 * Copyright (C) 1992-1998 Michael K. Johnson
 * May be distributed under the conditions of the
 * GNU Library General Public License
 */

#include "libbb.h"


typedef struct top_status_t {
	unsigned long vsz;
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	unsigned long ticks;
	unsigned pcpu; /* delta of ticks */
#endif
	unsigned pid, ppid;
	unsigned uid;
	char state[4];
	char comm[COMM_LEN];
} top_status_t;

typedef struct jiffy_counts_t {
	unsigned long long usr,nic,sys,idle,iowait,irq,softirq,steal;
	unsigned long long total;
	unsigned long long busy;
} jiffy_counts_t;

/* This structure stores some critical information from one frame to
   the next. Used for finding deltas. */
typedef struct save_hist {
	unsigned long ticks;
	unsigned pid;
} save_hist;

typedef int (*cmp_funcp)(top_status_t *P, top_status_t *Q);

enum { SORT_DEPTH = 3 };

struct globals {
	top_status_t *top;
	int ntop;
#if ENABLE_FEATURE_USE_TERMIOS
	struct termios initial_settings;
#endif
#if !ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	cmp_funcp sort_function;
#else
	cmp_funcp sort_function[SORT_DEPTH];
	struct save_hist *prev_hist;
	int prev_hist_count;
	jiffy_counts_t jif, prev_jif;
	/* int hist_iterations; */
	unsigned total_pcpu;
	/* unsigned long total_vsz; */
#endif
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define top              (G.top               )
#define ntop             (G.ntop              )
#if ENABLE_FEATURE_USE_TERMIOS
#define initial_settings (G. initial_settings )
#endif
#define sort_function    (G.sort_function     )
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
#define prev_hist        (G.prev_hist         )
#define prev_hist_count  (G.prev_hist_count   )
#define jif              (G.jif               )
#define prev_jif         (G.prev_jif          )
#define total_pcpu       (G.total_pcpu        )
#endif

#define OPT_BATCH_MODE (option_mask32 & 0x4)


#if ENABLE_FEATURE_USE_TERMIOS
static int pid_sort(top_status_t *P, top_status_t *Q)
{
	/* Buggy wrt pids with high bit set */
	/* (linux pids are in [1..2^15-1]) */
	return (Q->pid - P->pid);
}
#endif

static int mem_sort(top_status_t *P, top_status_t *Q)
{
	/* We want to avoid unsigned->signed and truncation errors */
	if (Q->vsz < P->vsz) return -1;
	return Q->vsz != P->vsz; /* 0 if ==, 1 if > */
}


#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE

static int pcpu_sort(top_status_t *P, top_status_t *Q)
{
	/* Buggy wrt ticks with high bit set */
	/* Affects only processes for which ticks overflow */
	return (int)Q->pcpu - (int)P->pcpu;
}

static int time_sort(top_status_t *P, top_status_t *Q)
{
	/* We want to avoid unsigned->signed and truncation errors */
	if (Q->ticks < P->ticks) return -1;
	return Q->ticks != P->ticks; /* 0 if ==, 1 if > */
}

static int mult_lvl_cmp(void* a, void* b)
{
	int i, cmp_val;

	for (i = 0; i < SORT_DEPTH; i++) {
		cmp_val = (*sort_function[i])(a, b);
		if (cmp_val != 0)
			return cmp_val;
	}
	return 0;
}


static void get_jiffy_counts(void)
{
	FILE* fp = xfopen("stat", "r");
	prev_jif = jif;
	if (fscanf(fp, "cpu  %lld %lld %lld %lld %lld %lld %lld %lld",
			&jif.usr,&jif.nic,&jif.sys,&jif.idle,
			&jif.iowait,&jif.irq,&jif.softirq,&jif.steal) < 4) {
		bb_error_msg_and_die("failed to read /proc/stat");
	}
	fclose(fp);
	jif.total = jif.usr + jif.nic + jif.sys + jif.idle
			+ jif.iowait + jif.irq + jif.softirq + jif.steal;
	/* procps 2.x does not count iowait as busy time */
	jif.busy = jif.total - jif.idle - jif.iowait;
}


static void do_stats(void)
{
	top_status_t *cur;
	pid_t pid;
	int i, last_i, n;
	struct save_hist *new_hist;

	get_jiffy_counts();
	total_pcpu = 0;
	/* total_vsz = 0; */
	new_hist = xmalloc(sizeof(struct save_hist)*ntop);
	/*
	 * Make a pass through the data to get stats.
	 */
	/* hist_iterations = 0; */
	i = 0;
	for (n = 0; n < ntop; n++) {
		cur = top + n;

		/*
		 * Calculate time in cur process.  Time is sum of user time
		 * and system time
		 */
		pid = cur->pid;
		new_hist[n].ticks = cur->ticks;
		new_hist[n].pid = pid;

		/* find matching entry from previous pass */
		cur->pcpu = 0;
		/* do not start at index 0, continue at last used one
		 * (brought hist_iterations from ~14000 down to 172) */
		last_i = i;
		if (prev_hist_count) do {
			if (prev_hist[i].pid == pid) {
				cur->pcpu = cur->ticks - prev_hist[i].ticks;
				total_pcpu += cur->pcpu;
				break;
			}
			i = (i+1) % prev_hist_count;
			/* hist_iterations++; */
		} while (i != last_i);
		/* total_vsz += cur->vsz; */
	}

	/*
	 * Save cur frame's information.
	 */
	free(prev_hist);
	prev_hist = new_hist;
	prev_hist_count = ntop;
}
#endif /* FEATURE_TOP_CPU_USAGE_PERCENTAGE */

#if ENABLE_FEATURE_TOP_CPU_GLOBAL_PERCENTS && ENABLE_FEATURE_TOP_DECIMALS
/* formats 7 char string (8 with terminating NUL) */
static char *fmt_100percent_8(char pbuf[8], unsigned value, unsigned total)
{
	unsigned t;
	if (value >= total) { /* 100% ? */
		strcpy(pbuf, "  100% ");
		return pbuf;
	}
	/* else generate " [N/space]N.N% " string */
	value = 1000 * value / total;
	t = value / 100;
	value = value % 100;
	pbuf[0] = ' ';
	pbuf[1] = t ? t + '0' : ' ';
	pbuf[2] = '0' + (value / 10);
	pbuf[3] = '.';
	pbuf[4] = '0' + (value % 10);
	pbuf[5] = '%';
	pbuf[6] = ' ';
	pbuf[7] = '\0';
	return pbuf;
}
#endif

/* display generic info (meminfo / loadavg) */
static unsigned long display_generic(int scr_width)
{
	FILE *fp;
	char buf[80];
	char scrbuf[80];
	unsigned long total, used, mfree, shared, buffers, cached;
#if ENABLE_FEATURE_TOP_CPU_GLOBAL_PERCENTS
	unsigned total_diff;
#endif

	/* read memory info */
	fp = xfopen("meminfo", "r");

	/*
	 * Old kernels (such as 2.4.x) had a nice summary of memory info that
	 * we could parse, however this is gone entirely in 2.6. Try parsing
	 * the old way first, and if that fails, parse each field manually.
	 *
	 * First, we read in the first line. Old kernels will have bogus
	 * strings we don't care about, whereas new kernels will start right
	 * out with MemTotal:
	 *                              -- PFM.
	 */
	if (fscanf(fp, "MemTotal: %lu %s\n", &total, buf) != 2) {
		fgets(buf, sizeof(buf), fp);    /* skip first line */

		fscanf(fp, "Mem: %lu %lu %lu %lu %lu %lu",
			&total, &used, &mfree, &shared, &buffers, &cached);
		/* convert to kilobytes */
		used /= 1024;
		mfree /= 1024;
		shared /= 1024;
		buffers /= 1024;
		cached /= 1024;
		total /= 1024;
	} else {
		/*
		 * Revert to manual parsing, which incidentally already has the
		 * sizes in kilobytes. This should be safe for both 2.4 and
		 * 2.6.
		 */

		fscanf(fp, "MemFree: %lu %s\n", &mfree, buf);

		/*
		 * MemShared: is no longer present in 2.6. Report this as 0,
		 * to maintain consistent behavior with normal procps.
		 */
		if (fscanf(fp, "MemShared: %lu %s\n", &shared, buf) != 2)
			shared = 0;

		fscanf(fp, "Buffers: %lu %s\n", &buffers, buf);
		fscanf(fp, "Cached: %lu %s\n", &cached, buf);

		used = total - mfree;
	}
	fclose(fp);

	/* output memory info */
	if (scr_width > sizeof(scrbuf))
		scr_width = sizeof(scrbuf);
	snprintf(scrbuf, scr_width,
		"Mem: %luK used, %luK free, %luK shrd, %luK buff, %luK cached",
		used, mfree, shared, buffers, cached);
	/* clear screen & go to top */
	printf(OPT_BATCH_MODE ? "%s\n" : "\e[H\e[J%s\n", scrbuf);

#if ENABLE_FEATURE_TOP_CPU_GLOBAL_PERCENTS
	/*
	 * xxx% = (jif.xxx - prev_jif.xxx) / (jif.total - prev_jif.total) * 100%
	 */
	/* using (unsigned) casts to make operations cheaper */
	total_diff = ((unsigned)(jif.total - prev_jif.total) ? : 1);
#if ENABLE_FEATURE_TOP_DECIMALS
/* Generated code is approx +0.3k */
#define CALC_STAT(xxx) char xxx[8]
#define SHOW_STAT(xxx) fmt_100percent_8(xxx, (unsigned)(jif.xxx - prev_jif.xxx), total_diff)
#define FMT "%s"
#else
#define CALC_STAT(xxx) unsigned xxx = 100 * (unsigned)(jif.xxx - prev_jif.xxx) / total_diff
#define SHOW_STAT(xxx) xxx
#define FMT "%4u%% "
#endif
	{ /* need block: CALC_STAT are declarations */
		CALC_STAT(usr);
		CALC_STAT(sys);
		CALC_STAT(nic);
		CALC_STAT(idle);
		CALC_STAT(iowait);
		CALC_STAT(irq);
		CALC_STAT(softirq);
		//CALC_STAT(steal);

		snprintf(scrbuf, scr_width,
			/* Barely fits in 79 chars when in "decimals" mode. */
			"CPU:"FMT"usr"FMT"sys"FMT"nice"FMT"idle"FMT"io"FMT"irq"FMT"softirq",
			SHOW_STAT(usr), SHOW_STAT(sys), SHOW_STAT(nic), SHOW_STAT(idle),
			SHOW_STAT(iowait), SHOW_STAT(irq), SHOW_STAT(softirq)
			//, SHOW_STAT(steal) - what is this 'steal' thing?
			// I doubt anyone wants to know it
		);
	}
	puts(scrbuf);
#undef SHOW_STAT
#undef CALC_STAT
#undef FMT
#endif

	/* read load average as a string */
	buf[0] = '\0';
	open_read_close("loadavg", buf, sizeof("N.NN N.NN N.NN")-1);
	buf[sizeof("N.NN N.NN N.NN")-1] = '\0';
	snprintf(scrbuf, scr_width, "Load average: %s", buf);
	puts(scrbuf);

	return total;
}

/* display process statuses */
static void display_status(int count, int scr_width)
{
	enum {
		BITS_PER_INT = sizeof(int)*8
	};

	top_status_t *s = top;
	char vsz_str_buf[8];
	unsigned long total_memory = display_generic(scr_width); /* or use total_vsz? */
	/* xxx_shift and xxx_scale variables allow us to replace
	 * expensive divides with multiply and shift */
	unsigned pmem_shift, pmem_scale, pmem_half;
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	unsigned pcpu_shift, pcpu_scale, pcpu_half;
	unsigned busy_jifs;

	/* what info of the processes is shown */
	printf(OPT_BATCH_MODE ? "%.*s" : "\e[7m%.*s\e[0m", scr_width,
		"  PID  PPID USER     STAT   VSZ %MEM %CPU COMMAND");
#define MIN_WIDTH \
	sizeof( "  PID  PPID USER     STAT   VSZ %MEM %CPU C")
#else

	/* !CPU_USAGE_PERCENTAGE */
	printf(OPT_BATCH_MODE ? "%.*s" : "\e[7m%.*s\e[0m", scr_width,
		"  PID  PPID USER     STAT   VSZ %MEM COMMAND");
#define MIN_WIDTH \
	sizeof( "  PID  PPID USER     STAT   VSZ %MEM C")
#endif

#if ENABLE_FEATURE_TOP_DECIMALS
#define UPSCALE 1000
#define CALC_STAT(name, val) div_t name = div((val), 10)
#define SHOW_STAT(name) name.quot, '0'+name.rem
#define FMT "%3u.%c"
#else
#define UPSCALE 100
#define CALC_STAT(name, val) unsigned name = (val)
#define SHOW_STAT(name) name
#define FMT "%4u%%"
#endif
	/*
	 * MEM% = s->vsz/MemTotal
	 */
	pmem_shift = BITS_PER_INT-11;
	pmem_scale = UPSCALE*(1U<<(BITS_PER_INT-11)) / total_memory;
	/* s->vsz is in kb. we want (s->vsz * pmem_scale) to never overflow */
	while (pmem_scale >= 512) {
		pmem_scale /= 4;
		pmem_shift -= 2;
	}
	pmem_half = (1U << pmem_shift) / (ENABLE_FEATURE_TOP_DECIMALS? 20 : 2);
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	busy_jifs = jif.busy - prev_jif.busy;
	/* This happens if there were lots of short-lived processes
	 * between two top updates (e.g. compilation) */
	if (total_pcpu < busy_jifs) total_pcpu = busy_jifs;

	/*
	 * CPU% = s->pcpu/sum(s->pcpu) * busy_cpu_ticks/total_cpu_ticks
	 * (pcpu is delta of sys+user time between samples)
	 */
	/* (jif.xxx - prev_jif.xxx) and s->pcpu are
	 * in 0..~64000 range (HZ*update_interval).
	 * we assume that unsigned is at least 32-bit.
	 */
	pcpu_shift = 6;
	pcpu_scale = (UPSCALE*64*(uint16_t)busy_jifs ? : 1);
	while (pcpu_scale < (1U<<(BITS_PER_INT-2))) {
		pcpu_scale *= 4;
		pcpu_shift += 2;
	}
	pcpu_scale /= ( (uint16_t)(jif.total-prev_jif.total)*total_pcpu ? : 1);
	/* we want (s->pcpu * pcpu_scale) to never overflow */
	while (pcpu_scale >= 1024) {
		pcpu_scale /= 4;
		pcpu_shift -= 2;
	}
	pcpu_half = (1U << pcpu_shift) / (ENABLE_FEATURE_TOP_DECIMALS? 20 : 2);
	/* printf(" pmem_scale=%u pcpu_scale=%u ", pmem_scale, pcpu_scale); */
#endif

	/* Ok, all prelim data is ready, go thru the list */
	while (count-- > 0) {
		int col = scr_width;
		CALC_STAT(pmem, (s->vsz*pmem_scale + pmem_half) >> pmem_shift);
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
		CALC_STAT(pcpu, (s->pcpu*pcpu_scale + pcpu_half) >> pcpu_shift);
#endif

		if (s->vsz >= 100*1024)
			sprintf(vsz_str_buf, "%6ldM", s->vsz/1024);
		else
			sprintf(vsz_str_buf, "%7ld", s->vsz);
		// PID PPID USER STAT VSZ %MEM [%CPU] COMMAND
		col -= printf("\n" "%5u%6u %-8.8s %s%s" FMT
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
				FMT
#endif
				" ",
				s->pid, s->ppid, get_cached_username(s->uid),
				s->state, vsz_str_buf,
				SHOW_STAT(pmem)
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
				, SHOW_STAT(pcpu)
#endif
		);
		if (col > 0) {
			char buf[col + 1];
			read_cmdline(buf, col, s->pid, s->comm);
			fputs(buf, stdout);
		}
		/* printf(" %d/%d %lld/%lld", s->pcpu, total_pcpu,
			jif.busy - prev_jif.busy, jif.total - prev_jif.total); */
		s++;
	}
	/* printf(" %d", hist_iterations); */
	putchar(OPT_BATCH_MODE ? '\n' : '\r');
	fflush(stdout);
}
#undef UPSCALE
#undef SHOW_STAT
#undef CALC_STAT
#undef FMT


static void clearmems(void)
{
	clear_username_cache();
	free(top);
	top = 0;
	ntop = 0;
}


#if ENABLE_FEATURE_USE_TERMIOS
#include <termios.h>
#include <signal.h>

static void reset_term(void)
{
	tcsetattr(0, TCSANOW, (void *) &initial_settings);
#if ENABLE_FEATURE_CLEAN_UP
	clearmems();
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	free(prev_hist);
#endif
#endif /* FEATURE_CLEAN_UP */
}

static void sig_catcher(int sig ATTRIBUTE_UNUSED)
{
	reset_term();
	exit(1);
}
#endif /* FEATURE_USE_TERMIOS */


int top_main(int argc, char **argv);
int top_main(int argc, char **argv)
{
	int count, lines, col;
	unsigned interval = 5; /* default update rate is 5 seconds */
	unsigned iterations = UINT_MAX; /* 2^32 iterations by default :) */
	char *sinterval, *siterations;
#if ENABLE_FEATURE_USE_TERMIOS
	struct termios new_settings;
	struct timeval tv;
	fd_set readfds;
	unsigned char c;
#endif /* FEATURE_USE_TERMIOS */

	interval = 5;

	/* do normal option parsing */
	opt_complementary = "-";
	getopt32(argv, "d:n:b", &sinterval, &siterations);
	if (option_mask32 & 0x1) interval = xatou(sinterval); // -d
	if (option_mask32 & 0x2) iterations = xatou(siterations); // -n
	//if (option_mask32 & 0x4) // -b

	/* change to /proc */
	xchdir("/proc");
#if ENABLE_FEATURE_USE_TERMIOS
	tcgetattr(0, (void *) &initial_settings);
	memcpy(&new_settings, &initial_settings, sizeof(struct termios));
	/* unbuffered input, turn off echo */
	new_settings.c_lflag &= ~(ISIG | ICANON | ECHO | ECHONL);

	signal(SIGTERM, sig_catcher);
	signal(SIGINT, sig_catcher);
	tcsetattr(0, TCSANOW, (void *) &new_settings);
	atexit(reset_term);
#endif /* FEATURE_USE_TERMIOS */

#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	sort_function[0] = pcpu_sort;
	sort_function[1] = mem_sort;
	sort_function[2] = time_sort;
#else
	sort_function = mem_sort;
#endif /* FEATURE_TOP_CPU_USAGE_PERCENTAGE */

	while (1) {
		procps_status_t *p = NULL;

		/* Default to 25 lines - 5 lines for status */
		lines = 24 - 3 USE_FEATURE_TOP_CPU_GLOBAL_PERCENTS( - 1);
		col = 79;
#if ENABLE_FEATURE_USE_TERMIOS
		get_terminal_width_height(0, &col, &lines);
		if (lines < 5 || col < MIN_WIDTH) {
			sleep(interval);
			continue;
		}
		lines -= 3 USE_FEATURE_TOP_CPU_GLOBAL_PERCENTS( + 1);
#endif /* FEATURE_USE_TERMIOS */

		/* read process IDs & status for all the processes */
		while ((p = procps_scan(p, 0
				| PSSCAN_PID
				| PSSCAN_PPID
				| PSSCAN_VSZ
				| PSSCAN_STIME
				| PSSCAN_UTIME
				| PSSCAN_STATE
				| PSSCAN_COMM
				| PSSCAN_SID
				| PSSCAN_UIDGID
		))) {
			int n = ntop;
			top = xrealloc(top, (++ntop) * sizeof(*top));
			top[n].pid = p->pid;
			top[n].ppid = p->ppid;
			top[n].vsz = p->vsz;
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
			top[n].ticks = p->stime + p->utime;
#endif
			top[n].uid = p->uid;
			strcpy(top[n].state, p->state);
			strcpy(top[n].comm, p->comm);
		}
		if (ntop == 0) {
			bb_error_msg_and_die("no process info in /proc");
		}
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
		if (!prev_hist_count) {
			do_stats();
			sleep(1);
			clearmems();
			continue;
		}
		do_stats();
/* TODO: we don't need to sort all 10000 processes, we need to find top 24! */
		qsort(top, ntop, sizeof(top_status_t), (void*)mult_lvl_cmp);
#else
		qsort(top, ntop, sizeof(top_status_t), (void*)sort_function);
#endif /* FEATURE_TOP_CPU_USAGE_PERCENTAGE */
		count = lines;
		if (OPT_BATCH_MODE || count > ntop) {
			count = ntop;
		}
		/* show status for each of the processes */
		display_status(count, col);
#if ENABLE_FEATURE_USE_TERMIOS
		tv.tv_sec = interval;
		tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		select(1, &readfds, NULL, NULL, &tv);
		if (FD_ISSET(0, &readfds)) {
			if (read(0, &c, 1) <= 0) {   /* signal */
				return EXIT_FAILURE;
			}
			if (c == 'q' || c == initial_settings.c_cc[VINTR])
				break;
			if (c == 'M') {
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
				sort_function[0] = mem_sort;
				sort_function[1] = pcpu_sort;
				sort_function[2] = time_sort;
#else
				sort_function = mem_sort;
#endif
			}
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
			if (c == 'P') {
				sort_function[0] = pcpu_sort;
				sort_function[1] = mem_sort;
				sort_function[2] = time_sort;
			}
			if (c == 'T') {
				sort_function[0] = time_sort;
				sort_function[1] = mem_sort;
				sort_function[2] = pcpu_sort;
			}
#endif
			if (c == 'N') {
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
				sort_function[0] = pid_sort;
#else
				sort_function = pid_sort;
#endif
			}
		}
		if (!--iterations)
			break;
#else
		sleep(interval);
#endif /* FEATURE_USE_TERMIOS */
		clearmems();
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		clearmems();
	putchar('\n');
	return EXIT_SUCCESS;
}
