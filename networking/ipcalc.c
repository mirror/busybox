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

#define NETMASK   0x01
#define BROADCAST 0x02
#define NETWORK   0x04
#define HOSTNAME  0x08
#define SILENT    0x10

int ipcalc_main(int argc, char **argv)
{
	unsigned long mode;

	unsigned long netmask = 0;
	unsigned long broadcast;
	unsigned long network;
	unsigned long ipaddr;

	static const struct option long_options[] = {
		{"netmask", no_argument, NULL, 'n'},
		{"broadcast", no_argument, NULL, 'b'},
		{"network", no_argument, NULL, 'w'},
#ifdef CONFIG_FEATURE_IPCALC_FANCY
		{"hostname", no_argument, NULL, 'h'},
		{"silent", no_argument, NULL, 's'},
#endif
		{NULL, 0, NULL, 0}
	};

	bb_applet_long_options = long_options;
	mode = bb_getopt_ulflags(argc, argv,
#ifdef CONFIG_FEATURE_IPCALC_FANCY
							  "nbwhs");
#else
							  "nbw");
#endif
	if (mode & (BROADCAST | NETWORK)) {
		if (argc - optind > 2) {
			bb_show_usage();
		}
	} else {
		if (argc - optind != 1) {
			bb_show_usage();
		}
	}

	ipaddr = inet_addr(argv[optind]);

	if (ipaddr == INADDR_NONE) {
		IPCALC_MSG(bb_error_msg_and_die("bad IP address: %s", argv[optind]),
				   exit(EXIT_FAILURE));
	}


	if (argc - optind == 2) {
		netmask = inet_addr(argv[optind + 1]);
	}

	if (ipaddr == INADDR_NONE) {
		IPCALC_MSG(bb_error_msg_and_die("bad netmask: %s", argv[optind + 1]),
				   exit(EXIT_FAILURE));
	}

	/* JHC - If the netmask wasn't provided then calculate it */
	if (!netmask) {
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
