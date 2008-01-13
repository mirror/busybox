/* vi: set sw=4 ts=4: */
/*
 * Small implementation of brctl for busybox.
 *
 * Copyright (C) 2008 by Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
/* This applet currently uses only the ioctl interface and no sysfs at all.
 * At the time of this writing this was considered a feature.
 */
#include "libbb.h"
#include <linux/sockios.h>
#include <net/if.h>

#ifdef ENABLE_FEATURE_BRCTL_SHOW
#error Remove these
#endif
#define ENABLE_FEATURE_BRCTL_SHOW 0
#define USE_FEATURE_BRCTL_SHOW(...)
#if 0
#define ENABLE_FEATURE_BRCTL_FANCY 0
#if ENABLE_FEATURE_BRCTL_FANCY
#define USE_FEATURE_BRCTL_FANCY(...) __VA_ARGS__
#else
#define USE_FEATURE_BRCTL_FANCY(...)
#endif
#endif
#if ENABLE_FEATURE_BRCTL_FANCY
#include <linux/if_bridge.h>
/* FIXME: These 4 funcs are not really clean and could be improved */
static inline ALWAYS_INLINE void strtotimeval(struct timeval *tv,
											  const char *time_str)
{
	double secs;
	if (sscanf(time_str, "%lf", &secs) != 1)
		bb_error_msg_and_die (bb_msg_invalid_arg, time_str, "timespec");
	tv->tv_sec = secs;
	tv->tv_usec = 1000000 * (secs - tv->tv_sec);
}

static inline ALWAYS_INLINE unsigned long __tv_to_jiffies(const struct timeval *tv)
{
	unsigned long long jif;

	jif = 1000000ULL * tv->tv_sec + tv->tv_usec;

	return jif/10000;
}
# ifdef UNUSED && 00
static void __jiffies_to_tv(struct timeval *tv, unsigned long jiffies)
{
	unsigned long long tvusec;

	tvusec = 10000ULL*jiffies;
	tv->tv_sec = tvusec/1000000;
	tv->tv_usec = tvusec - 1000000 * tv->tv_sec;
}
# endif
static unsigned long str_to_jiffies(const char *time_str)
{
	struct timeval tv;
	strtotimeval(&tv, time_str);
	return __tv_to_jiffies(&tv);
}
#endif


int brctl_main(int argc ATTRIBUTE_UNUSED, char **argv) MAIN_EXTERNALLY_VISIBLE;
int brctl_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int fd;
	static const char keywords[] ALIGN1 =
		"addbr\0" "delbr\0" "addif\0" "delif\0"
	USE_FEATURE_BRCTL_FANCY(
		"setageing\0" "setfd\0" "sethello\0" "setmaxage\0"
		"setpathcost\0" "setportprio\0" "setbridgeprio\0"
		"stp\0"
	)
		USE_FEATURE_BRCTL_SHOW("showmacs\0" "show\0");
	enum { ARG_addbr = 0, ARG_delbr, ARG_addif, ARG_delif
		USE_FEATURE_BRCTL_FANCY(,
		   ARG_setageing, ARG_setfd, ARG_sethello, ARG_setmaxage,
		   ARG_setpathcost, ARG_setportprio, ARG_setbridgeprio,
		   ARG_stp
		)
		  USE_FEATURE_BRCTL_SHOW(, ARG_showmacs, ARG_show) };
	smallint key;
	struct ifreq ifr;
	static char info[] = "bridge %s\0 iface %s";
	char *br;

	argv++;
	while (*argv) {
		key = index_in_strings(keywords, *argv);
		if (key == -1) /* no match found in keywords array, bail out. */
			bb_error_msg_and_die(bb_msg_invalid_arg, *argv, applet_name);
		argv++;
#if ENABLE_FEATURE_BRCTL_SHOW
		if (key == ARG_show) { /* show */
			goto out; /* FIXME: implement me! :) */
		}
#endif
		fd = xsocket(AF_INET, SOCK_STREAM, 0);
		br = *(argv++);

		if (key == ARG_addbr || key == ARG_delbr) { /* addbr or delbr */
			ioctl_or_perror_and_die(fd,
								key == ARG_addbr ? SIOCBRADDBR : SIOCBRDELBR,
								br, info, br);
			goto done;
		}
		if (!*argv) /* all but 'show' need at least one argument */
			bb_show_usage();
		safe_strncpy(ifr.ifr_name, br, IFNAMSIZ);
		if (key == ARG_addif || key == ARG_delif) { /* addif or delif */
			char *brif;

			brif = *(argv++);
			if (!(ifr.ifr_ifindex = if_nametoindex(brif))) {
				bb_perror_msg_and_die(info+11, brif);
			}
			ioctl_or_perror_and_die(fd,
								  key == ARG_addif ? SIOCBRADDIF : SIOCBRDELIF,
								  &ifr, info, br);
		}
#if ENABLE_FEATURE_BRCTL_FANCY
		if (key - ARG_delif < 5) { /* time related ops */
			unsigned long op = (key == ARG_setageing) ? BRCTL_SET_AGEING_TIME :
							(key == ARG_setfd) ? BRCTL_SET_BRIDGE_FORWARD_DELAY:
							(key == ARG_sethello) ? BRCTL_SET_BRIDGE_HELLO_TIME:
							(key == ARG_setmaxage) ? BRCTL_SET_BRIDGE_MAX_AGE :
							-1/*will never be used */;
			unsigned long jiff = str_to_jiffies (*(argv++));
			unsigned long args[4] = {op, jiff, 0, 0};
			ifr.ifr_data = (char *) &args;
			xioctl(fd, SIOCDEVPRIVATE, &ifr);
		}
#endif
 done:
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);
	}
 out:
	return EXIT_SUCCESS;
}
