/* vi: set sw=4 ts=4: */
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

#include <sys/socket.h>
#include <arpa/inet.h>

#include "libbb.h"

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

#if ENABLE_FEATURE_IPCALC_FANCY
static int get_prefix(unsigned long netmask)
{
	unsigned long msk = 0x80000000;
	int ret = 0;

	netmask = htonl(netmask);
	while (msk) {
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
	static const char ipcalc_longopts[] ALIGN1 =
		"netmask\0"   No_argument "m"
		"broadcast\0" No_argument "b"
		"network\0"   No_argument "n"
# if ENABLE_FEATURE_IPCALC_FANCY
		"prefix\0"    No_argument "p"
		"hostname\0"  No_argument "h"
		"silent\0"    No_argument "s"
# endif
		;
#endif

int ipcalc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ipcalc_main(int argc, char **argv)
{
	unsigned opt;
	int have_netmask = 0;
	in_addr_t netmask, broadcast, network, ipaddr;
	struct in_addr a;
	char *ipstr;

#if ENABLE_FEATURE_IPCALC_LONG_OPTIONS
	applet_long_options = ipcalc_longopts;
#endif
	opt = getopt32(argv, "mbn" USE_FEATURE_IPCALC_FANCY("phs"));
	argc -= optind;
	argv += optind;
	if (opt & (BROADCAST | NETWORK | NETPREFIX)) {
		if (argc > 2 || argc <= 0)
			bb_show_usage();
	} else {
		if (argc != 1)
			bb_show_usage();
	}
	if (opt & SILENT)
		logmode = LOGMODE_NONE; /* Suppress error_msg() output */

	ipstr = argv[0];
	if (ENABLE_FEATURE_IPCALC_FANCY) {
		unsigned long netprefix = 0;
		char *prefixstr;

		prefixstr = ipstr;

		while (*prefixstr) {
			if (*prefixstr == '/') {
				*prefixstr = (char)0;
				prefixstr++;
				if (*prefixstr) {
					unsigned msk;
					netprefix = xatoul_range(prefixstr, 0, 32);
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
		bb_error_msg_and_die("bad IP address: %s", argv[0]);
	}
	ipaddr = a.s_addr;

	if (argc == 2) {
		if (ENABLE_FEATURE_IPCALC_FANCY && have_netmask) {
			bb_error_msg_and_die("use prefix or netmask, not both");
		}

		netmask = inet_aton(argv[1], &a);
		if (netmask == 0) {
			bb_error_msg_and_die("bad netmask: %s", argv[1]);
		}
		netmask = a.s_addr;
	} else {

		/* JHC - If the netmask wasn't provided then calculate it */
		if (!ENABLE_FEATURE_IPCALC_FANCY || !have_netmask)
			netmask = get_netmask(ipaddr);
	}

	if (opt & NETMASK) {
		printf("NETMASK=%s\n", inet_ntoa((*(struct in_addr *) &netmask)));
	}

	if (opt & BROADCAST) {
		broadcast = (ipaddr & netmask) | ~netmask;
		printf("BROADCAST=%s\n", inet_ntoa((*(struct in_addr *) &broadcast)));
	}

	if (opt & NETWORK) {
		network = ipaddr & netmask;
		printf("NETWORK=%s\n", inet_ntoa((*(struct in_addr *) &network)));
	}

	if (ENABLE_FEATURE_IPCALC_FANCY) {
		if (opt & NETPREFIX) {
			printf("PREFIX=%i\n", get_prefix(netmask));
		}

		if (opt & HOSTNAME) {
			struct hostent *hostinfo;

			hostinfo = gethostbyaddr((char *) &ipaddr, sizeof(ipaddr), AF_INET);
			if (!hostinfo) {
				bb_herror_msg_and_die("cannot find hostname for %s", argv[0]);
			}
			str_tolower(hostinfo->h_name);

			printf("HOSTNAME=%s\n", hostinfo->h_name);
		}
	}

	return EXIT_SUCCESS;
}
