/* vi: set sw=4 ts=4: */
/*
 * A mini 'powertop' utility:
 *   Analyze power consumption on Intel-based laptops.
 * Based on powertop 1.11.
 *
 * Copyright (C) 2010 Marek Polacek <mmpolacek@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//applet:IF_POWERTOP(APPLET(powertop, _BB_DIR_BIN, _BB_SUID_DROP))

//kbuild:lib-$(CONFIG_POWERTOP) += powertop.o

//config:config POWERTOP
//config:	bool "powertop"
//config:	default y
//config:	help
//config:	  Analyze power consumption on Intel-based laptops

#include "libbb.h"

//#define debug(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)
#define debug(fmt, ...) ((void)0)

// XXX This should not be here
#define ENABLE_FEATURE_POWERTOP_PROCIRQ 1

#define DEFAULT_SLEEP      10
#define DEFAULT_SLEEP_STR "10"

/* Frequency of the ACPI timer */
#define FREQ_ACPI          3579.545
#define FREQ_ACPI_1000	   3579545

/* Max filename length of entry in /sys/devices subsystem */
#define BIG_SYSNAME_LEN    16

typedef unsigned long long ullong;

struct line {
	char *string;
	int count;
	int disk_count;
};

#if ENABLE_FEATURE_POWERTOP_PROCIRQ
#define IRQCOUNT		40

struct irqdata {
	int active;
	int number;
	ullong count;
	char irq_desc[32];
};
#endif

struct globals {
	bool timer_list_read;
	smallint nostats;
	int headline;
	int nlines;
	int linesize;
	int maxcstate;
#if ENABLE_FEATURE_POWERTOP_PROCIRQ
	int total_interrupt;
	int interrupt_0;
	int percpu_hpet_start;
	int percpu_hpet_end;
	struct irqdata interrupts[IRQCOUNT];
#endif
	unsigned total_cpus;
	ullong start_usage[8];
	ullong last_usage[8];
	ullong start_duration[8];
	ullong last_duration[8];
	char cstate_names[8][16];
	struct line *lines;
#if ENABLE_FEATURE_USE_TERMIOS
	struct termios init_settings;
#endif
};
#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)

#if ENABLE_FEATURE_USE_TERMIOS
static void reset_term(void)
{
	tcsetattr_stdin_TCSANOW(&G.init_settings);
}

static void sig_handler(int signo UNUSED_PARAM)
{
	reset_term();
	exit(EXIT_FAILURE);
}
#endif

static int write_str_to_file(const char *fname, const char *str)
{
	FILE *fp = fopen_for_write(fname);
	if (!fp)
		return 1;
	fputs(str, fp);
	fclose(fp);
	return 0;
}

/* Make it more readable */
#define start_timer()	write_str_to_file("/proc/timer_stats", "1\n")
#define stop_timer()	write_str_to_file("/proc/timer_stats", "0\n")

static void NOINLINE clear_lines(void)
{
	int i;

	for (i = 0; i < G.headline; i++)
		free(G.lines[i].string);
	free(G.lines);
	G.headline = G.linesize = 0;
	G.lines = NULL;
}

static void count_lines(void)
{
	int i;

	for (i = 0; i < G.headline; i++)
		G.nlines += G.lines[i].count;
}

static int line_compare(const void *p1, const void *p2)
{
	const struct line *a = p1;
	const struct line *b = p2;

	return (b->count + 50 * b->disk_count) - (a->count + 50 * a->disk_count);
}

static void do_sort(void)
{
	qsort(G.lines, G.headline, sizeof(struct line), line_compare);
}

/*
 * Save C-state names, usage and duration. Also get maxcstate.
 * Reads data from /proc.
 */
