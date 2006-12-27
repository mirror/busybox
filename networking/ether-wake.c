/* vi: set sw=4 ts=4: */
/*
 * ether-wake.c - Send a magic packet to wake up sleeping machines.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Author:      Donald Becker, http://www.scyld.com/"; http://www.scyld.com/wakeonlan.html
 * Busybox port: Christian Volkmann <haveaniceday@online.de>
 *               Used version of ether-wake.c: v1.09 11/12/2003 Donald Becker, http://www.scyld.com/";
 */

/* full usage according Donald Becker
 * usage: ether-wake [-i <ifname>] [-p aa:bb:cc:dd[:ee:ff]] 00:11:22:33:44:55\n"
 *
 *	This program generates and transmits a Wake-On-LAN (WOL)\n"
 *	\"Magic Packet\", used for restarting machines that have been\n"
 *	soft-powered-down (ACPI D3-warm state).\n"
 *	It currently generates the standard AMD Magic Packet format, with\n"
 *	an optional password appended.\n"
 *
 *	The single required parameter is the Ethernet MAC (station) address\n"
 *	of the machine to wake or a host ID with known NSS 'ethers' entry.\n"
 *	The MAC address may be found with the 'arp' program while the target\n"
 *	machine is awake.\n"
 *
 *	Options:\n"
 *		-b	Send wake-up packet to the broadcast address.\n"
 *		-D	Increase the debug level.\n"
 *		-i ifname	Use interface IFNAME instead of the default 'eth0'.\n"
 *		-p <pw>		Append the four or six byte password PW to the packet.\n"
 *					A password is only required for a few adapter types.\n"
 *					The password may be specified in ethernet hex format\n"
 *					or dotted decimal (Internet address)\n"
 *		-p 00:22:44:66:88:aa\n"
 *		-p 192.168.1.1\n";
 *
 *
 *	This program generates and transmits a Wake-On-LAN (WOL) "Magic Packet",
 *	used for restarting machines that have been soft-powered-down
 *	(ACPI D3-warm state).  It currently generates the standard AMD Magic Packet
 *	format, with an optional password appended.
 *
 *	This software may be used and distributed according to the terms
 *	of the GNU Public License, incorporated herein by reference.
 *	Contact the author for use under other terms.
 *
 *	This source file was originally part of the network tricks package, and
 *	is now distributed to support the Scyld Beowulf system.
 *	Copyright 1999-2003 Donald Becker and Scyld Computing Corporation.
 *
 *	The author may be reached as becker@scyld, or C/O
 *	 Scyld Computing Corporation
 *	 914 Bay Ridge Road, Suite 220
 *	 Annapolis MD 21403
 *
 *   Notes:
 *   On some systems dropping root capability allows the process to be
 *   dumped, traced or debugged.
 *   If someone traces this program, they get control of a raw socket.
 *   Linux handles this safely, but beware when porting this program.
 *
 *   An alternative to needing 'root' is using a UDP broadcast socket, however
 *   doing so only works with adapters configured for unicast+broadcast Rx
 *   filter.  That configuration consumes more power.
*/


#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#include <linux/if.h>

#include "busybox.h"

/* Note: PF_INET, SOCK_DGRAM, IPPROTO_UDP would allow SIOCGIFHWADDR to
 * work as non-root, but we need SOCK_PACKET to specify the Ethernet
 * destination address.
 */
#ifdef PF_PACKET
# define whereto_t sockaddr_ll
# define make_socket() xsocket(PF_PACKET, SOCK_RAW, 0)
#else
# define whereto_t sockaddr
# define make_socket() xsocket(AF_INET, SOCK_PACKET, SOCK_PACKET)
#endif

#ifdef DEBUG
# define bb_debug_msg(fmt, args...) fprintf(stderr, fmt, ## args)
void bb_debug_dump_packet(unsigned char *outpack, int pktsize)
{
	int i;
	printf("packet dump:\n");
	for (i = 0; i < pktsize; ++i) {
		printf("%2.2x ", outpack[i]);
		if (i % 20 == 19) puts("");
	}
	printf("\n\n");
}
#else
# define bb_debug_msg(fmt, args...)
# define bb_debug_dump_packet(outpack, pktsize)
#endif

static inline void get_dest_addr(const char *arg, struct ether_addr *eaddr);
static inline int get_fill(unsigned char *pkt, struct ether_addr *eaddr, int broadcast);
static inline int get_wol_pw(const char *ethoptarg, unsigned char *wol_passwd);

