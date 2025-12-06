/* vi: set sw=4 ts=4: */
/*
 * A tiny 'top' utility.
 *
 * This is written specifically for the linux /proc/<PID>/stat(m)
 * files format.
 *
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
 *
 * Sept 2008: Vineet Gupta <vineet.gupta@arc.com>
 * Added Support for reporting SMP Information
 * - CPU where process was last seen running
 *   (to see effect of sched_setaffinity() etc)
 * - CPU time split (idle/IO/wait etc) per CPU
 *
 * Copyright (c) 1992 Branko Lankester
 * Copyright (c) 1992 Roger Binns
 * Copyright (C) 1994-1996 Charles L. Blake.
 * Copyright (C) 1992-1998 Michael K. Johnson
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
/* How to snapshot /proc for debugging top problems:
 * for f in /proc/[0-9]*""/stat; do
 *         n=${f#/proc/}
 *         n=${n%/stat}_stat
 *         cp $f $n
 * done
 * cp /proc/stat /proc/meminfo /proc/loadavg .
 * top -bn1 >top.out
 *
 * ...and how to run top on it on another machine:
 * rm -rf proc; mkdir proc
 * for f in [0-9]*_stat; do
 *         p=${f%_stat}
 *         mkdir -p proc/$p
 *         cp $f proc/$p/stat
 * done
 * cp stat meminfo loadavg proc
 * chroot . ./top -bn1 >top1.out
 */
//config:config TOP
//config:	bool "top (18 kb)"
//config:	default y
//config:	help
//config:	The top program provides a dynamic real-time view of a running
//config:	system.
//config:
//config:config FEATURE_TOP_INTERACTIVE
//config:	bool "Accept keyboard commands"
//config:	default y
//config:	depends on TOP
//config:	help
//config:	Without this, top will only refresh display every 5 seconds.
//config:	No keyboard commands will work, only ^C to terminate.
//config:
//config:config FEATURE_TOP_CPU_USAGE_PERCENTAGE
//config:	bool "Show CPU per-process usage percentage"
//config:	default y
//config:	depends on TOP
//config:	help
//config:	Make top display CPU usage for each process.
//config:	This adds about 2k.
//config:
//config:config FEATURE_TOP_CPU_GLOBAL_PERCENTS
//config:	bool "Show CPU global usage percentage"
//config:	default y
//config:	depends on FEATURE_TOP_CPU_USAGE_PERCENTAGE
//config:	help
//config:	Makes top display "CPU: NN% usr NN% sys..." line.
//config:	This adds about 0.5k.
//config:
//config:config FEATURE_TOP_SMP_CPU
//config:	bool "SMP CPU usage display ('c' key)"
//config:	default y
//config:	depends on FEATURE_TOP_CPU_GLOBAL_PERCENTS
//config:	help
//config:	Allow 'c' key to switch between individual/cumulative CPU stats
//config:	This adds about 0.5k.
//config:
//config:config FEATURE_TOP_DECIMALS
//config:	bool "Show 1/10th of a percent in CPU/mem statistics"
//config:	default y
//config:	depends on FEATURE_TOP_CPU_USAGE_PERCENTAGE
//config:	help
//config:	Show 1/10th of a percent in CPU/mem statistics.
//config:	This adds about 0.3k.
//config:
//config:config FEATURE_TOP_SMP_PROCESS
//config:	bool "Show CPU process runs on ('j' field)"
//config:	default y
//config:	depends on TOP
//config:	help
//config:	Show CPU where process was last found running on.
//config:	This is the 'j' field.
//config:
//config:config FEATURE_TOPMEM
//config:	bool "Topmem command ('s' key)"
//config:	default y
//config:	depends on TOP
//config:	help
//config:	Enable 's' in top (gives lots of memory info).

//applet:IF_TOP(APPLET(top, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TOP) += top.o

#include "libbb.h"

#define ESC     "\033"
#define HOME    ESC"[H"
#define CLREOS  ESC"[J"
#define CLREOL  ESC"[K"
#define REVERSE ESC"[7m"
#define NORMAL  ESC"[m"

typedef struct top_status_t {
	unsigned long memsize;
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	unsigned long ticks;
	unsigned pcpu; /* delta of ticks */
#endif
	unsigned pid, ppid;
	unsigned uid;
	char state[4];
	char comm[COMM_LEN];
#if ENABLE_FEATURE_TOP_SMP_PROCESS
	int last_seen_on_cpu;
#endif
} top_status_t;

typedef struct jiffy_counts_t {
	/* Linux 2.4.x has only first four */
	unsigned long long usr, nic, sys, idle;
	unsigned long long iowait, irq, softirq, steal;
	unsigned long long total;
	unsigned long long busy;
} jiffy_counts_t;

/* This structure stores some critical information from one frame to
   the next. Used for finding deltas. */
typedef struct save_hist {
	unsigned long ticks;
	pid_t pid;
} save_hist;

typedef int (*cmp_funcp)(top_status_t *P, top_status_t *Q);


enum { SORT_DEPTH = 3 };

/* Screens wider than this are unlikely */
enum { LINE_BUF_SIZE = 512 - 64 };