static void read_data(ullong *usage, ullong *duration)
{
	DIR *dir;
	struct dirent *d;

	dir = opendir("/proc/acpi/processor");
	if (!dir)
		return;

	while ((d = readdir(dir)) != NULL) {
		FILE *fp;
		char buf[192];
		int level = 0;
		int len;

		len = strlen(d->d_name);
		if (len < 3 || len > BIG_SYSNAME_LEN)
			continue;

		sprintf(buf, "/proc/acpi/processor/%s/power", d->d_name);
		fp = fopen_for_read(buf);
		if (!fp)
			continue;

		while (fgets(buf, sizeof(buf), fp)) {
			char *p;

			/* Get usage */
			p = strstr(buf, "age[");
			if (!p)
				continue;
			p += 4;
			usage[level] += bb_strtoull(p, NULL, 10) + 1;

			/* Get duration */
			p = strstr(buf, "ation[");
			if (!p)
				continue;
			p += 6;
			duration[level] += bb_strtoull(p, NULL, 10);

			/* Increment level */
			level++;

			/* Also update maxcstate */
			if (level > G.maxcstate)
				G.maxcstate = level;
		}
		fclose(fp);
	}
	closedir(dir);
}

/* Add line and/or update count */
static void push_line(const char *string, int count)
{
	int i;

	if (!string)
		return;

	/* Loop through entries */
	for (i = 0; i < G.headline; i++) {
		if (strcmp(string, G.lines[i].string) == 0) {
			/* It's already there, only update count */
			G.lines[i].count += count;
			return;
		}
	}

	G.lines = xrealloc_vector(G.lines, 1, G.headline);

	G.lines[G.headline].string = xstrdup(string);
	G.lines[G.headline].count = count;
	G.lines[G.headline].disk_count = 0;

	/* We added a line */
	G.headline++;
}

#if ENABLE_FEATURE_POWERTOP_PROCIRQ
static int percpu_hpet_timer(const char *name)
{
	char *p;
	long hpet_chan;

	/* This is done once */
	if (!G.timer_list_read) {
		FILE *fp;
		char buf[80];

		G.timer_list_read = true;
		fp = fopen_for_read("/proc/timer_list");
		if (!fp)
			return 0;

		while (fgets(buf, sizeof(buf), fp)) {
			p = strstr(buf, "Clock Event Device: hpet");
			if (!p)
				continue;
			p += sizeof("Clock Event Device: hpet")-1;
			if (!isdigit(p[0]))
				continue;
			hpet_chan = xatoi_positive(p);
			if (hpet_chan < G.percpu_hpet_start)
				G.percpu_hpet_start = hpet_chan;
			if (hpet_chan > G.percpu_hpet_end)
				G.percpu_hpet_end = hpet_chan;
		}
		fclose(fp);
	}

	p = strstr(name, "hpet");
	if (!p)
		return 0;

	p += 4;
	if (!isdigit(p[0]))
		return 0;

	hpet_chan = xatoi_positive(p);
	if (G.percpu_hpet_start <= hpet_chan && hpet_chan <= G.percpu_hpet_end)
		return 1;

	return 0;
}

static int update_irq(int irq, ullong count)
{
	int unused = IRQCOUNT;
	int i;

	for (i = 0; i < IRQCOUNT; i++) {
		if (G.interrupts[i].active && G.interrupts[i].number == irq) {
			ullong old;
			old = G.interrupts[i].count;
			G.interrupts[i].count = count;
			return count - old;
		}
		if (!G.interrupts[i].active && unused > i)
			unused = i;
	}

	G.interrupts[unused].active = 1;
	G.interrupts[unused].count = count;
	G.interrupts[unused].number = irq;

	return count;
}

/*
 * Read /proc/interrupts, save IRQ counts and IRQ description.
 */
