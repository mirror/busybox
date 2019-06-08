/* vi: set sw=4 ts=4: */
/*
 * Small implementation of brctl for busybox.
 *
 * Copyright (C) 2008 by Bernhard Reutner-Fischer
 *
 * Some helper functions from bridge-utils are
 * Copyright (C) 2000 Lennert Buytenhek
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config BRCTL
//config:	bool "brctl (4.7 kb)"
//config:	default y
//config:	select PLATFORM_LINUX
//config:	help
//config:	Manage ethernet bridges.
//config:	Supports addbr/delbr and addif/delif.
//config:
//config:config FEATURE_BRCTL_FANCY
//config:	bool "Fancy options"
//config:	default y
//config:	depends on BRCTL
//config:	help
//config:	Add support for extended option like:
//config:		setageing, setfd, sethello, setmaxage,
//config:		setpathcost, setportprio, setbridgeprio,
//config:		stp
//config:	This adds about 600 bytes.
//config:
//config:config FEATURE_BRCTL_SHOW
//config:	bool "Support show"
//config:	default y
//config:	depends on BRCTL && FEATURE_BRCTL_FANCY
//config:	help
//config:	Add support for option which prints the current config:
//config:		show

//applet:IF_BRCTL(APPLET_NOEXEC(brctl, brctl, BB_DIR_USR_SBIN, BB_SUID_DROP, brctl))

//kbuild:lib-$(CONFIG_BRCTL) += brctl.o

//usage:#define brctl_trivial_usage
//usage:       "COMMAND [BRIDGE [ARGS]]"
//usage:#define brctl_full_usage "\n\n"
//usage:       "Manage ethernet bridges"
//usage:     "\nCommands:"
//usage:	IF_FEATURE_BRCTL_SHOW(
//usage:     "\n	show [BRIDGE]...	Show bridges"
//usage:	)
//usage:     "\n	addbr BRIDGE		Create BRIDGE"
//usage:     "\n	delbr BRIDGE		Delete BRIDGE"
//usage:     "\n	addif BRIDGE IFACE	Add IFACE to BRIDGE"
//usage:     "\n	delif BRIDGE IFACE	Delete IFACE from BRIDGE"
//usage:	IF_FEATURE_BRCTL_FANCY(
//usage:     "\n	stp BRIDGE 1/yes/on|0/no/off	STP on/off"
//usage:     "\n	setageing BRIDGE SECONDS	Set ageing time"
//usage:     "\n	setfd BRIDGE SECONDS		Set bridge forward delay"
//usage:     "\n	sethello BRIDGE SECONDS		Set hello time"
//usage:     "\n	setmaxage BRIDGE SECONDS	Set max message age"
//usage:     "\n	setbridgeprio BRIDGE PRIO	Set bridge priority"
//usage:     "\n	setportprio BRIDGE IFACE PRIO	Set port priority"
//usage:     "\n	setpathcost BRIDGE IFACE COST	Set path cost"
//usage:	)
// Not yet implemented:
//			hairpin BRIDGE IFACE on|off	Hairpin on/off
//			showmacs BRIDGE			List mac addrs
//			showstp	BRIDGE			Show stp info

#include "libbb.h"
#include "common_bufsiz.h"
#include <linux/sockios.h>
#include <net/if.h>

#ifndef SIOCBRADDBR
# define SIOCBRADDBR BRCTL_ADD_BRIDGE
#endif
#ifndef SIOCBRDELBR
# define SIOCBRDELBR BRCTL_DEL_BRIDGE
#endif
#ifndef SIOCBRADDIF
# define SIOCBRADDIF BRCTL_ADD_IF
#endif
#ifndef SIOCBRDELIF
# define SIOCBRDELIF BRCTL_DEL_IF
#endif

#if ENABLE_FEATURE_BRCTL_FANCY
static unsigned str_to_jiffies(const char *time_str)
{
	double dd;
	char *endptr;
	dd = /*bb_*/strtod(time_str, &endptr);
	if (endptr == time_str || dd < 0)
		bb_error_msg_and_die(bb_msg_invalid_arg_to, time_str, "timespec");

	dd *= 100;
	/* For purposes of brctl,
	 * capping SECONDS by ~20 million seconds is quite enough:
	 */
	if (dd > INT_MAX)
		dd = INT_MAX;

	return dd;
}
#endif

#define filedata bb_common_bufsiz1

#if ENABLE_FEATURE_BRCTL_SHOW
static int read_file(const char *name)
{
	int n = open_read_close(name, filedata, COMMON_BUFSIZE - 1);
	if (n < 0) {
		filedata[0] = '\0';
	} else {
		filedata[n] = '\0';
		if (n != 0 && filedata[n - 1] == '\n')
			filedata[--n] = '\0';
	}
	return n;
}

/* NB: we are in /sys/class/net
 */
