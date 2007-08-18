/* vi: set sw=4 ts=4: */
/*
 * arping.c - Ping hosts by ARP requests/replies
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Author:	Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
 * Busybox port: Nick Fedchik <nick@fedchik.org.ua>
 */

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>

#include "libbb.h"

/* We don't expect to see 1000+ seconds delay, unsigned is enough */
#define MONOTONIC_US() ((unsigned)monotonic_us())

static struct in_addr src;
static struct in_addr dst;
static struct sockaddr_ll me;
static struct sockaddr_ll he;
static unsigned last;

enum {
	DAD = 1,
	UNSOLICITED = 2,
	ADVERT = 4,
	QUIET = 8,
	QUIT_ON_REPLY = 16,
	BCAST_ONLY = 32,
	UNICASTING = 64
};

static int sock;
static unsigned count = UINT_MAX;
static unsigned timeout_us;
static unsigned sent;
static unsigned brd_sent;
static unsigned received;
static unsigned brd_recv;
static unsigned req_recv;

static int send_pack(struct in_addr *src_addr,
			struct in_addr *dst_addr, struct sockaddr_ll *ME,
			struct sockaddr_ll *HE)
{
	int err;
	unsigned now;
	unsigned char buf[256];
	struct arphdr *ah = (struct arphdr *) buf;
	unsigned char *p = (unsigned char *) (ah + 1);

	ah->ar_hrd = htons(ME->sll_hatype);
	ah->ar_hrd = htons(ARPHRD_ETHER);
	ah->ar_pro = htons(ETH_P_IP);
	ah->ar_hln = ME->sll_halen;
	ah->ar_pln = 4;
	ah->ar_op = option_mask32 & ADVERT ? htons(ARPOP_REPLY) : htons(ARPOP_REQUEST);

	memcpy(p, &ME->sll_addr, ah->ar_hln);
	p += ME->sll_halen;

	memcpy(p, src_addr, 4);
	p += 4;

	if (option_mask32 & ADVERT)
		memcpy(p, &ME->sll_addr, ah->ar_hln);
	else
		memcpy(p, &HE->sll_addr, ah->ar_hln);
	p += ah->ar_hln;

	memcpy(p, dst_addr, 4);
	p += 4;

	now = MONOTONIC_US();
	err = sendto(sock, buf, p - buf, 0, (struct sockaddr *) HE, sizeof(*HE));
	if (err == p - buf) {
		last = now;
		sent++;
		if (!(option_mask32 & UNICASTING))
			brd_sent++;
	}
	return err;
}

static void finish(void) ATTRIBUTE_NORETURN;
static void finish(void)
{
	if (!(option_mask32 & QUIET)) {
		printf("Sent %u probe(s) (%u broadcast(s))\n"
			"Received %u repl%s"
			" (%u request(s), %u broadcast(s))\n",
			sent, brd_sent,
			received, (received == 1) ? "ies" : "y",
			req_recv, brd_recv);
	}
	if (option_mask32 & DAD)
		exit(!!received);
	if (option_mask32 & UNSOLICITED)
		exit(0);
	exit(!received);
}

static void catcher(void)
{
	static unsigned start;

	unsigned now;

	now = MONOTONIC_US();
	if (start == 0)
		start = now;

	if (count == 0 || (timeout_us && (now - start) > (timeout_us + 500000)))
		finish();

	count--;

	if (last == 0 || (now - last) > 500000) {
		send_pack(&src, &dst, &me, &he);
		if (count == 0 && (option_mask32 & UNSOLICITED))
			finish();
	}
	alarm(1);
}