static void do_proc_irq(void)
{
	FILE *fp;
	char buf[128];

	/* Reset values */
	G.interrupt_0 = 0;
	G.total_interrupt = 0;

	fp = xfopen_for_read("/proc/interrupts");
	while (fgets(buf, sizeof(buf), fp)) {
		char irq_desc[sizeof("   <kernel IPI> : ") + sizeof(buf)];
		char *p;
		const char *name;
		int nr = -1;
		ullong count;
		ullong delta;
		int special;

		/* Skip header */
		p = strchr(buf, ':');
		if (!p)
			continue;
		/*  0:  143646045  153901007   IO-APIC-edge      timer
		 *   ^
		 */
		/* Deal with non-maskable interrupts -- make up fake numbers */
		special = 0;
		if (buf[0] != ' ' && !isdigit(buf[0])) {
			if (strncmp(buf, "NMI:", 4) == 0)
				nr = 20000;
			if (strncmp(buf, "RES:", 4) == 0)
				nr = 20001;
			if (strncmp(buf, "CAL:", 4) == 0)
				nr = 20002;
			if (strncmp(buf, "TLB:", 4) == 0)
				nr = 20003;
			if (strncmp(buf, "TRM:", 4) == 0)
				nr = 20004;
			if (strncmp(buf, "THR:", 4) == 0)
				nr = 20005;
			if (strncmp(buf, "SPU:", 4) == 0)
				nr = 20006;
			special = 1;
		} else {
			/* bb_strtou don't eat leading spaces, using strtoul */
			nr = strtoul(buf, NULL, 10); /* xato*() wouldn't work */
		}
		if (nr == -1)
			continue;

		p++;
		/*  0:  143646045  153901007   IO-APIC-edge      timer
		 *    ^
		 */
		/* Count sum of the IRQs */
		count = 0;
		while (1) {
			char *tmp;
			p = skip_whitespace(p);
			if (!isdigit(*p))
				break;
			count += bb_strtoull(p, &tmp, 10);
			p = tmp;
		}
		/*   0:  143646045  153901007   IO-APIC-edge      timer
		 * NMI:          1          2   Non-maskable interrupts
		 *		                ^
		 */
		if (!special) {
			/* Skip to the interrupt name, e.g. 'timer' */
			p = strchr(p, ' ');
			if (!p)
				continue;
			p = skip_whitespace(p);
		}

		name = p;
		strchrnul(name, '\n')[0] = '\0';
		/* Save description of the interrupt */
		if (special)
			sprintf(irq_desc, "   <kernel IPI> : %s", name);
		else
			sprintf(irq_desc, "    <interrupt> : %s", name);

		delta = update_irq(nr, count);

		/* Skip per CPU timer interrupts */
		if (percpu_hpet_timer(name))
			delta = 0;
		if (nr > 0 && delta > 0)
			push_line(irq_desc, delta);
		if (!nr)
			G.interrupt_0 = delta;
		else
			G.total_interrupt += delta;
	}

	fclose(fp);
}
#endif /* ENABLE_FEATURE_POWERTOP_PROCIRQ */

#ifdef __i386__
/*
 * Get information about CPU using CPUID opcode.
 */
static void cpuid(unsigned int *eax, unsigned int *ebx, unsigned int *ecx,
				  unsigned int *edx)
{
	/* EAX value specifies what information to return */
	__asm__(
		"	pushl %%ebx\n"     /* Save EBX */
		"	cpuid\n"
		"	movl %%ebx, %1\n"  /* Save content of EBX */
		"	popl %%ebx\n"      /* Restore EBX */
		: "=a"(*eax), /* Output */
		  "=r"(*ebx),
		  "=c"(*ecx),
		  "=d"(*edx)
		: "0"(*eax),  /* Input */
		  "1"(*ebx),
		  "2"(*ecx),
		  "3"(*edx)
		/* No clobbered registers */
	);
}
#endif

