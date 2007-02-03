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

#include "busybox.h"

#include "libiproute/utils.h"
#include "libiproute/ip_common.h"

int ip_main(int argc, char **argv);
int ip_main(int argc, char **argv)
{
	int ret = EXIT_FAILURE;

	ip_parse_common_args(&argc, &argv);

	if (argc > 1) {
		if (ENABLE_FEATURE_IP_ADDRESS && matches(argv[1], "address") == 0) {
			ret = do_ipaddr(argc-2, argv+2);
		}
		if (ENABLE_FEATURE_IP_ROUTE && matches(argv[1], "route") == 0) {
			ret = do_iproute(argc-2, argv+2);
		}
		if (ENABLE_FEATURE_IP_LINK && matches(argv[1], "link") == 0) {
			ret = do_iplink(argc-2, argv+2);
		}
		if (ENABLE_FEATURE_IP_TUNNEL
		 && (matches(argv[1], "tunnel") == 0 || strcmp(argv[1], "tunl") == 0)
		) {
			ret = do_iptunnel(argc-2, argv+2);
		}
		if (ENABLE_FEATURE_IP_RULE && matches(argv[1], "rule") == 0) {
			ret = do_iprule(argc-2, argv+2);
		}
	}
	if (ret) {
		bb_show_usage();
	}
	return EXIT_SUCCESS;
}