static int show_bridge(const char *name, int need_hdr)
{
/* Output:
 *bridge name	bridge id		STP enabled	interfaces
 *br0		8000.000000000000	no		eth0
 */
	char pathbuf[IFNAMSIZ + sizeof("/bridge/bridge_id") + 32];
	int tabs;
	DIR *ifaces;
	struct dirent *ent;
	char *sfx;

#if IFNAMSIZ == 16
	sfx = pathbuf + sprintf(pathbuf, "%.16s/bridge/", name);
#else
	sfx = pathbuf + sprintf(pathbuf, "%.*s/bridge/", (int)IFNAMSIZ, name);
#endif
	strcpy(sfx, "bridge_id");
	if (read_file(pathbuf) < 0)
		return -1; /* this iface is not a bridge */

	if (need_hdr)
		puts("bridge name\tbridge id\t\tSTP enabled\tinterfaces");
	printf("%s\t\t", name);
	printf("%s\t", filedata);

	strcpy(sfx, "stp_state");
	read_file(pathbuf);
	if (LONE_CHAR(filedata, '0'))
		strcpy(filedata, "no");
	else
	if (LONE_CHAR(filedata, '1'))
		strcpy(filedata, "yes");
	fputs(filedata, stdout);

	strcpy(sfx - (sizeof("bridge/")-1), "brif");
	tabs = 0;
	ifaces = opendir(pathbuf);
	if (ifaces) {
		while ((ent = readdir(ifaces)) != NULL) {
			if (DOT_OR_DOTDOT(ent->d_name))
				continue; /* . or .. */
			if (tabs)
				printf("\t\t\t\t\t");
			else
				tabs = 1;
			printf("\t\t%s\n", ent->d_name);
		}
		closedir(ifaces);
	}
	if (!tabs)  /* bridge has no interfaces */
		bb_putchar('\n');
	return 0;
}
#endif

#if ENABLE_FEATURE_BRCTL_FANCY
static void write_uint(const char *name, const char *leaf, unsigned val)
{
	char pathbuf[IFNAMSIZ + sizeof("/bridge/bridge_id") + 32];
	int fd, n;

#if IFNAMSIZ == 16
	sprintf(pathbuf, "%.16s/%s", name, leaf);
#else
	sprintf(pathbuf, "%.*s/%s", (int)IFNAMSIZ, name, leaf);
#endif
	fd = xopen(pathbuf, O_WRONLY);
	n = sprintf(filedata, "%u\n", val);
	if (write(fd, filedata, n) < 0)
		bb_simple_perror_msg_and_die(name);
	close(fd);
}
#endif

