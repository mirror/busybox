/* vi: set sw=4 ts=4: */
/*
 * vconfig implementation for busybox
 *
 * Copyright (C) 2001  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* BB_AUDIT SUSv3 N/A */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_vlan.h>
#include <string.h>
#include <limits.h>
#include "busybox.h"

#define VLAN_GROUP_ARRAY_LEN 4096
#define SIOCSIFVLAN	0x8983		/* Set 802.1Q VLAN options 	*/

/* On entry, table points to the length of the current string plus
 * nul terminator plus data length for the subsequent entry.  The
 * return value is the last data entry for the matching string. */
static const char *xfind_str(const char *table, const char *str)
{
	while (strcasecmp(str, table+1) != 0) {
		if (!*(table += table[0])) {
			bb_show_usage();
		}
	}
	return table - 1;
}

static const char cmds[] = {
	4, ADD_VLAN_CMD, 7,
	'a', 'd', 'd', 0,
	3, DEL_VLAN_CMD, 7,
	'r', 'e', 'm', 0,
	3, SET_VLAN_NAME_TYPE_CMD, 17,
	's', 'e', 't', '_',
	'n', 'a', 'm', 'e', '_',
	't', 'y', 'p', 'e', 0,
	4, SET_VLAN_FLAG_CMD, 12,
	's', 'e', 't', '_',
	'f', 'l', 'a', 'g', 0,
	5, SET_VLAN_EGRESS_PRIORITY_CMD, 18,
	's', 'e', 't', '_',
	'e', 'g', 'r', 'e', 's', 's', '_',
	'm', 'a', 'p', 0,
	5, SET_VLAN_INGRESS_PRIORITY_CMD, 16,
	's', 'e', 't', '_',
	'i', 'n', 'g', 'r', 'e', 's', 's', '_',
	'm', 'a', 'p', 0,
};

static const char name_types[] = {
	VLAN_NAME_TYPE_PLUS_VID, 16,
	'V', 'L', 'A', 'N',
	'_', 'P', 'L', 'U', 'S', '_', 'V', 'I', 'D',
	0,
	VLAN_NAME_TYPE_PLUS_VID_NO_PAD, 22,
	'V', 'L', 'A', 'N', 
	'_', 'P', 'L', 'U', 'S', '_', 'V', 'I', 'D',
	'_', 'N', 'O', '_', 'P', 'A', 'D', 0,
	VLAN_NAME_TYPE_RAW_PLUS_VID, 15,
	'D', 'E', 'V',
	'_', 'P', 'L', 'U', 'S', '_', 'V', 'I', 'D',
	0,
	VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD, 20,
	'D', 'E', 'V',
	'_', 'P', 'L', 'U', 'S', '_', 'V', 'I', 'D',
	'_', 'N', 'O', '_', 'P', 'A', 'D', 0,
};

static const char conf_file_name[] = "/proc/net/vlan/config";

int vconfig_main(int argc, char **argv)
{
	struct vlan_ioctl_args ifr;
	const char *p;
	int fd;

	if (argc < 3) {
		bb_show_usage();
	}

	/* Don't bother closing the filedes.  It will be closed on cleanup. */
	if (open(conf_file_name, O_RDONLY) < 0) { /* Is 802.1q is present? */
	    bb_perror_msg_and_die("open %s", conf_file_name);
	}

	memset(&ifr, 0, sizeof(struct vlan_ioctl_args));

	++argv;
	p = xfind_str(cmds+2, *argv);
	ifr.cmd = *p;
	if (argc != p[-1]) {
		bb_show_usage();
	}

	if (ifr.cmd == SET_VLAN_NAME_TYPE_CMD) { /* set_name_type */
		ifr.u.name_type = *xfind_str(name_types+1, argv[1]);
	} else {
		if (strlen(argv[1]) >= IF_NAMESIZE) {
			bb_error_msg_and_die("if_name >= %d chars\n", IF_NAMESIZE);
		}
		strcpy(ifr.device1, argv[1]);
		p = argv[2];

		/* I suppose one could try to combine some of the function calls below,
		 * since ifr.u.flag, ifr.u.VID, and ifr.u.skb_priority are all same-sized
		 * (unsigned) int members of a unions.  But because of the range checking,
		 * doing so wouldn't save that much space and would also make maintainence
		 * more of a pain. */
		if (ifr.cmd == SET_VLAN_FLAG_CMD) { /* set_flag */
			ifr.u.flag = bb_xgetularg10_bnd(p, 0, 1);
		} else if (ifr.cmd == ADD_VLAN_CMD) { /* add */
			ifr.u.VID = bb_xgetularg10_bnd(p, 0, VLAN_GROUP_ARRAY_LEN-1);
		} else if (ifr.cmd != DEL_VLAN_CMD) { /* set_{egress|ingress}_map */
			ifr.u.skb_priority = bb_xgetularg10_bnd(p, 0, ULONG_MAX);
			ifr.vlan_qos = bb_xgetularg10_bnd(argv[3], 0, 7);
		}
	}

	if (((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		|| (ioctl(fd, SIOCSIFVLAN, &ifr) < 0)
		) {
		bb_perror_msg_and_die("socket or ioctl error for %s", *argv);
	}

	return 0;
}