struct globals {
	top_status_t *top;
	int ntop;
	smallint inverted;
	smallint first_line_printed;
#if ENABLE_FEATURE_TOPMEM
	smallint sort_field;
#endif
#if ENABLE_FEATURE_TOP_SMP_CPU
	smallint smp_cpu_info; /* one/many cpu info lines? */
#endif
	int lines_remaining;
	unsigned lines;     /* screen height */
	unsigned scr_width; /* width, clamped <= LINE_BUF_SIZE-2 */
#if ENABLE_FEATURE_TOP_INTERACTIVE
	struct termios initial_settings;
	int scroll_ofs;
#define G_scroll_ofs G.scroll_ofs
#else
#define G_scroll_ofs 0
#endif
#if !ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	cmp_funcp sort_function[1];
#else
	cmp_funcp sort_function[SORT_DEPTH];
	struct save_hist *prev_hist;
	unsigned prev_hist_count;
	jiffy_counts_t cur_jif, prev_jif;
	/* int hist_iterations; */
	unsigned total_pcpu;
	/* unsigned long total_vsz; */
#endif
#if ENABLE_FEATURE_TOP_SMP_CPU
	/* Per CPU samples: current and last */
	jiffy_counts_t *cpu_jif, *cpu_prev_jif;
	unsigned num_cpus;
#endif
#if ENABLE_FEATURE_TOP_INTERACTIVE
	char kbd_input[KEYCODE_BUFFER_SIZE];
#endif
	char line_buf[LINE_BUF_SIZE];
};
#define G (*ptr_to_globals)
#define top              (G.top               )
#define ntop             (G.ntop              )
#define sort_field       (G.sort_field        )
#define inverted         (G.inverted          )
#define smp_cpu_info     (G.smp_cpu_info      )
#define initial_settings (G.initial_settings  )
#define sort_function    (G.sort_function     )
#define prev_hist        (G.prev_hist         )
#define prev_hist_count  (G.prev_hist_count   )
#define cur_jif          (G.cur_jif           )
#define prev_jif         (G.prev_jif          )
#define cpu_jif          (G.cpu_jif           )
#define cpu_prev_jif     (G.cpu_prev_jif      )
#define num_cpus         (G.num_cpus          )
#define total_pcpu       (G.total_pcpu        )
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	BUILD_BUG_ON(LINE_BUF_SIZE <= 80); \
} while (0)

enum {
	OPT_d = (1 << 0),
	OPT_n = (1 << 1),
	OPT_b = (1 << 2),
	OPT_H = (1 << 3),
	OPT_m = (1 << 4),
	OPT_EOF = (1 << 5), /* pseudo: "we saw EOF in stdin" */
};
#define OPT_BATCH_MODE (option_mask32 & OPT_b)


#if ENABLE_FEATURE_TOP_INTERACTIVE
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
	if (Q->memsize < P->memsize) return -1;
	return Q->memsize != P->memsize; /* 0 if ==, 1 if > */
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
			break;
	}
	return inverted ? -cmp_val : cmp_val;
}

static NOINLINE int read_cpu_jiffy(FILE *fp, jiffy_counts_t *p_jif)
{
#if !ENABLE_FEATURE_TOP_SMP_CPU
	static const char fmt[] ALIGN1 = "cpu %llu %llu %llu %llu %llu %llu %llu %llu";
#else
	static const char fmt[] ALIGN1 = "cp%*s %llu %llu %llu %llu %llu %llu %llu %llu";
#endif
	int ret;

	if (!fgets(G.line_buf, LINE_BUF_SIZE, fp) || G.line_buf[0] != 'c' /* not "cpu" */)
		return 0;
	ret = sscanf(G.line_buf, fmt,
			&p_jif->usr, &p_jif->nic, &p_jif->sys, &p_jif->idle,
			&p_jif->iowait, &p_jif->irq, &p_jif->softirq,
			&p_jif->steal);
	if (ret >= 4) {
		p_jif->total = p_jif->usr + p_jif->nic + p_jif->sys + p_jif->idle
			+ p_jif->iowait + p_jif->irq + p_jif->softirq + p_jif->steal;
		/* procps 2.x does not count iowait as busy time */
		p_jif->busy = p_jif->total - p_jif->idle - p_jif->iowait;
	}

	return ret;
}

static void get_jiffy_counts(void)
{
	FILE* fp = xfopen_for_read("stat");

	/* We need to parse cumulative counts even if SMP CPU display is on,
	 * they are used to calculate per process CPU% */
	prev_jif = cur_jif;
	if (read_cpu_jiffy(fp, &cur_jif) < 4)
		bb_error_msg_and_die("can't read '%s'", "/proc/stat");

#if !ENABLE_FEATURE_TOP_SMP_CPU
	fclose(fp);
	return;
#else
	if (!smp_cpu_info) {
		fclose(fp);
		return;
	}

	if (!num_cpus) {
		/* First time here. How many CPUs?
		 * There will be at least 1 /proc/stat line with cpu%d
		 */
		while (1) {
			cpu_jif = xrealloc_vector(cpu_jif, 1, num_cpus);
			if (read_cpu_jiffy(fp, &cpu_jif[num_cpus]) <= 4)
				break;
			num_cpus++;
		}
		if (num_cpus == 0) /* /proc/stat with only "cpu ..." line?! */
			smp_cpu_info = 0;

		cpu_prev_jif = xzalloc(sizeof(cpu_prev_jif[0]) * num_cpus);

		/* Otherwise the first per cpu display shows all 100% idles */
		usleep(50000);
	} else { /* Non first time invocation */
		jiffy_counts_t *tmp;
		int i;

		/* First switch the sample pointers: no need to copy */
		tmp = cpu_prev_jif;
		cpu_prev_jif = cpu_jif;
		cpu_jif = tmp;

		/* Get the new samples */
		for (i = 0; i < num_cpus; i++)
			read_cpu_jiffy(fp, &cpu_jif[i]);
	}
#endif
	fclose(fp);
}

static void do_stats(void)
{
	top_status_t *cur;
	pid_t pid;
	int n;
	unsigned i, last_i;
	struct save_hist *new_hist;

	get_jiffy_counts();
	total_pcpu = 0;
	/* total_memsize = 0; */
	new_hist = xmalloc(sizeof(new_hist[0]) * ntop);
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
		/* total_memsize += cur->memsize; */
	}

	/*
	 * Save cur frame's information.
	 */
	free(prev_hist);
	prev_hist = new_hist;
	prev_hist_count = ntop;
}

#endif /* FEATURE_TOP_CPU_USAGE_PERCENTAGE */

static void print_line_buf(void)
{
	const char *fmt;

	G.lines_remaining--;
	fmt = OPT_BATCH_MODE ? "\n""%.*s" : "\n""%.*s"CLREOL;
	if (!G.first_line_printed) {
		G.first_line_printed = 1;
		/* Go to top */
		fmt = OPT_BATCH_MODE ? "%.*s" : HOME"%.*s"CLREOL;
	}
	printf(fmt, G.scr_width - 1, G.line_buf);
}

