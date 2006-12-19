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
#if (defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || defined(_NEWLIB_VERSION)
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif

#include "common.h"


int raw_socket(int ifindex)
{
	int fd;
	struct sockaddr_ll sock;

	DEBUG("Opening raw socket on ifindex %d", ifindex);
	fd = xsocket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));

	sock.sll_family = AF_PACKET;
	sock.sll_protocol = htons(ETH_P_IP);
	sock.sll_ifindex = ifindex;
	xbind(fd, (struct sockaddr *) &sock, sizeof(sock));

	return fd;
}