int ether_wake_main(int argc, char *argv[])
{
	char *ifname = "eth0", *pass = NULL;
	unsigned long flags;
	unsigned char wol_passwd[6];
	int wol_passwd_sz = 0;

	int s;						/* Raw socket */
	int pktsize;
	unsigned char outpack[1000];

	struct ether_addr eaddr;
	struct whereto_t whereto;	/* who to wake up */

	/* handle misc user options */
	flags = getopt32(argc, argv, "bi:p:", &ifname, &pass);
	if (optind == argc)
		bb_show_usage();
	if (pass)
		wol_passwd_sz = get_wol_pw(pass, wol_passwd);

	/* create the raw socket */
	s = make_socket();

	/* now that we have a raw socket we can drop root */
	xsetuid(getuid());

	/* look up the dest mac address */
	get_dest_addr(argv[optind], &eaddr);

	/* fill out the header of the packet */
	pktsize = get_fill(outpack, &eaddr, flags /*& 1 [OPT_BROADCAST]*/);

	bb_debug_dump_packet(outpack, pktsize);

	/* Fill in the source address, if possible. */
#ifdef __linux__
	{
		struct ifreq if_hwaddr;

		strncpy(if_hwaddr.ifr_name, ifname, sizeof(if_hwaddr.ifr_name));
		if (ioctl(s, SIOCGIFHWADDR, &if_hwaddr) < 0)
			bb_perror_msg_and_die("SIOCGIFHWADDR on %s failed", ifname);

		memcpy(outpack+6, if_hwaddr.ifr_hwaddr.sa_data, 6);

# ifdef DEBUG
		{
			unsigned char *hwaddr = if_hwaddr.ifr_hwaddr.sa_data;
			printf("The hardware address (SIOCGIFHWADDR) of %s is type %d  "
				   "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n\n", ifname,
				   if_hwaddr.ifr_hwaddr.sa_family, hwaddr[0], hwaddr[1],
				   hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
		}
# endif
	}
#endif /* __linux__ */

	bb_debug_dump_packet(outpack, pktsize);

	/* append the password if specified */
	if (wol_passwd_sz > 0) {
		memcpy(outpack+pktsize, wol_passwd, wol_passwd_sz);
		pktsize += wol_passwd_sz;
	}

	bb_debug_dump_packet(outpack, pktsize);

	/* This is necessary for broadcasts to work */
	if (flags /*& 1 [OPT_BROADCAST]*/) {
		if (setsockopt_broadcast(s) < 0)
			bb_perror_msg("SO_BROADCAST");
	}

#if defined(PF_PACKET)
	{
		struct ifreq ifr;
		strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		if (ioctl(s, SIOCGIFINDEX, &ifr) == -1)
			bb_perror_msg_and_die("SIOCGIFINDEX");
		memset(&whereto, 0, sizeof(whereto));
		whereto.sll_family = AF_PACKET;
		whereto.sll_ifindex = ifr.ifr_ifindex;
		/* The manual page incorrectly claims the address must be filled.
		   We do so because the code may change to match the docs. */
		whereto.sll_halen = ETH_ALEN;
		memcpy(whereto.sll_addr, outpack, ETH_ALEN);
	}
#else
	whereto.sa_family = 0;
	strcpy(whereto.sa_data, ifname);
#endif

	if (sendto(s, outpack, pktsize, 0, (struct sockaddr *)&whereto, sizeof(whereto)) < 0)
		bb_perror_msg(bb_msg_write_error);

	close(s);

	return EXIT_SUCCESS;
}

/* Convert the host ID string to a MAC address.
 * The string may be a:
 *    Host name
 *    IP address string
 *    MAC address string
*/
static inline void get_dest_addr(const char *hostid, struct ether_addr *eaddr)
{
	struct ether_addr *eap;

	eap = ether_aton(hostid);
	if (eap) {
		*eaddr = *eap;
		bb_debug_msg("The target station address is %s\n\n", ether_ntoa(eaddr));
#if !defined(__UCLIBC__)
	} else if (ether_hostton(hostid, eaddr) == 0) {
		bb_debug_msg("Station address for hostname %s is %s\n\n", hostid, ether_ntoa(eaddr));
#else
# warning Need to implement ether_hostton() for uClibc
#endif
	} else
		bb_show_usage();
}

static inline int get_fill(unsigned char *pkt, struct ether_addr *eaddr, int broadcast)
{
	int offset, i;
	unsigned char *station_addr = eaddr->ether_addr_octet;

	if (broadcast)
		memset(pkt+0, 0xff, 6);
	else
		memcpy(pkt, station_addr, 6);
	memcpy(pkt+6, station_addr, 6);
	pkt[12] = 0x08;				/* Or 0x0806 for ARP, 0x8035 for RARP */
	pkt[13] = 0x42;
	offset = 14;

	memset(pkt+offset, 0xff, 6);
	offset += 6;

	for (i = 0; i < 16; ++i) {
		memcpy(pkt+offset, station_addr, 6);
		offset += 6;
	}

	return offset;
}

static inline int get_wol_pw(const char *ethoptarg, unsigned char *wol_passwd)
{
	int passwd[6];
	int byte_cnt, i;

	/* handle MAC format */
	byte_cnt = sscanf(ethoptarg, "%2x:%2x:%2x:%2x:%2x:%2x",
	                  &passwd[0], &passwd[1], &passwd[2],
	                  &passwd[3], &passwd[4], &passwd[5]);
	/* handle IP format */
	if (byte_cnt < 4)
		byte_cnt = sscanf(ethoptarg, "%d.%d.%d.%d",
		                  &passwd[0], &passwd[1], &passwd[2], &passwd[3]);
	if (byte_cnt < 4) {
		bb_error_msg("cannot read Wake-On-LAN pass");
		return 0;
	}

	for (i = 0; i < byte_cnt; ++i)
		wol_passwd[i] = passwd[i];

	bb_debug_msg("password: %2.2x %2.2x %2.2x %2.2x (%d)\n\n",
	             wol_passwd[0], wol_passwd[1], wol_passwd[2], wol_passwd[3],
	             byte_cnt);

	return byte_cnt;
}
