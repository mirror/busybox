/* vi: set sw=4 ts=4: */
/*
 * beep implementation for busybox
 *
 * Copyright (C) 2009 Bernhard Reutner-Fischer
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 */
#include "libbb.h"

#include <linux/kd.h>
#ifndef CLOCK_TICK_RATE
#define CLOCK_TICK_RATE 1193180
#endif

#define OPT_f (1<<0)
#define OPT_l (1<<1)
#define OPT_d (1<<2)
#define OPT_r (1<<3)
/* defaults */
#ifndef CONFIG_FEATURE_BEEP_FREQ
# define FREQ (4000)
#else
# define FREQ (CONFIG_FEATURE_BEEP_FREQ)
#endif
#ifndef CONFIG_FEATURE_BEEP_LENGTH
# define LENGTH (30)
#else
# define LENGTH (CONFIG_FEATURE_BEEP_LENGTH)
#endif
#define DELAY (0)
#define REPETITIONS (1)

#define GET_ARG do { if (!*++opt) opt = *++argv; if (opt == NULL) bb_show_usage();} while (0)
#define NEW_BEEP() { \
	freq = FREQ; \
	length = LENGTH; \
	delay = DELAY; \
	rep = REPETITIONS; \
	}

int beep_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int beep_main(int argc UNUSED_PARAM, char **argv)
{
	int speaker = get_console_fd_or_die();
	unsigned freq, length, delay, rep;
	unsigned long ioctl_arg;
	char *opt = NULL;
	bool do_parse = true;

	NEW_BEEP()
	while (*argv && *++argv) {
		opt = *argv;

		while (*opt == '-')
			++opt;
		if (do_parse)
			switch (*opt) {
			case 'f':
				GET_ARG;
				freq = xatoul(opt);
				continue;
			case 'l':
				GET_ARG;
				length = xatoul(opt);
				continue;
			case 'd':
				GET_ARG;
				delay = xatoul(opt);
				continue;
			case 'r':
				GET_ARG;
				rep = xatoul(opt);
				continue;
			case 'n':
				break;
			default:
				bb_show_usage();
				break;
			}
 again:
		while (rep) {
//bb_info_msg("rep[%d] freq=%d, length=%d, delay=%d", rep, freq, length, delay);
			ioctl_arg = (int)(CLOCK_TICK_RATE/freq);
			xioctl(speaker, KIOCSOUND, (void*)ioctl_arg);
			usleep(1000 * length);
			ioctl(speaker, KIOCSOUND, 0);
			if (rep--)
				usleep(delay);
		}
		if (opt && *opt == 'n')
				NEW_BEEP()
		if (!do_parse && *argv == NULL)
			goto out;
	}
	do_parse = false;
	goto again;
 out:
	if (ENABLE_FEATURE_CLEAN_UP)
		close(speaker);
	return EXIT_SUCCESS;
}
/*
 * so, e.g. Beethoven's 9th symphony "Ode an die Freude" would be
 * something like:
a=$((220*3))
b=$((247*3))
c=$((262*3))
d=$((294*3))
e=$((329*3))
f=$((349*3))
g=$((392*3))
#./beep -f$d -l200 -r2 -n -f$e -l100 -d 10 -n -f$c -l400 -f$g -l200
./beep -f$e -l200 -r2 \
        -n -d 100 -f$f -l200 \
        -n -f$g -l200 -r2 \
        -n -f$f -l200 \
        -n -f$e -l200 \
        -n -f$d -l200  \
        -n -f$c -l200 -r2  \
        -n -f$d -l200  \
        -n -f$e -l200  \
        -n -f$e -l400  \
        -n -f$d -l100 \
        -n -f$d -l200 \
*/