int brctl_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int brctl_main(int argc UNUSED_PARAM, char **argv)
{
	static const char keywords[] ALIGN1 =
		"addbr\0" "delbr\0" "addif\0" "delif\0"
	IF_FEATURE_BRCTL_FANCY(
		"stp\0"
		"setageing\0" "setfd\0" "sethello\0" "setmaxage\0"
		"setpathcost\0" "setportprio\0"
		"setbridgeprio\0"
	)
	IF_FEATURE_BRCTL_SHOW("show\0");
	enum { ARG_addbr = 0, ARG_delbr, ARG_addif, ARG_delif
		IF_FEATURE_BRCTL_FANCY(,
			ARG_stp,
			ARG_setageing, ARG_setfd, ARG_sethello, ARG_setmaxage,
			ARG_setpathcost, ARG_setportprio,
			ARG_setbridgeprio
		)
		IF_FEATURE_BRCTL_SHOW(, ARG_show)
	};

	argv++;
	if (!*argv) {
		/* bare "brctl" shows --help */
		bb_show_usage();
	}

	xchdir("/sys/class/net");

//	while (*argv)
	{
		smallint key;
		char *br;

		key = index_in_strings(keywords, *argv);
		if (key == -1) /* no match found in keywords array, bail out. */
			bb_error_msg_and_die(bb_msg_invalid_arg_to, *argv, applet_name);
		argv++;

#if ENABLE_FEATURE_BRCTL_SHOW
		if (key == ARG_show) { /* show [BR]... */
			DIR *net;
			struct dirent *ent;
			int need_hdr = 1;
			int exitcode = EXIT_SUCCESS;

			if (*argv) {
				/* "show BR1 BR2 BR3" */
				do {
					if (show_bridge(*argv, need_hdr) >= 0) {
						need_hdr = 0;
					} else {
						bb_error_msg("bridge %s does not exist", *argv);
//TODO: if device exists, but is not a BR, brctl from bridge-utils 1.6
//says this instead: "device eth0 is not a bridge"
						exitcode = EXIT_FAILURE;
					}
				} while (*++argv != NULL);
				return exitcode;
			}

			/* "show" (if no ifaces, shows nothing, not even header) */
			net = xopendir(".");
			while ((ent = readdir(net)) != NULL) {
				if (DOT_OR_DOTDOT(ent->d_name))
					continue; /* . or .. */
				if (show_bridge(ent->d_name, need_hdr) >= 0)
					need_hdr = 0;
			}
			if (ENABLE_FEATURE_CLEAN_UP)
				closedir(net);
			return exitcode;
		}
#endif

		if (!*argv) /* all but 'show' need at least one argument */
			bb_show_usage();

		br = *argv++;

		if (key == ARG_addbr || key == ARG_delbr) {
			/* addbr or delbr */
			/* brctl from bridge-utils 1.6 still uses ioctl
			 * for SIOCBRADDBR / SIOCBRDELBR, not /sys accesses
			 */
			int fd = xsocket(AF_INET, SOCK_STREAM, 0);
			ioctl_or_perror_and_die(fd,
				key == ARG_addbr ? SIOCBRADDBR : SIOCBRDELBR,
				br, "bridge %s", br
			);
			//close(fd);
			//goto done;
			/* bridge-utils 1.6 simply ignores trailing args:
			 * "brctl addbr BR1 ARGS" ignores ARGS
			 */
			if (ENABLE_FEATURE_CLEAN_UP)
				close(fd);
			return EXIT_SUCCESS;
		}

		if (!*argv) /* all but 'addbr/delbr' need at least two arguments */
			bb_show_usage();

#if ENABLE_FEATURE_BRCTL_FANCY
		if (key == ARG_stp) { /* stp */
			static const char no_yes[] ALIGN1 =
				"0\0" "off\0" "n\0" "no\0"   /* 0 .. 3 */
				"1\0" "on\0"  "y\0" "yes\0"; /* 4 .. 7 */
			int onoff = index_in_strings(no_yes, *argv);
			if (onoff < 0)
				bb_error_msg_and_die(bb_msg_invalid_arg_to, *argv, applet_name);
			onoff = (unsigned)onoff / 4;
			write_uint(br, "bridge/stp_state", onoff);
			//goto done_next_argv;
			return EXIT_SUCCESS;
		}

		if ((unsigned)(key - ARG_setageing) < 4) { /* time related ops */
			/* setageing BR N: "N*100\n" to /sys/class/net/BR/bridge/ageing_time
			 * setfd BR N:     "N*100\n" to /sys/class/net/BR/bridge/forward_delay
			 * sethello BR N:  "N*100\n" to /sys/class/net/BR/bridge/hello_time
			 * setmaxage BR N: "N*100\n" to /sys/class/net/BR/bridge/max_age
			 */
			write_uint(br,
				nth_string(
					"bridge/ageing_time"  "\0" /* ARG_setageing */
					"bridge/forward_delay""\0" /* ARG_setfd     */
					"bridge/hello_time"   "\0" /* ARG_sethello  */
					"bridge/max_age",          /* ARG_setmaxage */
					key - ARG_setageing
				),
				str_to_jiffies(*argv)
			);
			//goto done_next_argv;
			return EXIT_SUCCESS;
		}

		if (key == ARG_setbridgeprio) {
			write_uint(br, "bridge/priority", xatoi_positive(*argv));
			//goto done_next_argv;
			return EXIT_SUCCESS;
		}

		if (key == ARG_setpathcost
		 || key == ARG_setportprio
		) {
			if (!argv[1])
				bb_show_usage();
			/* BR is not used (and ignored!) for these commands:
			 * "setpathcost BR PORT N" writes "N\n" to
			 * /sys/class/net/PORT/brport/path_cost
			 * "setportprio BR PORT N" writes "N\n" to
			 * /sys/class/net/PORT/brport/priority
			 */
			write_uint(argv[0],
				nth_string(
					"brport/path_cost" "\0" /* ARG_setpathcost */
					"brport/priority",      /* ARG_setportprio */
					key - ARG_setpathcost
				),
				xatoi_positive(argv[1])
			);
			//argv++;
			//goto done_next_argv;
			return EXIT_SUCCESS;
		}

/* TODO: "showmacs BR"
 *	port no\tmac addr\t\tis local?\tageing timer
 *	<sp><sp>1\txx:xx:xx:xx:xx:xx\tno\t\t<sp><sp><sp>1.31
 *	port no	mac addr		is local?	ageing timer
 *	  1	xx:xx:xx:xx:xx:xx	no		   1.31
 * Read fixed-sized records from /sys/class/net/BR/brforward:
 *	struct __fdb_entry {
 *		uint8_t  mac_addr[ETH_ALEN];
 *		uint8_t  port_no; //lsb
 *		uint8_t  is_local;
 *		uint32_t ageing_timer_value;
 *		uint8_t  port_hi;
 *		uint8_t  pad0;
 *		uint16_t unused;
 *	};
 */
#endif
		/* always true: if (key == ARG_addif || key == ARG_delif) */ {
			/* addif or delif */
			struct ifreq ifr;
			int fd = xsocket(AF_INET, SOCK_STREAM, 0);

			strncpy_IFNAMSIZ(ifr.ifr_name, br);
			ifr.ifr_ifindex = if_nametoindex(*argv);
			if (ifr.ifr_ifindex == 0) {
				bb_perror_msg_and_die("iface %s", *argv);
			}
			ioctl_or_perror_and_die(fd,
				key == ARG_addif ? SIOCBRADDIF : SIOCBRDELIF,
				&ifr, "bridge %s", br
			);
			//close(fd);
			//goto done_next_argv;
			if (ENABLE_FEATURE_CLEAN_UP)
				close(fd);
			return EXIT_SUCCESS;
		}

// done_next_argv:
//		argv++;
// done:
	}

	return EXIT_SUCCESS;
}