static void print_line_bold(void)
{
	G.lines_remaining--;
//we never print first line in bold
//	if (!G.first_line_printed) {
//		printf(OPT_BATCH_MODE ? "%.*s" : HOME"%.*s"CLREOL, G.scr_width - 1, G.line_buf);
//		G.first_line_printed = 1;
//	} else {
		printf(OPT_BATCH_MODE ? "\n""%.*s" : "\n"REVERSE"%.*s"NORMAL CLREOL, G.scr_width - 1, G.line_buf);
//	}
}

static void print_end(void)
{
	fputs_stdout(OPT_BATCH_MODE ? "\n" : CLREOS"\r");
	/* next print will be "first line" (will clear the screen) */
	G.first_line_printed = 0;
}

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

#if ENABLE_FEATURE_TOP_CPU_GLOBAL_PERCENTS
static void display_cpus(void)
{
	/*
	 * xxx% = (cur_jif.xxx - prev_jif.xxx) / (cur_jif.total - prev_jif.total) * 100%
	 */
	unsigned total_diff;
	jiffy_counts_t *p_jif, *p_prev_jif;
	int i;
# if ENABLE_FEATURE_TOP_SMP_CPU
	int n_cpu_lines;
# endif

	/* using (unsigned) casts to make operations cheaper */
# define  CALC_TOTAL_DIFF do { \
	total_diff = (unsigned)(p_jif->total - p_prev_jif->total); \
	if (total_diff == 0) total_diff = 1; \
} while (0)

# if ENABLE_FEATURE_TOP_DECIMALS
#  define CALC_STAT(xxx) char xxx[8]
#  define SHOW_STAT(xxx) fmt_100percent_8(xxx, (unsigned)(p_jif->xxx - p_prev_jif->xxx), total_diff)
#  define FMT "%s"
# else
#  define CALC_STAT(xxx) unsigned xxx = 100 * (unsigned)(p_jif->xxx - p_prev_jif->xxx) / total_diff
#  define SHOW_STAT(xxx) xxx
#  define FMT "%4u%% "
# endif

# if !ENABLE_FEATURE_TOP_SMP_CPU
	{
		i = 1;
		p_jif = &cur_jif;
		p_prev_jif = &prev_jif;
# else
	/* Loop thru CPU(s) */
	n_cpu_lines = smp_cpu_info ? num_cpus : 1;
	if (n_cpu_lines > G.lines_remaining)
		n_cpu_lines = G.lines_remaining;

	for (i = 0; i < n_cpu_lines; i++) {
		p_jif = &cpu_jif[i];
		p_prev_jif = &cpu_prev_jif[i];
# endif
		CALC_TOTAL_DIFF;

		{ /* Need a block: CALC_STAT are declarations */
			CALC_STAT(usr);
			CALC_STAT(sys);
			CALC_STAT(nic);
			CALC_STAT(idle);
			CALC_STAT(iowait);
			CALC_STAT(irq);
			CALC_STAT(softirq);
			/*CALC_STAT(steal);*/

			sprintf(G.line_buf,
				/* Barely fits in 79 chars when in "decimals" mode. */
# if ENABLE_FEATURE_TOP_SMP_CPU
				"CPU%s:"FMT"usr"FMT"sys"FMT"nic"FMT"idle"FMT"io"FMT"irq"FMT"sirq",
				(smp_cpu_info ? utoa(i) : ""),
# else
				"CPU:"FMT"usr"FMT"sys"FMT"nic"FMT"idle"FMT"io"FMT"irq"FMT"sirq",
# endif
				SHOW_STAT(usr), SHOW_STAT(sys), SHOW_STAT(nic), SHOW_STAT(idle),
				SHOW_STAT(iowait), SHOW_STAT(irq), SHOW_STAT(softirq)
				/*, SHOW_STAT(steal) - what is this 'steal' thing? */
				/* I doubt anyone wants to know it */
			);
			print_line_buf();
		}
	}
# undef SHOW_STAT
# undef CALC_STAT
# undef FMT
}
#else  /* !ENABLE_FEATURE_TOP_CPU_GLOBAL_PERCENTS */
# define display_cpus() ((void)0)
#endif

enum {
	MI_MEMTOTAL,
	MI_MEMFREE,
	MI_MEMSHARED,
	MI_SHMEM,
	MI_BUFFERS,
	MI_CACHED,
	MI_SWAPTOTAL,
	MI_SWAPFREE,
	MI_DIRTY,
	MI_WRITEBACK,
	MI_ANONPAGES,
	MI_MAPPED,
	MI_SLAB,
	MI_MAX
};

static void parse_meminfo(unsigned long meminfo[MI_MAX])
{
	static const char fields[] ALIGN1 =
		"MemTotal\0"
		"MemFree\0"
		"MemShared\0"
		"Shmem\0"
		"Buffers\0"
		"Cached\0"
		"SwapTotal\0"
		"SwapFree\0"
		"Dirty\0"
		"Writeback\0"
		"AnonPages\0"
		"Mapped\0"
		"Slab\0";
	char buf[60]; /* actual lines we expect are ~30 chars or less */
	FILE *f;
	int i;

	memset(meminfo, 0, sizeof(meminfo[0]) * MI_MAX);
	f = xfopen_for_read("meminfo");
	while (fgets(buf, sizeof(buf), f) != NULL) {
		char *c = strchr(buf, ':');
		if (!c)
			continue;
		*c = '\0';
		i = index_in_strings(fields, buf);
		if (i >= 0)
			meminfo[i] = strtoul(c+1, NULL, 10);
	}
	fclose(f);
}

static void cmdline_to_line_buf_and_print(unsigned offset, unsigned pid, const char *comm)
{
	int width = G.scr_width - offset;
	if (width > 1) /* wider than to fit just the NUL? */
		read_cmdline(G.line_buf + offset, width, pid, comm);
//TODO: read_cmdline() sanitizes control chars, but not chars above 0x7e
	print_line_buf();
}

