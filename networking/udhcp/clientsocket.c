/* vi: set sw=4 ts=4: */
/*
 * clientsocket.c -- DHCP client socket creation
 *
 * udhcp client
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <features.h>
#include <asm/types.h>
#if (defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || defined(_NEWLIB_VERSION)
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif
#include <linux/filter.h>

#include "common.h"
#include "dhcpd.h"
#include "dhcpc.h"

int raw_socket(int ifindex)
{
	int fd;
	struct sockaddr_ll sock;

	/*
	 * Comment:
	 *
	 *	I've selected not to see LL header, so BPF doesn't see it, too.
	 *	The filter may also pass non-IP and non-ARP packets, but we do
	 *	a more complete check when receiving the message in userspace.
	 *
	 * and filter shamelessly stolen from:
	 *
	 *	http://www.flamewarmaster.de/software/dhcpclient/
	 *
	 * There are a few other interesting ideas on that page (look under
	 * "Motivation").  Use of netlink events is most interesting.  Think
	 * of various network servers listening for events and reconfiguring.
	 * That would obsolete sending HUP signals and/or make use of restarts.
	 *
	 * Copyright: 2006, 2007 Stefan Rompf <sux@loplof.de>.
	 * License: GPL v2.
	 *
	 * TODO: make conditional?
	 */
#define SERVER_AND_CLIENT_PORTS  ((67 << 16) + 68)
	static const struct sock_filter filter_instr[] = {
		/* check for udp */
		BPF_STMT(BPF_LD|BPF_B|BPF_ABS, 9),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, IPPROTO_UDP, 2, 0),     /* L5, L1, is UDP? */
		/* ugly check for arp on ethernet-like and IPv4 */
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS, 2),                      /* L1: */
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0x08000604, 3, 4),      /* L3, L4 */
		/* skip IP header */
		BPF_STMT(BPF_LDX|BPF_B|BPF_MSH, 0),                     /* L5: */
		/* check udp source and destination ports */
		BPF_STMT(BPF_LD|BPF_W|BPF_IND, 0),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, SERVER_AND_CLIENT_PORTS, 0, 1),	/* L3, L4 */
		/* returns */
		BPF_STMT(BPF_RET|BPF_K, 0x0fffffff ),                   /* L3: pass */
		BPF_STMT(BPF_RET|BPF_K, 0),                             /* L4: reject */
	};
	static const struct sock_fprog filter_prog = {
		.len = sizeof(filter_instr) / sizeof(filter_instr[0]),
		/* casting const away: */
		.filter = (struct sock_filter *) filter_instr,
	};

	DEBUG("opening raw socket on ifindex %d", ifindex);

	fd = xsocket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	DEBUG("got raw socket fd %d", fd);

	if (SERVER_PORT == 67 && CLIENT_PORT == 68) {
		/* Use only if standard ports are in use */
		/* Ignoring error (kernel may lack support for this) */
		if (setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, &filter_prog,
				sizeof(filter_prog)) >= 0)
			DEBUG("attached filter to raw socket fd %d", fd);
	}

	sock.sll_family = AF_PACKET;
	sock.sll_protocol = htons(ETH_P_IP);
	sock.sll_ifindex = ifindex;
	xbind(fd, (struct sockaddr *) &sock, sizeof(sock));
	DEBUG("bound to raw socket fd %d", fd);

	return fd;
}
