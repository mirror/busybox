/* vi: set sw=4 ts=4: */
/*
 * Small implementation of brctl for busybox.
 *
 * Copyright (C) 2008 by Bernhard Fischer
 *
 * Some helper functions from bridge-utils are
 * Copyright (C) 2000 Lennert Buytenhek
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
/* This applet currently uses only the ioctl interface and no sysfs at all.
 * At the time of this writing this was considered a feature.
 */
#include "libbb.h"
#include <linux/sockios.h>
#include <net/if.h>

/* Maximum number of ports supported per bridge interface.  */
#ifndef MAX_PORTS
#define MAX_PORTS 32
#endif

/* Use internal number parsing and not the "exact" conversion.  */
/* #define BRCTL_USE_INTERNAL 0 */ /* use exact conversion */
#define BRCTL_USE_INTERNAL 1

#ifdef ENABLE_FEATURE_BRCTL_SHOW
#error Remove these
#endif
#define ENABLE_FEATURE_BRCTL_SHOW 0
#define USE_FEATURE_BRCTL_SHOW(...)

#if ENABLE_FEATURE_BRCTL_FANCY
#include <linux/if_bridge.h>

/* FIXME: These 4 funcs are not really clean and could be improved */
static ALWAYS_INLINE void strtotimeval(struct timeval *tv,
		const char *time_str)
{
	double secs;
#if BRCTL_USE_INTERNAL
	secs = /*bb_*/strtod(time_str, NULL);
	if (!secs)
#else
	if (sscanf(time_str, "%lf", &secs) != 1)
#endif
		bb_error_msg_and_die (bb_msg_invalid_arg, time_str, "timespec");
	tv->tv_sec = secs;
	tv->tv_usec = 1000000 * (secs - tv->tv_sec);
}

static ALWAYS_INLINE unsigned long __tv_to_jiffies(const struct timeval *tv)
{
	unsigned long long jif;

	jif = 1000000ULL * tv->tv_sec + tv->tv_usec;

	return jif/10000;
}
# if 0
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

static void arm_ioctl(unsigned long *args,
		unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
	args[0] = arg0;
	args[1] = arg1;
	args[2] = arg2;
	args[3] = 0;
}
#endif


int brctl_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int brctl_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	static const char keywords[] ALIGN1 =
		"addbr\0" "delbr\0" "addif\0" "delif\0"
	USE_FEATURE_BRCTL_FANCY(
		"stp\0"
		"setageing\0" "setfd\0" "sethello\0" "setmaxage\0"
		"setpathcost\0" "setportprio\0" "setbridgeprio\0"
	)
		USE_FEATURE_BRCTL_SHOW("showmacs\0" "show\0");

	enum { ARG_addbr = 0, ARG_delbr, ARG_addif, ARG_delif
		USE_FEATURE_BRCTL_FANCY(,
		   ARG_stp,
		   ARG_setageing, ARG_setfd, ARG_sethello, ARG_setmaxage,
		   ARG_setpathcost, ARG_setportprio, ARG_setbridgeprio
		)
		  USE_FEATURE_BRCTL_SHOW(, ARG_showmacs, ARG_show)
	};

	int fd;
	smallint key;
	struct ifreq ifr;
	char *br, *brif;
#if ENABLE_FEATURE_BRCTL_FANCY
	unsigned long args[4] = {0, 0, 0, 0};
	int port;
	int tmp;
#endif

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
		br = *argv++;

		if (key == ARG_addbr || key == ARG_delbr) { /* addbr or delbr */
			ioctl_or_perror_and_die(fd,
					key == ARG_addbr ? SIOCBRADDBR : SIOCBRDELBR,
					br, "bridge %s", br);
			goto done;
		}
		if (!*argv) /* all but 'show' need at least one argument */
			bb_show_usage();
		safe_strncpy(ifr.ifr_name, br, IFNAMSIZ);
		if (key == ARG_addif || key == ARG_delif) { /* addif or delif */
			brif = *argv++;
			ifr.ifr_ifindex = if_nametoindex(brif);
			if (!ifr.ifr_ifindex) {
				bb_perror_msg_and_die("iface %s", brif);
			}
			ioctl_or_perror_and_die(fd,
					key == ARG_addif ? SIOCBRADDIF : SIOCBRDELIF,
					&ifr, "bridge %s", br);
			goto done;
		}
#if ENABLE_FEATURE_BRCTL_FANCY
		ifr.ifr_data = (char *) &args;
		if (key == ARG_stp) { /* stp */
			/* FIXME: parsing yes/y/on/1 versus no/n/off/0 is too involved */
			arm_ioctl(args, BRCTL_SET_BRIDGE_STP_STATE,
					  (unsigned)(**argv - '0'), 0);
			goto fire;
		}
		if ((unsigned)(key - ARG_stp) < 5) { /* time related ops */
			unsigned long op = (key == ARG_setageing) ? BRCTL_SET_AGEING_TIME :
			                   (key == ARG_setfd) ? BRCTL_SET_BRIDGE_FORWARD_DELAY :
			                   (key == ARG_sethello) ? BRCTL_SET_BRIDGE_HELLO_TIME :
			                   /*key == ARG_setmaxage*/ BRCTL_SET_BRIDGE_MAX_AGE;
			arm_ioctl(args, op, str_to_jiffies(*argv), 0);
			goto fire;
		}
		port = -1;
		if (key == ARG_setpathcost || key == ARG_setportprio) {/* get portnum */
			int ifidx[MAX_PORTS];
			unsigned i;

			port = if_nametoindex(*argv);
			if (!port)
				bb_error_msg_and_die(bb_msg_invalid_arg, *argv, "port");
			argv++;
			memset(ifidx, 0, sizeof ifidx);
			arm_ioctl(args, BRCTL_GET_PORT_LIST, (unsigned long)ifidx,
					  MAX_PORTS);
			xioctl(fd, SIOCDEVPRIVATE, &ifr);
			for (i = 0; i < MAX_PORTS; i++) {
				if (ifidx[i] == port) {
					port = i;
					break;
				}
			}
		}
		if (key == ARG_setpathcost
		 || key == ARG_setportprio
		 || key == ARG_setbridgeprio
		) {
			unsigned long op = (key == ARG_setpathcost) ? BRCTL_SET_PATH_COST :
			                   (key == ARG_setportprio) ? BRCTL_SET_PORT_PRIORITY :
			                   /*key == ARG_setbridgeprio*/ BRCTL_SET_BRIDGE_PRIORITY;
			unsigned long arg1 = port;
			unsigned long arg2;
# if BRCTL_USE_INTERNAL
			tmp = xatoi(*argv);
# else
			if (sscanf(*argv, "%i", &tmp) != 1)
				bb_error_msg_and_die(bb_msg_invalid_arg, *argv,
						key == ARG_setpathcost ? "cost" : "prio");
# endif
			if (key == ARG_setbridgeprio) {
				arg1 = tmp;
				arg2 = 0;
			} else
				arg2 = tmp;
			arm_ioctl(args, op, arg1, arg2);
		}
 fire:
		/* Execute the previously set command.  */
		xioctl(fd, SIOCDEVPRIVATE, &ifr);
		argv++;
#endif
 done:
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);
	}
 USE_FEATURE_BRCTL_SHOW(out:)
	return EXIT_SUCCESS;
}