static unsigned long display_header(void)
{
	char *buf;
	unsigned long meminfo[MI_MAX];

	parse_meminfo(meminfo);

	/* Output memory info */
	sprintf(G.line_buf,
		"Mem: %luK used, %luK free, %luK shrd, %luK buff, %luK cached",
		meminfo[MI_MEMTOTAL] - meminfo[MI_MEMFREE],
		meminfo[MI_MEMFREE],
		meminfo[MI_MEMSHARED] + meminfo[MI_SHMEM],
		meminfo[MI_BUFFERS],
		meminfo[MI_CACHED]);
	print_line_buf();

	/* Display CPU time split as percentage of total time.
	 * This displays either a cumulative line or one line per CPU.
	 */
	display_cpus();

	/* Read load average as a string */
	buf = stpcpy(G.line_buf, "Load average: ");
	open_read_close("loadavg", buf, sizeof(G.line_buf) - sizeof("Load average: "));
	G.line_buf[sizeof(G.line_buf) - 1] = '\0'; /* paranoia */
	strchrnul(buf, '\n')[0] = '\0';
	print_line_buf();

	return meminfo[MI_MEMTOTAL];
}

static NOINLINE void display_process_list(void)
{
	enum {
		BITS_PER_INT = sizeof(int) * 8
	};

	top_status_t *s;
	unsigned long total_memory = display_header();
	/* xxx_shift and xxx_scale variables allow us to replace
	 * expensive divides with multiply and shift */
	unsigned pmem_shift, pmem_scale, pmem_half;
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	unsigned tmp_unsigned;
	unsigned pcpu_shift, pcpu_scale, pcpu_half;
	unsigned busy_jifs;
#endif

#if ENABLE_FEATURE_TOP_DECIMALS
# define UPSCALE 1000
	typedef struct { unsigned quot, rem; } bb_div_t;
/* Used to have "div_t name = div((val), 10)" here
 * (IOW: intended to use libc-compatible way to divide and use
 * both result and remainder, but musl does not inline div()...)
 * Oh well. Modern compilers detect "N/d, N%d" idiom by themselves:
 */
# define CALC_STAT(name, val) bb_div_t name = { (val) / 10, (val) % 10 }
# define SHOW_STAT(name) name.quot, '0'+name.rem
# define SANITIZE(name)  if (name.quot > 99) name.quot = 99, name.rem = (unsigned char)('+' - '0')
# define FMT "%3u.%c"
#else
# define UPSCALE 100
# define CALC_STAT(name, val) unsigned name = (val)
# define SANITIZE(name)  if (name > 99) name = 99
# define SHOW_STAT(name) name
# define FMT "%4u%%"
#endif

	strcpy(G.line_buf, "  PID  PPID USER     STAT   RSS %RSS"
		IF_FEATURE_TOP_SMP_PROCESS(" CPU")
		IF_FEATURE_TOP_CPU_USAGE_PERCENTAGE(" %CPU")
		" COMMAND");
	print_line_bold();

	/* %RSS = s->memsize / MemTotal * 100%
	 * Calculate this with multiply and shift. Example:
	 * shift = 12
	 * scale = 100 * 0x1000 / total_memory
	 * percent_mem = (size_mem * scale) >> shift
	 *            ~= (size_mem >> shift) * scale
	 *            ~= (size_mem >> shift) * 100 * (1 << shift) / total_memory
	 *            ~= size_mem * 100 / total_memory
	 */
	pmem_shift = BITS_PER_INT-11;
	pmem_scale = UPSCALE*(1U<<(BITS_PER_INT-11)) / total_memory;
	/* s->memsize is in kb. we want (s->memsize * pmem_scale) to never overflow */
	while (pmem_scale >= 512) {
		pmem_scale /= 4;
		pmem_shift -= 2;
	}
	pmem_half = (1U << pmem_shift) / (ENABLE_FEATURE_TOP_DECIMALS ? 20 : 2);
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	busy_jifs = cur_jif.busy - prev_jif.busy;
	/* This happens if there were lots of short-lived processes
	 * between two top updates (e.g. compilation) */
	if (total_pcpu < busy_jifs) total_pcpu = busy_jifs;

	/*
	 * CPU% = s->pcpu/sum(s->pcpu) * busy_cpu_ticks/total_cpu_ticks
	 * (pcpu is delta of sys+user time between samples)
	 */
	/* (cur_jif.xxx - prev_jif.xxx) and s->pcpu are
	 * in 0..~64000 range (HZ*update_interval).
	 * we assume that unsigned is at least 32-bit.
	 */
	pcpu_shift = 6;
	pcpu_scale = UPSCALE*64 * (uint16_t)busy_jifs;
	if (pcpu_scale == 0)
		pcpu_scale = 1;
	while (pcpu_scale < (1U << (BITS_PER_INT-2))) {
		pcpu_scale *= 4;
		pcpu_shift += 2;
	}
	tmp_unsigned = (uint16_t)(cur_jif.total - prev_jif.total) * total_pcpu;
	if (tmp_unsigned != 0)
		pcpu_scale /= tmp_unsigned;
	/* we want (s->pcpu * pcpu_scale) to never overflow */
	while (pcpu_scale >= 1024) {
		pcpu_scale /= 4;
		pcpu_shift -= 2;
	}
	pcpu_half = (1U << pcpu_shift) / (ENABLE_FEATURE_TOP_DECIMALS ? 20 : 2);
	/* printf(" pmem_scale=%u pcpu_scale=%u ", pmem_scale, pcpu_scale); */
#endif
	if (G.lines_remaining > ntop - G_scroll_ofs)
		G.lines_remaining = ntop - G_scroll_ofs;

	/* Ok, all preliminary data is ready, go through the list */
	s = top + G_scroll_ofs;
	while (G.lines_remaining > 0) {
		int n;
		char *ppu;
		char ppubuf[sizeof(int)*3 * 2 + 12];
		char memsize_str_buf[8];
		unsigned col;

		CALC_STAT(pmem, (s->memsize*pmem_scale + pmem_half) >> pmem_shift);
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
		CALC_STAT(pcpu, (s->pcpu*pcpu_scale + pcpu_half) >> pcpu_shift);
#endif
		/* VSZ can be much larger than total memory
		 * (seen values close to 2Tbyte), don't try to display
		 * "uses 12345.6% of MemTotal" (won't fit the column)
		 */
		SANITIZE(pmem);

		smart_ulltoa5(s->memsize, memsize_str_buf, " mgtpezy");
		/* PID PPID USER STAT VSZ %VSZ [%CPU] COMMAND */
		n = sprintf(ppubuf, "%5u %5u %-8.8s", s->pid, s->ppid, get_cached_username(s->uid));
		ppu = ppubuf;
		if (n != 6+6+8) {
			/* Format PID PPID USER part into 6+6+8 chars:
			 * shrink PID/PPID if possible, then truncate USER.
			 * Tested on Linux 5.18.0:
			 * sysctl kernel.pid_max=4194304 is the maximum allowed,
			 * so PID and PPID are 7 chars wide at most.
			 */
			char *p, *pp;
			if (*ppu == ' ') {
				do {
					ppu++, n--;
					if (n == 6+6+8)
						goto shortened;
				} while (*ppu == ' ');
			}
			pp = p = skip_non_whitespace(ppu) + 1;
			if (*p == ' ') {
				do
					p++, n--;
				while (n != 6+6+8 && *p == ' ');
				overlapping_strcpy(pp, p); /* shrink PPID */
			}
			ppu[6+6+8] = '\0'; /* truncate USER */
		}
 shortened:
		col = sprintf(G.line_buf,
				"%s %s  %.5s" FMT
				IF_FEATURE_TOP_SMP_PROCESS(" %3d")
				IF_FEATURE_TOP_CPU_USAGE_PERCENTAGE(FMT)
				" ",
				ppu,
				s->state, memsize_str_buf,
				SHOW_STAT(pmem)
				IF_FEATURE_TOP_SMP_PROCESS(, s->last_seen_on_cpu)
				IF_FEATURE_TOP_CPU_USAGE_PERCENTAGE(, SHOW_STAT(pcpu))
		);
		cmdline_to_line_buf_and_print(col, s->pid, s->comm);
		/* printf(" %d/%d %lld/%lld", s->pcpu, total_pcpu,
			cur_jif.busy - prev_jif.busy, cur_jif.total - prev_jif.total); */
		s++;
	}
	/* printf(" %d", hist_iterations); */
}
#undef UPSCALE
#undef SHOW_STAT
#undef CALC_STAT
#undef FMT

