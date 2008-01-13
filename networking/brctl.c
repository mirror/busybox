/* vi: set sw=4 ts=4: */
/*
 * Small implementation of brctl for busybox.
 *
 * Copyright (C) 2008 by Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <linux/sockios.h>
#include <net/if.h>

#ifdef ENABLE_FEATURE_BRCTL_SHOW
#error Remove these
#endif
#define ENABLE_FEATURE_BRCTL_SHOW 0
#define USE_FEATURE_BRCTL_SHOW(...)


/* Fancy diagnostics -- printing add/del -- costs 49 bytes. */
#if 0
#define BRCTL_VERBOSE(...) __VA_ARGS__
#else
#define BRCTL_VERBOSE(...)
#endif

int brctl_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int brctl_main(int argc, char **argv)
{
	int fd;
	static const char keywords[] ALIGN1 =
		"addbr\0" "delbr\0" "addif\0" "delif\0"
		USE_FEATURE_BRCTL_SHOW("show\0");
	enum { ARG_addbr = 1, ARG_delbr, ARG_addif, ARG_delif
		  USE_FEATURE_BRCTL_SHOW(, ARG_show) };
	smalluint key;
	static char info[] = BRCTL_VERBOSE("%s ")"bridge %s\0 iface %s";

	argv++;
	while (*argv) {
		BRCTL_VERBOSE(char *op;)

		key = index_in_strings(keywords, *argv) + 1;
		if (key == 0) /* no match found in keywords array, bail out. */
			bb_error_msg_and_die(bb_msg_invalid_arg, *argv, applet_name);
		argv++;
#if ENABLE_FEATURE_BRCTL_SHOW
		if (key == ARG_show) { /* show */
			;
		}
#endif
		BRCTL_VERBOSE(op = (char*)((key % 2) ? "add" : "del");)
		fd = xsocket(AF_INET, SOCK_STREAM, 0);
		if (key < 3) {/* addbr or delbr */
			char *br;

			br = *(argv++);
			if (ioctl(fd, key == ARG_addbr ? SIOCBRADDBR : SIOCBRDELBR, br) < 0)
			{
				info[9 BRCTL_VERBOSE(+3)] = '\0';
				bb_perror_msg_and_die(info, BRCTL_VERBOSE(op,) br);
			}
		}
		if (key > 2) { /* addif or delif */
			struct ifreq ifr;
			char *br, *brif;

			br = *(argv++);
			if (!*argv)
				bb_show_usage();
			brif = *(argv++);

			if (!(ifr.ifr_ifindex = if_nametoindex(brif))) {
				bb_perror_msg_and_die(info+11 BRCTL_VERBOSE(+3), brif);
			}
			safe_strncpy(ifr.ifr_name, br, IFNAMSIZ);
			if (ioctl(fd,
					  key == ARG_addif ? SIOCBRADDIF : SIOCBRDELIF, &ifr) < 0) {
				info[9 BRCTL_VERBOSE(+3)] = ',';
				bb_perror_msg_and_die (info, BRCTL_VERBOSE(op,) br, brif);
			}
		}
		if (ENABLE_FEATURE_CLEAN_UP)
			close(fd);
	}
	return EXIT_SUCCESS;
}
