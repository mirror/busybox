/* vi: set sw=4 ts=4: */
/*
 * arping.c - Ping hosts by ARP requests/replies
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Author:	Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
 * Busybox port: Nick Fedchik <nick@fedchik.org.ua>
 */

#include <sys/ioctl.h>
#include <signal.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>

#include "busybox.h"

static struct in_addr src;
static struct in_addr dst;
static struct sockaddr_ll me;
static struct sockaddr_ll he;
static struct timeval last;

enum cfg_e {
	dad = 1,
	unsolicited = 2,
	advert = 4,
	quiet = 8,
	quit_on_reply = 16,
	broadcast_only = 32,
	unicasting = 64
};
static int cfg;

static int s;
static int count = -1;
static int timeout;
static int sent;
static int brd_sent;
static int received;
static int brd_recv;
static int req_recv;


#define MS_TDIFF(tv1,tv2) ( ((tv1).tv_sec-(tv2).tv_sec)*1000 + \
			   ((tv1).tv_usec-(tv2).tv_usec)/1000 )
#if 0
static void set_signal(int signo, void (*handler) (void))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = (void (*)(int)) handler;
	sa.sa_flags = SA_RESTART;
	sigaction(signo, &sa, NULL);
}
#endif

static int send_pack(int sock, struct in_addr *src_addr,
					 struct in_addr *dst_addr, struct sockaddr_ll *ME,
					 struct sockaddr_ll *HE)
{
	int err;
	struct timeval now;
	RESERVE_CONFIG_UBUFFER(buf, 256);
	struct arphdr *ah = (struct arphdr *) buf;
	unsigned char *p = (unsigned char *) (ah + 1);

	ah->ar_hrd = htons(ME->sll_hatype);
	ah->ar_hrd = htons(ARPHRD_ETHER);
	ah->ar_pro = htons(ETH_P_IP);
	ah->ar_hln = ME->sll_halen;
	ah->ar_pln = 4;
	ah->ar_op = cfg&advert ? htons(ARPOP_REPLY) : htons(ARPOP_REQUEST);

	memcpy(p, &ME->sll_addr, ah->ar_hln);
	p += ME->sll_halen;

	memcpy(p, src_addr, 4);
	p += 4;

	if (cfg&advert)
		memcpy(p, &ME->sll_addr, ah->ar_hln);
	else
		memcpy(p, &HE->sll_addr, ah->ar_hln);
	p += ah->ar_hln;

	memcpy(p, dst_addr, 4);
	p += 4;

	gettimeofday(&now, NULL);
	err = sendto(sock, buf, p - buf, 0, (struct sockaddr *) HE, sizeof(*HE));
	if (err == p - buf) {
		last = now;
		sent++;
		if (!(cfg&unicasting))
			brd_sent++;
	}
	RELEASE_CONFIG_BUFFER(buf);
	return err;
}

static void finish(void)
{
	if (!(cfg&quiet)) {
		printf("Sent %d probes (%d broadcast(s))\n"
			"Received %d repl%s",
			sent, brd_sent,
			received, (received > 1) ? "ies" : "y");
		if (brd_recv || req_recv) {
			printf(" (");
			if (req_recv)
				printf("%d request(s)", req_recv);
			if (brd_recv)
				printf("%s%d broadcast(s)", req_recv ? ", " : "", brd_recv);
			putchar(')');
		}
		putchar('\n');
		fflush(stdout);
	}
	if (cfg&dad)
		exit(!!received);
	if (cfg&unsolicited)
		exit(0);
	exit(!received);
}