static void clearmems(void)
{
	clear_username_cache();
	free(top);
	top = NULL;
}

#if ENABLE_FEATURE_TOP_INTERACTIVE
static void reset_term(void)
{
	if (!OPT_BATCH_MODE)
		tcsetattr_stdin_TCSANOW(&initial_settings);
}

static void sig_catcher(int sig)
{
	reset_term();
	kill_myself_with_sig(sig);
}
#endif /* FEATURE_TOP_INTERACTIVE */

/*
 * TOPMEM support
 */

typedef unsigned long mem_t;

typedef struct topmem_status_t {
	unsigned pid;
	char comm[COMM_LEN];
	/* vsz doesn't count /dev/xxx mappings except /dev/zero */
	mem_t vsz     ;
	mem_t vszrw   ;
	mem_t rss     ;
	mem_t rss_sh  ;
	mem_t dirty   ;
	mem_t dirty_sh;
	mem_t stack   ;
} topmem_status_t;

enum { NUM_SORT_FIELD = 7 };

#define topmem ((topmem_status_t*)top)

#if ENABLE_FEATURE_TOPMEM

static int topmem_sort(char *a, char *b)
{
	int n;
	mem_t l, r;

	n = offsetof(topmem_status_t, vsz) + (sort_field * sizeof(mem_t));
	l = *(mem_t*)(a + n);
	r = *(mem_t*)(b + n);
	if (l == r) {
		l = ((topmem_status_t*)a)->dirty;
		r = ((topmem_status_t*)b)->dirty;
	}
	/* We want to avoid unsigned->signed and truncation errors */
	/* l>r: -1, l=r: 0, l<r: 1 */
	n = (l > r) ? -1 : (l != r);
	return inverted ? -n : n;
}

/* display header info (meminfo / loadavg) */
static void display_topmem_header(void)
{
	unsigned long meminfo[MI_MAX];

	parse_meminfo(meminfo);

	sprintf(G.line_buf,
		"Mem total:%lu anon:%lu map:%lu free:%lu",
		meminfo[MI_MEMTOTAL],
		meminfo[MI_ANONPAGES],
		meminfo[MI_MAPPED],
		meminfo[MI_MEMFREE]);
	print_line_buf();

	sprintf(G.line_buf,
		" slab:%lu buf:%lu cache:%lu dirty:%lu write:%lu",
		meminfo[MI_SLAB],
		meminfo[MI_BUFFERS],
		meminfo[MI_CACHED],
		meminfo[MI_DIRTY],
		meminfo[MI_WRITEBACK]);
	print_line_buf();

	sprintf(G.line_buf,
		"Swap total:%lu free:%lu", // TODO: % used?
		meminfo[MI_SWAPTOTAL],
		meminfo[MI_SWAPFREE]);
	print_line_buf();
}

/* see http://en.wikipedia.org/wiki/Tera */
static void ulltoa5_and_space(unsigned long long ul, char buf[6])
{
	smart_ulltoa5(ul, buf, " mgtpezy")[0] = ' ';
}
static void ulltoa4_and_space(unsigned long long ul, char buf[5])
{
	smart_ulltoa4(ul, buf, " mgtpezy")[0] = ' ';
}