static int recv_pack(unsigned char *buf, int len, struct sockaddr_ll *FROM)
{
	struct arphdr *ah = (struct arphdr *) buf;
	unsigned char *p = (unsigned char *) (ah + 1);
	struct in_addr src_ip, dst_ip;

	/* Filter out wild packets */
	if (FROM->sll_pkttype != PACKET_HOST
	 && FROM->sll_pkttype != PACKET_BROADCAST
	 && FROM->sll_pkttype != PACKET_MULTICAST)
		return 0;

	/* Only these types are recognised */
	if (ah->ar_op != htons(ARPOP_REQUEST) && ah->ar_op != htons(ARPOP_REPLY))
		return 0;

	/* ARPHRD check and this darned FDDI hack here :-( */
	if (ah->ar_hrd != htons(FROM->sll_hatype)
	 && (FROM->sll_hatype != ARPHRD_FDDI || ah->ar_hrd != htons(ARPHRD_ETHER)))
		return 0;

	/* Protocol must be IP. */
	if (ah->ar_pro != htons(ETH_P_IP))
		return 0;
	if (ah->ar_pln != 4)
		return 0;
	if (ah->ar_hln != me.sll_halen)
		return 0;
	if (len < sizeof(*ah) + 2 * (4 + ah->ar_hln))
		return 0;
	memcpy(&src_ip, p + ah->ar_hln, 4);
	memcpy(&dst_ip, p + ah->ar_hln + 4 + ah->ar_hln, 4);
	if (!(option_mask32 & DAD)) {
		if (src_ip.s_addr != dst.s_addr)
			return 0;
		if (src.s_addr != dst_ip.s_addr)
			return 0;
		if (memcmp(p + ah->ar_hln + 4, &me.sll_addr, ah->ar_hln))
			return 0;
	} else {
		/* DAD packet was:
		   src_ip = 0 (or some src)
		   src_hw = ME
		   dst_ip = tested address
		   dst_hw = <unspec>

		   We fail, if receive request/reply with:
		   src_ip = tested_address
		   src_hw != ME
		   if src_ip in request was not zero, check
		   also that it matches to dst_ip, otherwise
		   dst_ip/dst_hw do not matter.
		 */
		if (src_ip.s_addr != dst.s_addr)
			return 0;
		if (memcmp(p, &me.sll_addr, me.sll_halen) == 0)
			return 0;
		if (src.s_addr && src.s_addr != dst_ip.s_addr)
			return 0;
	}
	if (!(option_mask32 & QUIET)) {
		int s_printed = 0;

		printf("%scast re%s from %s [%s]",
			FROM->sll_pkttype == PACKET_HOST ? "Uni" : "Broad",
			ah->ar_op == htons(ARPOP_REPLY) ? "ply" : "quest",
			inet_ntoa(src_ip),
			ether_ntoa((struct ether_addr *) p));
		if (dst_ip.s_addr != src.s_addr) {
			printf("for %s ", inet_ntoa(dst_ip));
			s_printed = 1;
		}
		if (memcmp(p + ah->ar_hln + 4, me.sll_addr, ah->ar_hln)) {
			if (!s_printed)
				printf("for ");
			printf("[%s]",
				ether_ntoa((struct ether_addr *) p + ah->ar_hln + 4));
		}

		if (last) {
			printf(" %u.%03ums\n", last / 1000, last % 1000);
		} else {
			printf(" UNSOLICITED?\n");
		}
		fflush(stdout);
	}
	received++;
	if (FROM->sll_pkttype != PACKET_HOST)
		brd_recv++;
	if (ah->ar_op == htons(ARPOP_REQUEST))
		req_recv++;
	if (option_mask32 & QUIT_ON_REPLY)
		finish();
	if (!(option_mask32 & BCAST_ONLY)) {
		memcpy(he.sll_addr, p, me.sll_halen);
		option_mask32 |= UNICASTING;
	}
	return 1;
}

