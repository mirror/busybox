/*
 * ip.c		"ip" utility frontend.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 */

#include "./libiproute/utils.h"
#include "./libiproute/ip_common.h"

#include "busybox.h"

int iplink_main(int argc, char **argv)
{
	ip_parse_common_args(&argc, &argv);

	return do_iplink(argc-1, argv+1);
}