static void NOINLINE print_intel_cstates(void)
{
#ifdef __i386__
	int bios_table[8] = { 0 };
	int nbios = 0;
	DIR *cpudir;
	struct dirent *d;
	int i;
	unsigned eax, ebx, ecx, edx;

	cpudir = opendir("/sys/devices/system/cpu");
	if (!cpudir)
		return;

	/* Loop over cpuN entries */
	while ((d = readdir(cpudir)) != NULL) {
		DIR *dir;
		int len;
		char fname[sizeof("/sys/devices/system/cpu//cpuidle//desc") + 2*BIG_SYSNAME_LEN];

		len = strlen(d->d_name);
		if (len < 3 || len > BIG_SYSNAME_LEN)
			continue;

		if (!isdigit(d->d_name[3]))
			continue;

		len = sprintf(fname, "/sys/devices/system/cpu/%s/cpuidle", d->d_name);
		dir = opendir(fname);
		if (!dir)
			continue;

		/*
		 * Every C-state has its own stateN directory, that
		 * contains a `time' and a `usage' file.
		 */
		while ((d = readdir(dir)) != NULL) {
			FILE *fp;
			char buf[64];
			int n;

			n = strlen(d->d_name);
			if (n < 3 || n > BIG_SYSNAME_LEN)
				continue;

			sprintf(fname + len, "/%s/desc", d->d_name);
			fp = fopen_for_read(fname);
			if (fp) {
				char *p = fgets(buf, sizeof(buf), fp);
				fclose(fp);
				if (!p)
					break;
				p = strstr(p, "MWAIT ");
				if (p) {
					int pos;
					p += sizeof("MWAIT ") - 1;
					pos = (bb_strtoull(p, NULL, 16) >> 4) + 1;
					if (pos >= ARRAY_SIZE(bios_table))
						continue;
					bios_table[pos]++;
					nbios++;
				}
			}
		}
		closedir(dir);
	}
	closedir(cpudir);

	if (!nbios)
		return;

	eax = 5;
	ebx = ecx = edx = 0;
	cpuid(&eax, &ebx, &ecx, &edx);
	if (!edx || !(ecx & 1))
		return;

	printf("Your CPU supports the following C-states: ");
	i = 0;
	while (edx) {
		if (edx & 7)
			printf("C%u ", i);
		edx >>= 4;
		i++;
	}
	bb_putchar('\n');

	/* Print BIOS C-States */
	printf("Your BIOS reports the following C-states: ");
	for (i = 0; i < 8; i++)
		if (bios_table[i])
			printf("C%u ", i);

	bb_putchar('\n');
#endif
}

static void print_header(void)
{
	printf(
		/* Clear the screen */
		"\033[H\033[J"
		/* Print the header */
		"\033[7m%.*s\033[0m", 79, "PowerTOP (C) 2007 Intel Corporation\n"
	);
}

static void show_cstates(char cstate_lines[][64])
{
	int i;

	for (i = 0; i < 10; i++)
		if ((cstate_lines[i][0]))
			printf("%s", cstate_lines[i]);
}

static void show_timerstats(int nostats)
{
	unsigned lines;

	/* Get terminal height */
	get_terminal_width_height(STDOUT_FILENO, NULL, &lines);

	/* We don't have whole terminal just for timerstats */
	lines -= 12;

	if (!nostats) {
		int i, n = 0;

		puts("\nTop causes for wakeups:");
		for (i = 0; i < G.headline; i++) {
			if ((G.lines[i].count > 0 || G.lines[i].disk_count > 0)
			 && n++ < lines
			) {
				char c = ' ';
				if (G.lines[i].disk_count)
					c = 'D';
				printf(" %5.1f%% (%5.1f)%c  %s\n",
						G.lines[i].count * 100.0 / G.nlines,
						G.lines[i].count * 1.0 / DEFAULT_SLEEP, c,
						G.lines[i].string);
			}
		}
	} else {
		bb_putchar('\n');
		bb_error_msg("no stats available; run as root or"
				" enable the cpufreq_stats module");
	}
}

//usage:#define powertop_trivial_usage
//usage:       ""
//usage:#define powertop_full_usage "\n\n"
//usage:       "Analyze power consumption on Intel-based laptops\n"