static NOINLINE void display_topmem_process_list(void)
{
	const topmem_status_t *s = topmem + G_scroll_ofs;
	char *cp, ch;

	display_topmem_header();

	strcpy(G.line_buf, "  PID   VSZ VSZRW   RSS (SHR) DIRTY (SHR) STACK COMMAND");
	/* Mark the ^FIELD^ we sort by */
	cp = &G.line_buf[5 + sort_field * 6];
	ch = "^_"[inverted];
	cp[6] = ch;
	do *cp++ = ch; while (*cp == ' ');
	print_line_bold();

	if (G.lines_remaining > ntop - G_scroll_ofs)
		G.lines_remaining = ntop - G_scroll_ofs;
	while (G.lines_remaining > 0) {
		/* PID VSZ VSZRW RSS (SHR) DIRTY (SHR) COMMAND */
		int n = sprintf(G.line_buf, "%5u ", s->pid);
		if (n > 7) {
			/* PID is 7 chars long (up to 4194304) */
			ulltoa4_and_space(s->vsz  , &G.line_buf[8]);
			ulltoa4_and_space(s->vszrw, &G.line_buf[8+5]);
			/* the next field (RSS) starts at 8+10 = 3*6 */
		} else {
			if (n == 7) /* PID is 6 chars long */
				ulltoa4_and_space(s->vsz, &G.line_buf[7]);
				/* the next field (VSZRW) starts at 7+5 = 2*6 */
			else /* PID is 5 chars or less */
				ulltoa5_and_space(s->vsz, &G.line_buf[6]);
			ulltoa5_and_space(s->vszrw, &G.line_buf[2*6]);
		}
		ulltoa5_and_space(s->rss     , &G.line_buf[3*6]);
		ulltoa5_and_space(s->rss_sh  , &G.line_buf[4*6]);
		ulltoa5_and_space(s->dirty   , &G.line_buf[5*6]);
		ulltoa5_and_space(s->dirty_sh, &G.line_buf[6*6]);
		ulltoa5_and_space(s->stack   , &G.line_buf[7*6]);
		G.line_buf[8*6] = '\0';
		cmdline_to_line_buf_and_print(8*6, s->pid, s->comm);
		s++;
	}
}

#endif /* end TOPMEM support */

enum {
	TOP_MASK = 0
		| PSSCAN_PID
		| PSSCAN_PPID
		| PSSCAN_RSS
		| PSSCAN_STIME
		| PSSCAN_UTIME
		| PSSCAN_STATE
		| PSSCAN_COMM
		| PSSCAN_CPU
		| PSSCAN_UIDGID,
	TOPMEM_MASK = 0
		| PSSCAN_PID
		| PSSCAN_SMAPS
		| PSSCAN_COMM,
	EXIT_MASK = 0,
	ONLY_REDRAW = (unsigned)-1,
};

#if ENABLE_FEATURE_TOP_INTERACTIVE
static unsigned handle_input(unsigned scan_mask, duration_t interval)
{
	if (option_mask32 & OPT_EOF) {
		/* EOF on stdin ("top </dev/null") */
		sleep_for_duration(interval);
		return scan_mask;
	}

	while (1) {
		int32_t c, cc;

		c = safe_read_key(STDIN_FILENO, G.kbd_input, interval * 1000);
		if (c == -1) {
			if (errno != EAGAIN)
				/* error/EOF */
				option_mask32 |= OPT_EOF;
			/* else: timeout - rescan and refresh */
			break;
		}
		interval = 0;
		/* "continue" statements below return to do one additional
		 * quick attempt to read a key. This prevents
		 * long sequence of e.g. "nnnnnnnnnnnnnnnnnnnnnnnnnn"
		 * to cause lots of rescans.
		 */

		if (c == initial_settings.c_cc[VINTR])
			return EXIT_MASK;
		if (c == initial_settings.c_cc[VEOF])
			return EXIT_MASK;

		if (c == KEYCODE_UP) {
			G_scroll_ofs--;
			goto normalize_ofs;
		}
		if (c == KEYCODE_DOWN) {
			G_scroll_ofs++;
			goto normalize_ofs;
		}
		if (c == KEYCODE_HOME) {
			G_scroll_ofs = 0;
			goto normalize_ofs;
		}
		if (c == KEYCODE_END) {
			G_scroll_ofs = ntop - G.lines / 2;
			goto normalize_ofs;
		}
		if (c == KEYCODE_PAGEUP) {
			G_scroll_ofs -= G.lines / 2;
			goto normalize_ofs;
		}
		if (c == KEYCODE_PAGEDOWN) {
			G_scroll_ofs += G.lines / 2;
 normalize_ofs:
			if (G_scroll_ofs >= ntop)
				G_scroll_ofs = ntop - 1;
			if (G_scroll_ofs < 0)
				G_scroll_ofs = 0;
			return ONLY_REDRAW;
		}

		cc = c;
		c |= 0x20; /* lowercase */
		if (c == 'q')
			return EXIT_MASK;

		if (c == 'n') {
			IF_FEATURE_TOPMEM(scan_mask = TOP_MASK;)
			sort_function[0] = pid_sort;
			continue;
		}
		if (c == 'm') {
			IF_FEATURE_TOPMEM(scan_mask = TOP_MASK;)
			sort_function[0] = mem_sort;
# if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
			sort_function[1] = pcpu_sort;
			sort_function[2] = time_sort;
# endif
			continue;
		}
# if ENABLE_FEATURE_SHOW_THREADS
		if (c == 'h'
		IF_FEATURE_TOPMEM(&& scan_mask != TOPMEM_MASK)
		) {
			scan_mask ^= PSSCAN_TASKS;
#  if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
			free(prev_hist);
			prev_hist = NULL;
			prev_hist_count = 0;
#   endif
			continue;
		}
# endif
# if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
		if (c == 'p') {
			IF_FEATURE_TOPMEM(scan_mask = TOP_MASK;)
			sort_function[0] = pcpu_sort;
			sort_function[1] = mem_sort;
			sort_function[2] = time_sort;
			continue;
		}
		if (c == 't') {
			IF_FEATURE_TOPMEM(scan_mask = TOP_MASK;)
			sort_function[0] = time_sort;
			sort_function[1] = mem_sort;
			sort_function[2] = pcpu_sort;
			continue;
		}
#  if ENABLE_FEATURE_TOPMEM
		if (cc == 'S') {
			if (--sort_field < 0)
				sort_field = NUM_SORT_FIELD - 1;
			if (--sort_field < 0)
				sort_field = NUM_SORT_FIELD - 1;
		}
		if (c == 's') {
			sort_field = (sort_field + 1) % NUM_SORT_FIELD;
			if (scan_mask == TOPMEM_MASK)
				return ONLY_REDRAW;
			scan_mask = TOPMEM_MASK;
			free(prev_hist);
			prev_hist = NULL;
			prev_hist_count = 0;
			continue;
		}
#  endif
		if (c == 'r') {
			inverted ^= 1;
			return ONLY_REDRAW;
		}
#  if ENABLE_FEATURE_TOP_SMP_CPU
		/* procps-2.0.18 uses 'C', 3.2.7 uses '1' */
		if (c == 'c' || c == '1') {
			/* User wants to toggle per cpu <> aggregate */
			if (smp_cpu_info) {
				free(cpu_prev_jif);
				free(cpu_jif);
				cpu_jif = &cur_jif;
				cpu_prev_jif = &prev_jif;
			} else {
				/* Prepare for xrealloc() */
				cpu_jif = cpu_prev_jif = NULL;
			}
			num_cpus = 0;
			smp_cpu_info = !smp_cpu_info;
			get_jiffy_counts();
			continue;
		}
#  endif
# endif
		/* Unknown key. Eat remaining buffered input (if any) */
	} /* while (1) */

	return scan_mask;
}
#endif

