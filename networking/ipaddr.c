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

#include "libiproute/utils.h"
#include "libiproute/ip_common.h"

#include "busybox.h"

int ipaddr_main(int argc, char **argv);
int ipaddr_main(int argc, char **argv)
{
	ip_parse_common_args(&argc, &argv);

	return do_ipaddr(argc-1, argv+1);
}
