/* vi: set sw=4 ts=4 ai: */
/*
 * Mini ipcalc implementation for busybox
 *
 * By Jordan Crouse <jordan@cosmicpenguin.net>
 *    Stephan Linz  <linz@li-pro.net>
 *
 * This is a complete reimplementation of the ipcalc program
 * from Red Hat.  I didn't look at their source code, but there
 * is no denying that this is a loving reimplementation
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <ctype.h>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define IPCALC_MSG(CMD,ALTCMD) if (mode & SILENT) {ALTCMD;} else {CMD;}

#define CLASS_A_NETMASK	ntohl(0xFF000000)
#define CLASS_B_NETMASK	ntohl(0xFFFF0000)
#define CLASS_C_NETMASK	ntohl(0xFFFFFF00)

static unsigned long get_netmask(unsigned long ipaddr)
{
	ipaddr = htonl(ipaddr);

	if ((ipaddr & 0xC0000000) == 0xC0000000)
		return CLASS_C_NETMASK;
	else if ((ipaddr & 0x80000000) == 0x80000000)
		return CLASS_B_NETMASK;
	else if ((ipaddr & 0x80000000) == 0)
		return CLASS_A_NETMASK;
	else
		return 0;
}

#ifdef CONFIG_FEATURE_IPCALC_FANCY
static int get_prefix(unsigned long netmask)
{
	unsigned long msk = 0x80000000;
	int ret = 0;

	netmask = htonl(netmask);
	while(msk) {
		if (netmask & msk)
			ret++;
		msk >>= 1;
	}
	return ret;
}
#else
int get_prefix(unsigned long netmask);
#endif

#define NETMASK   0x01
#define BROADCAST 0x02
#define NETWORK   0x04
#define NETPREFIX 0x08
#define HOSTNAME  0x10
#define SILENT    0x20

#if ENABLE_FEATURE_IPCALC_LONG_OPTIONS
	static const struct option long_options[] = {
		{"netmask",		no_argument, NULL, 'm'},
		{"broadcast",	no_argument, NULL, 'b'},
		{"network",		no_argument, NULL, 'n'},
#ifdef CONFIG_FEATURE_IPCALC_FANCY
		{"prefix",		no_argument, NULL, 'p'},
		{"hostname",	no_argument, NULL, 'h'},
		{"silent",		no_argument, NULL, 's'},
#endif
		{NULL, 0, NULL, 0}
	};
#else
#define long_options 0
#endif



int ipcalc_main(int argc, char **argv)
{
	unsigned long mode;
	int have_netmask = 0;
	in_addr_t netmask, broadcast, network, ipaddr;
	struct in_addr a;
	char *ipstr;

	if (ENABLE_FEATURE_IPCALC_LONG_OPTIONS)
		bb_applet_long_options = long_options;

	mode = bb_getopt_ulflags(argc, argv, "mbn" USE_FEATURE_IPCALC_FANCY("phs"));

	argc -= optind;
	argv += optind;
	if (mode & (BROADCAST | NETWORK | NETPREFIX)) {
		if (argc > 2 || argc <= 0)
			bb_show_usage();
	} else {
		if (argc != 1)
			bb_show_usage();
	}

	ipstr = argv[0];
	if (ENABLE_FEATURE_IPCALC_FANCY) {
		unsigned long netprefix = 0;
		char *prefixstr;

		prefixstr = ipstr;

		while(*prefixstr) {
			if (*prefixstr == '/') {
				*prefixstr = (char)0;
				prefixstr++;
				if (*prefixstr) {
					unsigned int msk;

					if (safe_strtoul(prefixstr, &netprefix) || netprefix > 32) {
						IPCALC_MSG(bb_error_msg_and_die("bad IP prefix: %s\n", prefixstr),
								exit(EXIT_FAILURE));
					}
					netmask = 0;
					msk = 0x80000000;
					while (netprefix > 0) {
						netmask |= msk;
						msk >>= 1;
						netprefix--;
					}
					netmask = htonl(netmask);
					/* Even if it was 0, we will signify that we have a netmask. This allows */
					/* for specification of default routes, etc which have a 0 netmask/prefix */
					have_netmask = 1;
				}
				break;
			}
			prefixstr++;
		}
	}
	ipaddr = inet_aton(ipstr, &a);

	if (ipaddr == 0) {
		IPCALC_MSG(bb_error_msg_and_die("bad IP address: %s", argv[0]),
				exit(EXIT_FAILURE));
	}
	ipaddr = a.s_addr;

	if (argc == 2) {
		if (ENABLE_FEATURE_IPCALC_FANCY && have_netmask) {
			IPCALC_MSG(bb_error_msg_and_die("Use prefix or netmask, not both.\n"),
					exit(EXIT_FAILURE));
		}

		netmask = inet_aton(argv[1], &a);
		if (netmask == 0) {
			IPCALC_MSG(bb_error_msg_and_die("bad netmask: %s", argv[1]),
					exit(EXIT_FAILURE));
		}
		netmask = a.s_addr;
	} else {

		/* JHC - If the netmask wasn't provided then calculate it */
		if (!ENABLE_FEATURE_IPCALC_FANCY || !have_netmask)
			netmask = get_netmask(ipaddr);
	}

	if (mode & NETMASK) {
		printf("NETMASK=%s\n", inet_ntoa((*(struct in_addr *) &netmask)));
	}

	if (mode & BROADCAST) {
		broadcast = (ipaddr & netmask) | ~netmask;
		printf("BROADCAST=%s\n", inet_ntoa((*(struct in_addr *) &broadcast)));
	}

	if (mode & NETWORK) {
		network = ipaddr & netmask;
		printf("NETWORK=%s\n", inet_ntoa((*(struct in_addr *) &network)));
	}

	if (ENABLE_FEATURE_IPCALC_FANCY) {
		if (mode & NETPREFIX) {
			printf("PREFIX=%i\n", get_prefix(netmask));
		}

		if (mode & HOSTNAME) {
			struct hostent *hostinfo;
			int x;

			hostinfo = gethostbyaddr((char *) &ipaddr, sizeof(ipaddr), AF_INET);
			if (!hostinfo) {
				IPCALC_MSG(bb_herror_msg_and_die(
							"cannot find hostname for %s", argv[0]),);
				exit(EXIT_FAILURE);
			}
			for (x = 0; hostinfo->h_name[x]; x++) {
				hostinfo->h_name[x] = tolower(hostinfo->h_name[x]);
			}

			printf("HOSTNAME=%s\n", hostinfo->h_name);
		}
	}

	return EXIT_SUCCESS;
}