int arping_main(int argc, char **argv);
int arping_main(int argc, char **argv)
{
	const char *device = "eth0";
	int ifindex;
	char *source = NULL;
	char *target;
	unsigned char *packet;

	sock = xsocket(PF_PACKET, SOCK_DGRAM, 0);

	// Drop suid root privileges
	xsetuid(getuid());

	{
		unsigned opt;
		char *str_count, *str_timeout;

		/* Dad also sets quit_on_reply.
		 * Advert also sets unsolicited.
		 */
		opt_complementary = "=1:Df:AU";
		opt = getopt32(argv, "DUAqfbc:w:I:s:",
				&str_count, &str_timeout, &device, &source);
		if (opt & 0x40) /* -c: count */
			count = xatou(str_count);
		if (opt & 0x80) /* -w: timeout */
			timeout_us = xatou_range(str_timeout, 0, INT_MAX/2000000) * 1000000;
		//if (opt & 0x100) /* -I: interface */
		if (strlen(device) >= IF_NAMESIZE) {
			bb_error_msg_and_die("interface name '%s' is too long",
							device);
		}
		//if (opt & 0x200) /* -s: source */
		option_mask32 &= 0x3f; /* set respective flags */
	}

	target = argv[optind];

	xfunc_error_retval = 2;

	{
		struct ifreq ifr;

		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, device, IFNAMSIZ - 1);
		ioctl_or_perror_and_die(sock, SIOCGIFINDEX, &ifr, "interface %s not found", device);
		ifindex = ifr.ifr_ifindex;

		xioctl(sock, SIOCGIFFLAGS, (char *) &ifr);

		if (!(ifr.ifr_flags & IFF_UP)) {
			bb_error_msg_and_die("interface %s is down", device);
		}
		if (ifr.ifr_flags & (IFF_NOARP | IFF_LOOPBACK)) {
			bb_error_msg("interface %s is not ARPable", device);
			return (option_mask32 & DAD ? 0 : 2);
		}
	}

	if (!inet_aton(target, &dst)) {
		len_and_sockaddr *lsa;
		lsa = xhost_and_af2sockaddr(target, 0, AF_INET);
		memcpy(&dst, &lsa->sin.sin_addr.s_addr, 4);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(lsa);
	}

	if (source && !inet_aton(source, &src)) {
		bb_error_msg_and_die("invalid source address %s", source);
	}

	if (!(option_mask32 & DAD) && (option_mask32 & UNSOLICITED) && src.s_addr == 0)
		src = dst;

	if (!(option_mask32 & DAD) || src.s_addr) {
		struct sockaddr_in saddr;
		int probe_fd = xsocket(AF_INET, SOCK_DGRAM, 0);

		if (device) {
			if (setsockopt(probe_fd, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device) + 1) == -1)
				bb_error_msg("warning: interface %s is ignored", device);
		}
		memset(&saddr, 0, sizeof(saddr));
		saddr.sin_family = AF_INET;
		if (src.s_addr) {
			saddr.sin_addr = src;
			xbind(probe_fd, (struct sockaddr *) &saddr, sizeof(saddr));
		} else if (!(option_mask32 & DAD)) {
			socklen_t alen = sizeof(saddr);

			saddr.sin_port = htons(1025);
			saddr.sin_addr = dst;

			if (setsockopt(probe_fd, SOL_SOCKET, SO_DONTROUTE, &const_int_1, sizeof(const_int_1)) == -1)
				bb_perror_msg("warning: setsockopt(SO_DONTROUTE)");
			xconnect(probe_fd, (struct sockaddr *) &saddr, sizeof(saddr));
			if (getsockname(probe_fd, (struct sockaddr *) &saddr, &alen) == -1) {
				bb_error_msg_and_die("getsockname");
			}
			src = saddr.sin_addr;
		}
		close(probe_fd);
	}

	me.sll_family = AF_PACKET;
	me.sll_ifindex = ifindex;
	me.sll_protocol = htons(ETH_P_ARP);
	xbind(sock, (struct sockaddr *) &me, sizeof(me));

	{
		socklen_t alen = sizeof(me);

		if (getsockname(sock, (struct sockaddr *) &me, &alen) == -1) {
			bb_error_msg_and_die("getsockname");
		}
	}
	if (me.sll_halen == 0) {
		bb_error_msg("interface \"%s\" is not ARPable (no ll address)", device);
		return (option_mask32 & DAD ? 0 : 2);
	}
	he = me;
	memset(he.sll_addr, -1, he.sll_halen);

	if (!(option_mask32 & QUIET)) {
		printf("ARPING to %s from %s via %s\n",
			inet_ntoa(dst), inet_ntoa(src),
			device ? device : "unknown");
	}

	if (!src.s_addr && !(option_mask32 & DAD)) {
		bb_error_msg_and_die("no src address in the non-DAD mode");
	}

	{
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_flags = SA_RESTART;

		sa.sa_handler = (void (*)(int)) finish;
		sigaction(SIGINT, &sa, NULL);

		sa.sa_handler = (void (*)(int)) catcher;
		sigaction(SIGALRM, &sa, NULL);
	}

	catcher();

	packet = xmalloc(4096);
	while (1) {
		sigset_t sset, osset;
		struct sockaddr_ll from;
		socklen_t alen = sizeof(from);
		int cc;

		cc = recvfrom(sock, packet, 4096, 0, (struct sockaddr *) &from, &alen);
		if (cc < 0) {
			bb_perror_msg("recvfrom");
			continue;
		}
		sigemptyset(&sset);
		sigaddset(&sset, SIGALRM);
		sigaddset(&sset, SIGINT);
		sigprocmask(SIG_BLOCK, &sset, &osset);
		recv_pack(packet, cc, &from);
		sigprocmask(SIG_SETMASK, &osset, NULL);
	}
}