static void catcher(void)
{
	struct timeval tv;
	static struct timeval start;

	gettimeofday(&tv, NULL);

	if (start.tv_sec == 0)
		start = tv;

	if (count-- == 0
		|| (timeout && MS_TDIFF(tv, start) > timeout * 1000 + 500))
		finish();

	if (last.tv_sec == 0 || MS_TDIFF(tv, last) > 500) {
		send_pack(s, &src, &dst, &me, &he);
		if (count == 0 && cfg&unsolicited)
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
	if (FROM->sll_pkttype != PACKET_HOST &&
		FROM->sll_pkttype != PACKET_BROADCAST &&
		FROM->sll_pkttype != PACKET_MULTICAST)
		return 0;

	/* Only these types are recognised */
	if (ah->ar_op != htons(ARPOP_REQUEST) && ah->ar_op != htons(ARPOP_REPLY))
		return 0;

	/* ARPHRD check and this darned FDDI hack here :-( */
	if (ah->ar_hrd != htons(FROM->sll_hatype) &&
		(FROM->sll_hatype != ARPHRD_FDDI
		 || ah->ar_hrd != htons(ARPHRD_ETHER)))
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
	if (!(cfg&dad)) {
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
	if (!(cfg&quiet)) {
		int s_printed = 0;
		struct timeval tv;

		gettimeofday(&tv, NULL);

		printf("%s %s from %s [%s]",
			FROM->sll_pkttype == PACKET_HOST ? "Unicast" : "Broadcast",
			ah->ar_op == htons(ARPOP_REPLY) ? "reply" : "request",
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

		if (last.tv_sec) {
			long usecs = (tv.tv_sec - last.tv_sec) * 1000000 +
				tv.tv_usec - last.tv_usec;
			long msecs = (usecs + 500) / 1000;

			usecs -= msecs * 1000 - 500;
			printf(" %ld.%03ldms\n", msecs, usecs);
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
	if (cfg&quit_on_reply)
		finish();
	if (!(cfg&broadcast_only)) {
		memcpy(he.sll_addr, p, me.sll_halen);
		cfg |= unicasting;
	}
	return 1;
}

int arping_main(int argc, char **argv)
{
	char *device = "eth0";
	int ifindex;
	char *source = NULL;
	char *target;

	s = socket(PF_PACKET, SOCK_DGRAM, 0);
	ifindex = errno;

	// Drop suid root privileges
	xsetuid(getuid());

	{
		unsigned long opt;
		char *_count, *_timeout, *_device;

		/* Dad also sets quit_on_reply.
		 * Advert also sets unsolicited.
		 */
		bb_opt_complementally = "Df:AU";
		opt = bb_getopt_ulflags(argc, argv, "DUAqfbc:w:i:s:",
						&_count, &_timeout, &_device);
		cfg |= opt & 63; /* set respective flags */
		if (opt & 64) /* count */
			count = atoi(_count);
		if (opt & 128) /* timeout */
			timeout = atoi(_timeout);
		if (opt & 256) { /* interface */
			if (strlen(_device) > IF_NAMESIZE) {
				bb_error_msg_and_die("Interface name `%s' must be less than %d",
								_device, IF_NAMESIZE);
			}
			device = _device;
		}
		if (opt & 512) /* source */
			source = optarg;
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		bb_show_usage();

	target = *argv;


	if (s < 0) {
		bb_default_error_retval = ifindex;
		bb_perror_msg_and_die("socket");
	}
	bb_default_error_retval = 2;

	{
		struct ifreq ifr;

		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, device, IFNAMSIZ - 1);
		if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
			bb_error_msg_and_die("Interface %s not found", device);
		}
		ifindex = ifr.ifr_ifindex;

		if (ioctl(s, SIOCGIFFLAGS, (char *) &ifr)) {
			bb_error_msg_and_die("SIOCGIFFLAGS");
		}
		if (!(ifr.ifr_flags & IFF_UP)) {
			bb_error_msg_and_die("Interface %s is down", device);
		}
		if (ifr.ifr_flags & (IFF_NOARP | IFF_LOOPBACK)) {
			bb_error_msg("Interface %s is not ARPable", device);
			exit(cfg&dad ? 0 : 2);
		}
	}

	if (!inet_aton(target, &dst)) {
		struct hostent *hp;

		hp = gethostbyname2(target, AF_INET);
		if (!hp) {
			bb_error_msg_and_die("invalid or unknown target %s", target);
		}
		memcpy(&dst, hp->h_addr, 4);
	}

	if (source && !inet_aton(source, &src)) {
		bb_error_msg_and_die("invalid source address %s", source);
	}

	if (!(cfg&dad) && cfg&unsolicited && src.s_addr == 0)
		src = dst;

	if (!(cfg&dad) || src.s_addr) {
		struct sockaddr_in saddr;
		int probe_fd = socket(AF_INET, SOCK_DGRAM, 0); /* maybe use bb_xsocket? */

		if (probe_fd < 0) {
			bb_error_msg_and_die("socket");
		}
		if (device) {
			if (setsockopt
				(probe_fd, SOL_SOCKET, SO_BINDTODEVICE, device,
				 strlen(device) + 1) == -1)
				bb_error_msg("WARNING: interface %s is ignored", device);
		}
		memset(&saddr, 0, sizeof(saddr));
		saddr.sin_family = AF_INET;
		if (src.s_addr) {
			saddr.sin_addr = src;
			if (bind(probe_fd, (struct sockaddr *) &saddr, sizeof(saddr)) == -1) {
				bb_error_msg_and_die("bind");
			}
		} else if (!(cfg&dad)) {
			int on = 1;
			socklen_t alen = sizeof(saddr);

			saddr.sin_port = htons(1025);
			saddr.sin_addr = dst;

			if (setsockopt
				(probe_fd, SOL_SOCKET, SO_DONTROUTE, (char *) &on,
				 sizeof(on)) == -1)
				bb_perror_msg("WARNING: setsockopt(SO_DONTROUTE)");
			if (connect(probe_fd, (struct sockaddr *) &saddr, sizeof(saddr))
				== -1) {
				bb_error_msg_and_die("connect");
			}
			if (getsockname(probe_fd, (struct sockaddr *) &saddr, &alen) ==
				-1) {
				bb_error_msg_and_die("getsockname");
			}
			src = saddr.sin_addr;
		}
		close(probe_fd);
	};

	me.sll_family = AF_PACKET;
	me.sll_ifindex = ifindex;
	me.sll_protocol = htons(ETH_P_ARP);
	if (bind(s, (struct sockaddr *) &me, sizeof(me)) == -1) {
		bb_error_msg_and_die("bind");
	}

	{
		socklen_t alen = sizeof(me);

		if (getsockname(s, (struct sockaddr *) &me, &alen) == -1) {
			bb_error_msg_and_die("getsockname");
		}
	}
	if (me.sll_halen == 0) {
		bb_error_msg("Interface \"%s\" is not ARPable (no ll address)", device);
		exit(cfg&dad ? 0 : 2);
	}
	he = me;
	memset(he.sll_addr, -1, he.sll_halen);

	if (!(cfg&quiet)) {
		printf("ARPING to %s from %s via %s\n",
			inet_ntoa(dst), inet_ntoa(src),
			device ? device : "unknown");
	}

	if (!src.s_addr && !(cfg&dad)) {
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

	while (1) {
		sigset_t sset, osset;
		RESERVE_CONFIG_UBUFFER(packet, 4096);
		struct sockaddr_ll from;
		socklen_t alen = sizeof(from);
		int cc;

		if ((cc = recvfrom(s, packet, 4096, 0,
						   (struct sockaddr *) &from, &alen)) < 0) {
			perror("recvfrom");
			continue;
		}
		sigemptyset(&sset);
		sigaddset(&sset, SIGALRM);
		sigaddset(&sset, SIGINT);
		sigprocmask(SIG_BLOCK, &sset, &osset);
		recv_pack(packet, cc, &from);
		sigprocmask(SIG_SETMASK, &osset, NULL);
		RELEASE_CONFIG_BUFFER(packet);
	}
}