//usage:#if ENABLE_FEATURE_SHOW_THREADS || ENABLE_FEATURE_TOP_SMP_CPU
//usage:# define IF_SHOW_THREADS_OR_TOP_SMP(...) __VA_ARGS__
//usage:#else
//usage:# define IF_SHOW_THREADS_OR_TOP_SMP(...)
//usage:#endif
//usage:#define top_trivial_usage
//usage:       "[-b"IF_FEATURE_TOPMEM("m")IF_FEATURE_SHOW_THREADS("H")"]"
//usage:       " [-n COUNT] [-d SECONDS]"
//usage:#define top_full_usage "\n\n"
//usage:       "Show a view of process activity in real time."
//usage:   "\n""Read the status of all processes from /proc each SECONDS"
//usage:   "\n""and show a screenful of them."
//usage:   "\n"
//usage:	IF_FEATURE_TOP_INTERACTIVE(
//usage:       "Keys:"
//usage:   "\n""	N/M"
//usage:                IF_FEATURE_TOP_CPU_USAGE_PERCENTAGE("/P")
//usage:                IF_FEATURE_TOP_CPU_USAGE_PERCENTAGE("/T")
//usage:           ": " IF_FEATURE_TOPMEM("show CPU usage, ") "sort by pid/mem"
//usage:                IF_FEATURE_TOP_CPU_USAGE_PERCENTAGE("/cpu")
//usage:                IF_FEATURE_TOP_CPU_USAGE_PERCENTAGE("/time")
//usage:	IF_FEATURE_TOPMEM(
//usage:   "\n""	S: show memory"
//usage:	)
//usage:   "\n""	R: reverse sort"
//usage:	IF_SHOW_THREADS_OR_TOP_SMP(
//usage:   "\n""	"
//usage:                IF_FEATURE_SHOW_THREADS("H: toggle threads")
//usage:                IF_FEATURE_SHOW_THREADS(IF_FEATURE_TOP_SMP_CPU(", "))
//usage:                IF_FEATURE_TOP_SMP_CPU("1: toggle SMP")
//usage:	)
//usage:   "\n""	Q,^C: exit"
//usage:   "\n""Options:"
//usage:	)
//usage:   "\n""	-b	Batch mode"
//usage:   "\n""	-n N	Exit after N iterations"
//usage:   "\n""	-d SEC	Delay between updates"
//usage:	IF_FEATURE_TOPMEM(
//usage:   "\n""	-m	Same as 's' key"
//usage:	)
//usage:	IF_FEATURE_SHOW_THREADS(
//usage:   "\n""	-H	Show threads"
//usage:	)

/* Interactive testing:
 * echo sss | ./busybox top
 * - shows memory screen
 * echo sss | ./busybox top -bn1 >mem
 * - saves memory screen - the *whole* list, not first NROWS processes!
 * echo .m.s.s.s.s.s.s.q | ./busybox top -b >z
 * - saves several different screens, and exits
 *
 * TODO: -i STRING param as a better alternative?
 */

