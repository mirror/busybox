/* vi: set sw=4 ts=4 ai: */
/*
 * Mini ipcalc implementation for busybox
 *
 * By Jordan Crouse <jordan@cosmicpenguin.net>
 *    Stephan Linz  <linz@li-pro.net>
 *
 * This is a complete reimplentation of the ipcalc program
 * from Redhat.  I didn't look at their source code, but there
 * is no denying that this is a loving reimplementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "busybox.h"

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
#endif

#define NETMASK   0x01
#define BROADCAST 0x02
#define NETWORK   0x04
#define NETPREFIX 0x08
#define HOSTNAME  0x10
#define SILENT    0x20


int ipcalc_main(int argc, char **argv)
{
	unsigned long mode;

	unsigned long netmask;
	unsigned long broadcast;
	unsigned long network;
	unsigned long ipaddr;
	struct in_addr a;

#ifdef CONFIG_FEATURE_IPCALC_FANCY
	unsigned long netprefix = 0;
	int have_netmask = 0;
	char *ipstr, *prefixstr;
#endif

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

	bb_applet_long_options = long_options;
	mode = bb_getopt_ulflags(argc, argv,
#ifdef CONFIG_FEATURE_IPCALC_FANCY
			"mbnphs"
#else
			"mbn"
#endif
			);
	if (mode & (BROADCAST | NETWORK | NETPREFIX)) {
		if (argc - optind > 2) {
			bb_show_usage();
		}
	} else {
		if (argc - optind != 1) {
			bb_show_usage();
		}
	}

#ifdef CONFIG_FEATURE_IPCALC_FANCY
	prefixstr = ipstr = argv[optind];

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
	ipaddr = inet_aton(ipstr, &a);
#else
	ipaddr = inet_aton(argv[optind], &a);
#endif

	if (ipaddr == 0) {
		IPCALC_MSG(bb_error_msg_and_die("bad IP address: %s", argv[optind]),
				exit(EXIT_FAILURE));
	}
	ipaddr = a.s_addr;

	if (argc - optind == 2) {
#ifdef CONFIG_FEATURE_IPCALC_FANCY
		if (have_netmask) {
			IPCALC_MSG(bb_error_msg_and_die("Both prefix and netmask were specified, use one or the other.\n"),
					exit(EXIT_FAILURE));
		}

#endif
		netmask = inet_aton(argv[optind + 1], &a);
		if (netmask == 0) {
			IPCALC_MSG(bb_error_msg_and_die("bad netmask: %s", argv[optind + 1]),
					exit(EXIT_FAILURE));
		}
		netmask = a.s_addr;
	} else {
#ifdef CONFIG_FEATURE_IPCALC_FANCY

		if (!have_netmask)
#endif
			/* JHC - If the netmask wasn't provided then calculate it */
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

#ifdef CONFIG_FEATURE_IPCALC_FANCY
	if (mode & NETPREFIX) {
		printf("PREFIX=%i\n", get_prefix(netmask));
	}

	if (mode & HOSTNAME) {
		struct hostent *hostinfo;
		int x;

		hostinfo = gethostbyaddr((char *) &ipaddr, sizeof(ipaddr), AF_INET);
		if (!hostinfo) {
			IPCALC_MSG(bb_herror_msg_and_die(
						"cannot find hostname for %s", argv[optind]),);
			exit(EXIT_FAILURE);
		}
		for (x = 0; hostinfo->h_name[x]; x++) {
			hostinfo->h_name[x] = tolower(hostinfo->h_name[x]);
		}

		printf("HOSTNAME=%s\n", hostinfo->h_name);
	}
#endif

	return EXIT_SUCCESS;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
