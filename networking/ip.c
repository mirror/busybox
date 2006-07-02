/* vi: set sw=4 ts=4: */
/*
 * ip.c		"ip" utility frontend.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include "libiproute/utils.h"
#include "libiproute/ip_common.h"

#include "busybox.h"

int ip_main(int argc, char **argv)
{
	int ret = EXIT_FAILURE;

	ip_parse_common_args(&argc, &argv);

	if (argc > 1) {
#ifdef CONFIG_FEATURE_IP_ADDRESS
		if (matches(argv[1], "address") == 0) {
			ret = do_ipaddr(argc-2, argv+2);
		}
#endif
#ifdef CONFIG_FEATURE_IP_ROUTE
		if (matches(argv[1], "route") == 0) {
			ret = do_iproute(argc-2, argv+2);
		}
#endif
#ifdef CONFIG_FEATURE_IP_LINK
		if (matches(argv[1], "link") == 0) {
			ret = do_iplink(argc-2, argv+2);
		}
#endif
#ifdef CONFIG_FEATURE_IP_TUNNEL
		if (matches(argv[1], "tunnel") == 0 || strcmp(argv[1], "tunl") == 0) {
			ret = do_iptunnel(argc-2, argv+2);
		}
#endif
	}
	if (ret) {
		bb_show_usage();
	}
	return(EXIT_SUCCESS);
}