int top_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int top_main(int argc UNUSED_PARAM, char **argv)
{
	duration_t interval;
	int iterations;
	unsigned opt;
	char *str_interval, *str_iterations;
	unsigned scan_mask = TOP_MASK;

	INIT_G();

//worth it?
//	setvbuf(stdout, /*buf*/ NULL, _IOFBF, /*size*/ 0);

	interval = 5; /* default update interval is 5 seconds */
	iterations = 0; /* infinite */
#if ENABLE_FEATURE_TOP_SMP_CPU
	/*num_cpus = 0;*/
	/*smp_cpu_info = 0;*/  /* to start with show aggregate */
	cpu_jif = &cur_jif;
	cpu_prev_jif = &prev_jif;
#endif

	/* all args are options; -n NUM */
	make_all_argv_opts(argv); /* options can be specified w/o dash */
	opt = getopt32(argv, "d:n:bHm", &str_interval, &str_iterations);
	/* NB: -m and -H are accepted even if not configured */
#if ENABLE_FEATURE_TOPMEM
	if (opt & OPT_m) /* -m (busybox specific) */
		scan_mask = TOPMEM_MASK;
#endif
	if (opt & OPT_d) {
		/* work around for "-d 1" -> "-d -1" done by make_all_argv_opts() */
		if (str_interval[0] == '-')
			str_interval++;
		interval = parse_duration_str(str_interval);
		/* Need to limit it to not overflow poll timeout */
		if (interval > INT_MAX / 1000)
			interval = INT_MAX / 1000;
	}
	if (opt & OPT_n) {
		if (str_iterations[0] == '-')
			str_iterations++;
		iterations = xatou(str_iterations);
	}
#if ENABLE_FEATURE_SHOW_THREADS
	if (opt & OPT_H) {
		scan_mask |= PSSCAN_TASKS;
	}
#endif

	xchdir("/proc");

#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
	sort_function[0] = pcpu_sort;
	sort_function[1] = mem_sort;
	sort_function[2] = time_sort;
#else
	sort_function[0] = mem_sort;
#endif

	if (OPT_BATCH_MODE) {
		option_mask32 |= OPT_EOF;
	}
#if ENABLE_FEATURE_TOP_INTERACTIVE
	else {
		/* Turn on unbuffered input; turn off echoing, ^C ^Z etc */
		set_termios_to_raw(STDIN_FILENO, &initial_settings, TERMIOS_CLEAR_ISIG);
		die_func = reset_term;
	}

	bb_signals(BB_FATAL_SIGS, sig_catcher);

	/* Eat initial input, if any */
	scan_mask = handle_input(scan_mask, 0);
#endif

	while (scan_mask != EXIT_MASK) {
		IF_FEATURE_TOP_INTERACTIVE(unsigned new_mask = scan_mask;)
		procps_status_t *p = NULL;

		G.lines = INT_MAX;
		G.scr_width = LINE_BUF_SIZE - 2; /* +2 bytes for '\n', NUL */
		if (!OPT_BATCH_MODE) {
			G.lines = 24; /* default */
			G.scr_width = 80;
			/* We output to stdout, we need size of stdout (not stdin)! */
			get_terminal_width_height(STDOUT_FILENO, &G.scr_width, &G.lines);
			if (G.lines < 5 || G.scr_width < 10) {
				sleep_for_duration(interval);
				continue;
			}
			if (G.scr_width > LINE_BUF_SIZE - 2)
				G.scr_width = LINE_BUF_SIZE - 2;
		}

		/* read process IDs & status for all the processes */
		ntop = 0;
		while ((p = procps_scan(p, scan_mask)) != NULL) {
			int n;

			IF_FEATURE_TOPMEM(if (scan_mask != TOPMEM_MASK)) {
				n = ntop;
				top = xrealloc_vector(top, 6, ntop++);
				top[n].pid = p->pid;
				top[n].ppid = p->ppid;
				top[n].memsize = p->rss;
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
				top[n].ticks = p->stime + p->utime;
#endif
				top[n].uid = p->uid;
				strcpy(top[n].state, p->state);
				strcpy(top[n].comm, p->comm);
#if ENABLE_FEATURE_TOP_SMP_PROCESS
				top[n].last_seen_on_cpu = p->last_seen_on_cpu;
#endif
			}
#if ENABLE_FEATURE_TOPMEM
			else { /* TOPMEM */
				if (!(p->mapped_ro | p->mapped_rw))
					continue; /* kernel threads are ignored */
				n = ntop;
				/* No bug here - top and topmem are the same */
				top = xrealloc_vector(topmem, 6, ntop++);
				strcpy(topmem[n].comm, p->comm);
				topmem[n].pid      = p->pid;
				topmem[n].vsz      = p->mapped_rw + p->mapped_ro;
				topmem[n].vszrw    = p->mapped_rw;
				topmem[n].rss_sh   = p->shared_clean + p->shared_dirty;
				topmem[n].rss      = p->private_clean + p->private_dirty + topmem[n].rss_sh;
				topmem[n].dirty    = p->private_dirty + p->shared_dirty;
				topmem[n].dirty_sh = p->shared_dirty;
				topmem[n].stack    = p->stack;
			}
#endif
		} /* end of "while we read /proc" */
		if (ntop == 0) {
			bb_simple_error_msg("no process info in /proc");
			break;
		}

		IF_FEATURE_TOPMEM(if (scan_mask != TOPMEM_MASK)) {
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
			if (!prev_hist_count) {
				do_stats();
				usleep(100000);
				clearmems();
				continue;
			}
			do_stats();
			/* TODO: we don't need to sort all 10000 processes, we need to find top 24! */
			qsort(top, ntop, sizeof(top_status_t), (void*)mult_lvl_cmp);
#else
			qsort(top, ntop, sizeof(top_status_t), (void*)(sort_function[0]));
#endif
		}
#if ENABLE_FEATURE_TOPMEM
		else { /* TOPMEM */
			qsort(topmem, ntop, sizeof(topmem_status_t), (void*)topmem_sort);
		}
#endif
 IF_FEATURE_TOP_INTERACTIVE(redraw:)
		G.lines_remaining = G.lines;
		IF_FEATURE_TOPMEM(if (scan_mask != TOPMEM_MASK)) {
			display_process_list();
		}
#if ENABLE_FEATURE_TOPMEM
		else { /* TOPMEM */
			display_topmem_process_list();
		}
#endif
		print_end();
		fflush_all();
		if (iterations >= 0 && !--iterations)
			break;
#if !ENABLE_FEATURE_TOP_INTERACTIVE
		clearmems();
		sleep_for_duration(interval);
#else
		new_mask = handle_input(scan_mask,
			/* After "redraw with no rescan", have one
			 * key timeout shorter that normal
			 * (IOW: rescan sooner):
			 */
			(new_mask == ONLY_REDRAW ? 1 : interval)
		);
		if (new_mask == ONLY_REDRAW)
			goto redraw;
		scan_mask = new_mask;
		clearmems();
#endif
	} /* end of "while (not Q)" */

	if (!OPT_BATCH_MODE)
		bb_putchar('\n');
#if ENABLE_FEATURE_TOP_INTERACTIVE
	reset_term();
#endif
	if (ENABLE_FEATURE_CLEAN_UP) {
		clearmems();
#if ENABLE_FEATURE_TOP_CPU_USAGE_PERCENTAGE
		free(prev_hist);
#endif
	}
	return EXIT_SUCCESS;
}