int powertop_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int powertop_main(int UNUSED_PARAM argc, char UNUSED_PARAM **argv)
{
	ullong cur_usage[8];
	ullong cur_duration[8];
	char cstate_lines[12][64];
	char buf[128];
#if ENABLE_FEATURE_USE_TERMIOS
	struct termios new_settings;
	struct pollfd pfd[1];

	pfd[0].fd = 0;
	pfd[0].events = POLLIN;
#endif

	INIT_G();

#if ENABLE_FEATURE_POWERTOP_PROCIRQ
	G.percpu_hpet_start = INT_MAX;
	G.percpu_hpet_end = INT_MIN;
#endif

	/* Print warning when we don't have superuser privileges */
	if (geteuid() != 0)
		bb_error_msg("run as root to collect enough information");

#if ENABLE_FEATURE_USE_TERMIOS
	/* So we don't forget to reset term settings */
	atexit(reset_term);
#endif

	/* Get number of CPUs */
	G.total_cpus = get_cpu_count();

	printf("Collecting data for "DEFAULT_SLEEP_STR" seconds\n");

#if ENABLE_FEATURE_USE_TERMIOS
	tcgetattr(0, (void *)&G.init_settings);
	memcpy(&new_settings, &G.init_settings, sizeof(new_settings));

	/* Turn on unbuffered input, turn off echoing */
	new_settings.c_lflag &= ~(ISIG | ICANON | ECHO | ECHONL);

	bb_signals(BB_FATAL_SIGS, sig_handler);
	tcsetattr_stdin_TCSANOW(&new_settings);
#endif

#if ENABLE_FEATURE_POWERTOP_PROCIRQ
	/* Collect initial data */
	do_proc_irq();
	do_proc_irq();
#endif

	/* Read initial usage and duration */
	read_data(&G.start_usage[0], &G.start_duration[0]);

	/* Copy them to "last" */
	memcpy(G.last_usage, G.start_usage, sizeof(G.last_usage));
	memcpy(G.last_duration, G.start_duration, sizeof(G.last_duration));

	/* Display C-states */
	print_intel_cstates();

	if (stop_timer())
		G.nostats = 1;

	/* The main loop */
	for (;;) {
		double maxsleep = 0.0;
		ullong totalticks, totalevents;
		int i;
		FILE *fp;
		double newticks;

		if (start_timer())
			G.nostats = 1;

#if !ENABLE_FEATURE_USE_TERMIOS
		sleep(DEFAULT_SLEEP);
#else
		if (safe_poll(pfd, 1, DEFAULT_SLEEP * 1000) > 0) {
			unsigned char c;
			if (safe_read(STDIN_FILENO, &c, 1) != 1)
				break; /* EOF/error */
			if (c == G.init_settings.c_cc[VINTR])
				break; /* ^C */
			if ((c | 0x20) == 'q')
				break;
		}
#endif

		if (stop_timer())
			G.nostats = 1;

		clear_lines();
#if ENABLE_FEATURE_POWERTOP_PROCIRQ
		do_proc_irq();
#endif

		/* Clear the stats */
		memset(cur_duration, 0, sizeof(cur_duration));
		memset(cur_usage, 0, sizeof(cur_usage));

		/* Read them */
		read_data(&cur_usage[0], &cur_duration[0]);

		totalticks = totalevents = 0;

		/* Count totalticks and totalevents */
		for (i = 0; i < 8; i++) {
			if (cur_usage[i]) {
				totalticks += cur_duration[i] - G.last_duration[i];
				totalevents += cur_usage[i] - G.last_usage[i];
			}
		}

		/* Show title bar */
		print_header();

		/* Clear C-state lines */
		memset(&cstate_lines, 0, sizeof(cstate_lines));

		if (totalevents == 0 && G.maxcstate <= 1) {
			/* This should not happen */
			sprintf(cstate_lines[5], "< Detailed C-state information is not "
				"available.>\n");
		} else {
			double slept;
			double percentage;

			newticks = G.total_cpus * DEFAULT_SLEEP * FREQ_ACPI_1000 - totalticks;

			/* Handle rounding errors: do not display negative values */
			if (newticks < 0)
				newticks = 0;

			sprintf(cstate_lines[0], "Cn\t          Avg residency\n");
			percentage = newticks * 100.0 / (G.total_cpus * DEFAULT_SLEEP * FREQ_ACPI_1000);
			sprintf(cstate_lines[1], "C0 (cpu running)        (%4.1f%%)\n",
				percentage);

			/* Compute values for individual C-states */
			for (i = 0; i < 8; i++) {
				if (cur_usage[i]) {
					slept = (cur_duration[i] - G.last_duration[i])
						/ (cur_usage[i] - G.last_usage[i] + 0.1) / FREQ_ACPI;
					percentage = (cur_duration[i] - G.last_duration[i]) * 100
						/ (G.total_cpus * DEFAULT_SLEEP * FREQ_ACPI_1000);

					if (!G.cstate_names[i][0])
						sprintf(G.cstate_names[i], "C%u", i + 1);
					sprintf(cstate_lines[i + 2], "%s\t%5.1fms (%4.1f%%)\n",
						G.cstate_names[i], slept, percentage);
					if (maxsleep < slept)
						maxsleep = slept;
				}
			}
		}

		/* Display C-states */
		show_cstates(cstate_lines);

		/* Do timer_stats info */
		buf[0] = '\0';
		totalticks = 0;

		fp = NULL;
		if (!G.nostats)
			fp = fopen_for_read("/proc/timer_stats");
		if (fp) {
			while (fgets(buf, sizeof(buf), fp)) {
				const char *count, *process, *func;
				char line[512];
				int cnt = 0;
				bool defferable = false;
				char *p;
				int j = 0;

/* Find char ' ', then eat remaining spaces */
#define ADVANCE(p) do {           \
	(p) = strchr((p), ' ');   \
	if (!(p))                 \
		continue;         \
	*(p) = '\0';              \
	(p)++;                    \
	(p) = skip_whitespace(p); \
} while (0)

				if (strstr(buf, "total events"))
					break;

				while (isspace(buf[j]))
					j++;

				count = &buf[j];
				p = (char *)count;

				/* Skip PID */
				p = strchr(p, ',');
				if (!p)
					continue;
				*p = '\0';
				p++;

				p = skip_whitespace(p);

				/* Get process */
				ADVANCE(p);
				process = p;

				/* Get function */
				ADVANCE(p);
				func = p;

				if (strcmp(process, "swapper") == 0
				 && strcmp(func, "hrtimer_start_range_ns (tick_sched_timer)\n") == 0
				) {
					process = "[kernel scheduler]";
					func = "Load balancing tick";
				}

				if (strcmp(process, "insmod") == 0)
					process = "[kernel module]";
				if (strcmp(process, "modprobe") == 0)
					process = "[kernel module]";
				if (strcmp(process, "swapper") == 0)
					process = "[kernel core]";

				p = strchr(p, '\n');

				if (strncmp(func, "tick_nohz_", 10) == 0)
					continue;
				if (strncmp(func, "tick_setup_sched_timer", 20) == 0)
					continue;
				if (strcmp(process, "powertop") == 0)
					continue;

				if (p)
					*p = '\0';

				cnt = bb_strtoull(count, &p, 10);
				while (*p != 0) {
					if (*p++ == 'D')
						defferable = true;
				}
				if (defferable)
					continue;

				if (strchr(process, '['))
					sprintf(line, "%s %s", process, func);
				else
					sprintf(line, "%s", process);
				push_line(line, cnt);
			}
			fclose(fp);
		}

#if ENABLE_FEATURE_POWERTOP_PROCIRQ
		if (strstr(buf, "total events")) {
			int n = bb_strtoull(buf, NULL, 10) / G.total_cpus;

			if (totalevents == 0) {
				/* No C-state info available, use timerstats */
				totalevents = n * G.total_cpus + G.total_interrupt;
				if (n < 0)
					totalevents += G.interrupt_0 - n;
			}
			if (n > 0 && n < G.interrupt_0)
				push_line("[extra timer interrupt]", G.interrupt_0 - n);
		}
#endif
		if (totalevents)
			printf("\n\033[1mWakeups-from-idle per second : %4.1f\tinterval:"
				"%ds\n\033[0m",
				(double)totalevents / DEFAULT_SLEEP / G.total_cpus, DEFAULT_SLEEP);

		count_lines();
		do_sort();

		show_timerstats(G.nostats);

		fflush(stdout);

		/* Clear the stats */
		memset(cur_duration, 0, sizeof(cur_duration));
		memset(cur_usage, 0, sizeof(cur_usage));

		/* Get new values */
		read_data(&cur_usage[0], &cur_duration[0]);

		/* Save them */
		memcpy(G.last_usage, cur_usage, sizeof(G.last_usage));
		memcpy(G.last_duration, cur_duration, sizeof(G.last_duration));
	} /* for (;;) */

	bb_putchar('\n');

	return EXIT_SUCCESS;
}
