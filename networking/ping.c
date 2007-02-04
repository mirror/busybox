/* vi: set sw=4 ts=4: */
/*
 * Mini ping implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * Adapted from the ping in netkit-base 0.10:
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
/* from ping6.c:
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * This version of ping is adapted from the ping in netkit-base 0.10,
 * which is:
 *
 * Original copyright notice is retained at the end of this file.
 *
 * This version is an adaptation of ping.c from busybox.
 * The code was modified by Bart Visscher <magick@linux-fan.com>
 */

#include <net/if.h>
#include <netinet/ip_icmp.h>
#include "busybox.h"

#if ENABLE_PING6
#include <netinet/icmp6.h>
/* I see RENUMBERED constants in bits/in.h - !!?
 * What a fuck is going on with libc? Is it a glibc joke? */
#ifdef IPV6_2292HOPLIMIT
#undef IPV6_HOPLIMIT
#define IPV6_HOPLIMIT IPV6_2292HOPLIMIT
#endif
#endif

enum {
	DEFDATALEN = 56,
	MAXIPLEN = 60,
	MAXICMPLEN = 76,
	MAXPACKET = 65468,
	MAX_DUP_CHK = (8 * 128),
	MAXWAIT = 10,
	PINGINTERVAL = 1, /* 1 second */
};

/* common routines */

static int in_cksum(unsigned short *buf, int sz)
{
	int nleft = sz;
	int sum = 0;
	unsigned short *w = buf;
	unsigned short ans = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(unsigned char *) (&ans) = *(unsigned char *) w;
		sum += ans;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	ans = ~sum;
	return ans;
}

#if !ENABLE_FEATURE_FANCY_PING

/* simple version */

static char *hostname;

static void noresp(int ign ATTRIBUTE_UNUSED)
{
	printf("No response from %s\n", hostname);
	exit(EXIT_FAILURE);
}

static void ping4(len_and_sockaddr *lsa)
{
	struct sockaddr_in pingaddr;
	struct icmp *pkt;
	int pingsock, c;
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];

	pingsock = create_icmp_socket();
	pingaddr = lsa->sin;

	pkt = (struct icmp *) packet;
	memset(pkt, 0, sizeof(packet));
	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, sizeof(packet));

	c = sendto(pingsock, packet, DEFDATALEN + ICMP_MINLEN, 0,
			   (struct sockaddr *) &pingaddr, sizeof(pingaddr));

	if (c < 0) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close(pingsock);
		bb_perror_msg_and_die("sendto");
	}

	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		socklen_t fromlen = sizeof(from);

		c = recvfrom(pingsock, packet, sizeof(packet), 0,
				(struct sockaddr *) &from, &fromlen);
		if (c < 0) {
			if (errno != EINTR)
				bb_perror_msg("recvfrom");
			continue;
		}
		if (c >= 76) {			/* ip + icmp */
			struct iphdr *iphdr = (struct iphdr *) packet;

			pkt = (struct icmp *) (packet + (iphdr->ihl << 2));	/* skip ip hdr */
			if (pkt->icmp_type == ICMP_ECHOREPLY)
				break;
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		close(pingsock);
}

#if ENABLE_PING6
static void ping6(len_and_sockaddr *lsa)
{
	struct sockaddr_in6 pingaddr;
	struct icmp6_hdr *pkt;
	int pingsock, c;
	int sockopt;
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];

	pingsock = create_icmp6_socket();
	pingaddr = lsa->sin6;

	pkt = (struct icmp6_hdr *) packet;
	memset(pkt, 0, sizeof(packet));
	pkt->icmp6_type = ICMP6_ECHO_REQUEST;

	sockopt = offsetof(struct icmp6_hdr, icmp6_cksum);
	setsockopt(pingsock, SOL_RAW, IPV6_CHECKSUM, &sockopt, sizeof(sockopt));

	c = sendto(pingsock, packet, DEFDATALEN + sizeof (struct icmp6_hdr), 0,
			   (struct sockaddr *) &pingaddr, sizeof(pingaddr));

	if (c < 0) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close(pingsock);
		bb_perror_msg_and_die("sendto");
	}

	/* listen for replies */
	while (1) {
		struct sockaddr_in6 from;
		socklen_t fromlen = sizeof(from);

		c = recvfrom(pingsock, packet, sizeof(packet), 0,
				(struct sockaddr *) &from, &fromlen);
		if (c < 0) {
			if (errno != EINTR)
				bb_perror_msg("recvfrom");
			continue;
		}
		if (c >= 8) {			/* icmp6_hdr */
			pkt = (struct icmp6_hdr *) packet;
			if (pkt->icmp6_type == ICMP6_ECHO_REPLY)
				break;
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		close(pingsock);
}
#endif

int ping_main(int argc, char **argv);
int ping_main(int argc, char **argv)
{
	len_and_sockaddr *lsa;
#if ENABLE_PING6
	sa_family_t af = AF_UNSPEC;
	while (++argv[0][0] == '-') {
		if (argv[0][1] == '4') {
			af = AF_INET;
			continue;
		}
		if (argv[0][1] == '6') {
			af = AF_INET6;
			continue;
		}
		bb_show_usage();
	}
#else
	argv++;
#endif

	hostname = *argv;
	if (!hostname)
		bb_show_usage();

#if ENABLE_PING6
	lsa = xhost_and_af2sockaddr(hostname, 0, af);
#else
	lsa = xhost_and_af2sockaddr(hostname, 0, AF_INET);
#endif
	/* Set timer _after_ DNS resolution */
	signal(SIGALRM, noresp);
	alarm(5); /* give the host 5000ms to respond */

#if ENABLE_PING6
	if (lsa->sa.sa_family == AF_INET6)
		ping6(lsa);
	else
#endif
		ping4(lsa);
	printf("%s is alive!\n", hostname);
	return EXIT_SUCCESS;
}


#else /* FEATURE_FANCY_PING */


/* full(er) version */

#define OPT_STRING ("qvc:s:I:4" USE_PING6("6"))
enum {
	OPT_QUIET = 1 << 0,
	OPT_VERBOSE = 1 << 1,
	OPT_c = 1 << 2,
	OPT_s = 1 << 3,
	OPT_I = 1 << 4,
	OPT_IPV4 = 1 << 5,
	OPT_IPV6 = (1 << 6) * ENABLE_PING6,
};


static union {
	struct sockaddr sa;
	struct sockaddr_in sin;
#if ENABLE_PING6
	struct sockaddr_in6 sin6;
#endif
} pingaddr;
static struct sockaddr_in sourceaddr;
static int pingsock = -1;
static unsigned datalen; /* intentionally uninitialized to work around gcc bug */

static int if_index;

static unsigned long ntransmitted, nreceived, nrepeats, pingcount;
static int myid;
static unsigned long tmin = ULONG_MAX, tmax, tsum;
static char rcvd_tbl[MAX_DUP_CHK / 8];

static const char *hostname;
static const char *dotted;

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/**************************************************************************/

static void pingstats(int junk ATTRIBUTE_UNUSED)
{
	int status;

	signal(SIGINT, SIG_IGN);

	printf("\n--- %s ping statistics ---\n", hostname);
	printf("%lu packets transmitted, ", ntransmitted);
	printf("%lu packets received, ", nreceived);
	if (nrepeats)
		printf("%lu duplicates, ", nrepeats);
	if (ntransmitted)
		ntransmitted = (ntransmitted - nreceived) * 100 / ntransmitted;
	printf("%lu%% packet loss\n", ntransmitted);
	if (nreceived)
		printf("round-trip min/avg/max = %lu.%lu/%lu.%lu/%lu.%lu ms\n",
			tmin / 10, tmin % 10,
			(tsum / (nreceived + nrepeats)) / 10,
			(tsum / (nreceived + nrepeats)) % 10, tmax / 10, tmax % 10);
	if (nreceived != 0)
		status = EXIT_SUCCESS;
	else
		status = EXIT_FAILURE;
	exit(status);
}

static void sendping_tail(void (*sp)(int), int sz, int sizeof_packet)
{
	if (sz < 0)
		bb_perror_msg_and_die("sendto");
	if (sz != sizeof_packet)
		bb_error_msg_and_die("ping wrote %d chars; %d expected", sz,
			sizeof_packet);

	signal(SIGALRM, sp);
	if (pingcount == 0 || ntransmitted < pingcount) {	/* schedule next in 1s */
		alarm(PINGINTERVAL);
	} else { /* done, wait for the last ping to come back */
		/* todo, don't necessarily need to wait so long... */
		signal(SIGALRM, pingstats);
		alarm(MAXWAIT);
	}
}

static void sendping4(int junk ATTRIBUTE_UNUSED)
{
	struct icmp *pkt;
	int i;
	char packet[datalen + ICMP_MINLEN];

	pkt = (struct icmp *) packet;

	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_code = 0;
	pkt->icmp_cksum = 0;
	pkt->icmp_seq = htons(ntransmitted); /* don't ++ here, it can be a macro */
	pkt->icmp_id = myid;
	CLR((uint16_t)ntransmitted % MAX_DUP_CHK);
	ntransmitted++;

	gettimeofday((struct timeval *) &pkt->icmp_dun, NULL);
	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, sizeof(packet));

	i = sendto(pingsock, packet, sizeof(packet), 0,
			&pingaddr.sa, sizeof(pingaddr.sin));

	sendping_tail(sendping4, i, sizeof(packet));
}
#if ENABLE_PING6
static void sendping6(int junk ATTRIBUTE_UNUSED)
{
	struct icmp6_hdr *pkt;
	int i;
	char packet[datalen + sizeof (struct icmp6_hdr)];

	pkt = (struct icmp6_hdr *) packet;

	pkt->icmp6_type = ICMP6_ECHO_REQUEST;
	pkt->icmp6_code = 0;
	pkt->icmp6_cksum = 0;
	pkt->icmp6_seq = htons(ntransmitted); /* don't ++ here, it can be a macro */
	pkt->icmp6_id = myid;
	CLR((uint16_t)ntransmitted % MAX_DUP_CHK);
	ntransmitted++;

	gettimeofday((struct timeval *) &pkt->icmp6_data8[4], NULL);

	i = sendto(pingsock, packet, sizeof(packet), 0,
			&pingaddr.sa, sizeof(pingaddr.sin6));

	sendping_tail(sendping6, i, sizeof(packet));
}
#endif

static const char *icmp_type_name(int id)
{
	switch (id) {
	case ICMP_ECHOREPLY:      return "Echo Reply";
	case ICMP_DEST_UNREACH:   return "Destination Unreachable";
	case ICMP_SOURCE_QUENCH:  return "Source Quench";
	case ICMP_REDIRECT:       return "Redirect (change route)";
	case ICMP_ECHO:           return "Echo Request";
	case ICMP_TIME_EXCEEDED:  return "Time Exceeded";
	case ICMP_PARAMETERPROB:  return "Parameter Problem";
	case ICMP_TIMESTAMP:      return "Timestamp Request";
	case ICMP_TIMESTAMPREPLY: return "Timestamp Reply";
	case ICMP_INFO_REQUEST:   return "Information Request";
	case ICMP_INFO_REPLY:     return "Information Reply";
	case ICMP_ADDRESS:        return "Address Mask Request";
	case ICMP_ADDRESSREPLY:   return "Address Mask Reply";
	default:                  return "unknown ICMP type";
	}
}
#if ENABLE_PING6
/* RFC3542 changed some definitions from RFC2292 for no good reason, whee!
 * the newer 3542 uses a MLD_ prefix where as 2292 uses ICMP6_ prefix */
#ifndef MLD_LISTENER_QUERY
# define MLD_LISTENER_QUERY ICMP6_MEMBERSHIP_QUERY
#endif
#ifndef MLD_LISTENER_REPORT
# define MLD_LISTENER_REPORT ICMP6_MEMBERSHIP_REPORT
#endif
#ifndef MLD_LISTENER_REDUCTION
# define MLD_LISTENER_REDUCTION ICMP6_MEMBERSHIP_REDUCTION
#endif
static const char *icmp6_type_name(int id)
{
	switch (id) {
	case ICMP6_DST_UNREACH:      return "Destination Unreachable";
	case ICMP6_PACKET_TOO_BIG:   return "Packet too big";
	case ICMP6_TIME_EXCEEDED:    return "Time Exceeded";
	case ICMP6_PARAM_PROB:       return "Parameter Problem";
	case ICMP6_ECHO_REPLY:       return "Echo Reply";
	case ICMP6_ECHO_REQUEST:     return "Echo Request";
	case MLD_LISTENER_QUERY:     return "Listener Query";
	case MLD_LISTENER_REPORT:    return "Listener Report";
	case MLD_LISTENER_REDUCTION: return "Listener Reduction";
	default:                     return "unknown ICMP type";
	}
}
#endif

static void unpack4(char *buf, int sz, struct sockaddr_in *from)
{
	struct icmp *icmppkt;
	struct iphdr *iphdr;
	struct timeval tv, *tp;
	int hlen, dupflag;
	unsigned long triptime;

	gettimeofday(&tv, NULL);

	/* discard if too short */
	if (sz < (datalen + ICMP_MINLEN))
		return;

	/* check IP header */
	iphdr = (struct iphdr *) buf;
	hlen = iphdr->ihl << 2;
	sz -= hlen;
	icmppkt = (struct icmp *) (buf + hlen);
	if (icmppkt->icmp_id != myid)
		return;				/* not our ping */

	if (icmppkt->icmp_type == ICMP_ECHOREPLY) {
		uint16_t recv_seq = ntohs(icmppkt->icmp_seq);
		++nreceived;
		tp = (struct timeval *) icmppkt->icmp_data;

		if ((tv.tv_usec -= tp->tv_usec) < 0) {
			--tv.tv_sec;
			tv.tv_usec += 1000000;
		}
		tv.tv_sec -= tp->tv_sec;

		triptime = tv.tv_sec * 10000 + (tv.tv_usec / 100);
		tsum += triptime;
		if (triptime < tmin)
			tmin = triptime;
		if (triptime > tmax)
			tmax = triptime;

		if (TST(recv_seq % MAX_DUP_CHK)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(recv_seq % MAX_DUP_CHK);
			dupflag = 0;
		}

		if (option_mask32 & OPT_QUIET)
			return;

		printf("%d bytes from %s: icmp_seq=%u", sz,
			   inet_ntoa(*(struct in_addr *) &from->sin_addr.s_addr),
			   recv_seq);
		printf(" ttl=%d", iphdr->ttl);
		printf(" time=%lu.%lu ms", triptime / 10, triptime % 10);
		if (dupflag)
			printf(" (DUP!)");
		puts("");
	} else {
		if (icmppkt->icmp_type != ICMP_ECHO)
			bb_error_msg("warning: got ICMP %d (%s)",
					icmppkt->icmp_type,
					icmp_type_name(icmppkt->icmp_type));
	}
	fflush(stdout);
}
#if ENABLE_PING6
static void unpack6(char *packet, int sz, struct sockaddr_in6 *from, int hoplimit)
{
	struct icmp6_hdr *icmppkt;
	struct timeval tv, *tp;
	int dupflag;
	unsigned long triptime;
	char buf[INET6_ADDRSTRLEN];

	gettimeofday(&tv, NULL);

	/* discard if too short */
	if (sz < (datalen + sizeof(struct icmp6_hdr)))
		return;

	icmppkt = (struct icmp6_hdr *) packet;
	if (icmppkt->icmp6_id != myid)
		return;				/* not our ping */

	if (icmppkt->icmp6_type == ICMP6_ECHO_REPLY) {
		uint16_t recv_seq = ntohs(icmppkt->icmp6_seq);
		++nreceived;
		tp = (struct timeval *) &icmppkt->icmp6_data8[4];

		if ((tv.tv_usec -= tp->tv_usec) < 0) {
			--tv.tv_sec;
			tv.tv_usec += 1000000;
		}
		tv.tv_sec -= tp->tv_sec;

		triptime = tv.tv_sec * 10000 + (tv.tv_usec / 100);
		tsum += triptime;
		if (triptime < tmin)
			tmin = triptime;
		if (triptime > tmax)
			tmax = triptime;

		if (TST(recv_seq % MAX_DUP_CHK)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(recv_seq % MAX_DUP_CHK);
			dupflag = 0;
		}

		if (option_mask32 & OPT_QUIET)
			return;

		printf("%d bytes from %s: icmp6_seq=%u", sz,
			   inet_ntop(AF_INET6, &pingaddr.sin6.sin6_addr,
						 buf, sizeof(buf)),
			   recv_seq);
		printf(" ttl=%d time=%lu.%lu ms", hoplimit,
			   triptime / 10, triptime % 10);
		if (dupflag)
			printf(" (DUP!)");
		puts("");
	} else {
		if (icmppkt->icmp6_type != ICMP6_ECHO_REQUEST)
			bb_error_msg("warning: got ICMP %d (%s)",
					icmppkt->icmp6_type,
					icmp6_type_name(icmppkt->icmp6_type));
	}
	fflush(stdout);
}
#endif

static void ping4(len_and_sockaddr *lsa)
{
	char packet[datalen + MAXIPLEN + MAXICMPLEN];
	int sockopt;

	pingsock = create_icmp_socket();
	pingaddr.sin = lsa->sin;
	if (sourceaddr.sin_addr.s_addr) {
		xbind(pingsock, (struct sockaddr*)&sourceaddr, sizeof(sourceaddr));
	}

	/* enable broadcast pings */
	setsockopt_broadcast(pingsock);

	/* set recv buf for broadcast pings */
	sockopt = 48 * 1024; /* explain why 48k? */
	setsockopt(pingsock, SOL_SOCKET, SO_RCVBUF, &sockopt, sizeof(sockopt));

	printf("PING %s (%s)", hostname, dotted);
	if (sourceaddr.sin_addr.s_addr) {
		printf(" from %s",
			inet_ntoa(*(struct in_addr *) &sourceaddr.sin_addr.s_addr));
	}
	printf(": %d data bytes\n", datalen);

	signal(SIGINT, pingstats);

	/* start the ping's going ... */
	sendping4(0);

	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		socklen_t fromlen = (socklen_t) sizeof(from);
		int c;

		c = recvfrom(pingsock, packet, sizeof(packet), 0,
				(struct sockaddr *) &from, &fromlen);
		if (c < 0) {
			if (errno != EINTR)
				bb_perror_msg("recvfrom");
			continue;
		}
		unpack4(packet, c, &from);
		if (pingcount > 0 && nreceived >= pingcount)
			break;
	}
}
#if ENABLE_PING6
extern int BUG_bad_offsetof_icmp6_cksum(void);
static void ping6(len_and_sockaddr *lsa)
{
	char packet[datalen + MAXIPLEN + MAXICMPLEN];
	int sockopt;
	struct msghdr msg;
	struct sockaddr_in6 from;
	struct iovec iov;
	char control_buf[CMSG_SPACE(36)];

	pingsock = create_icmp6_socket();
	pingaddr.sin6 = lsa->sin6;
	//if (sourceaddr.sin_addr.s_addr) {
	//	xbind(pingsock, (struct sockaddr*)&sourceaddr, sizeof(sourceaddr));
	//}

#ifdef ICMP6_FILTER
	{
		struct icmp6_filter filt;
		if (!(option_mask32 & OPT_VERBOSE)) {
			ICMP6_FILTER_SETBLOCKALL(&filt);
			ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filt);
		} else {
			ICMP6_FILTER_SETPASSALL(&filt);
		}
		if (setsockopt(pingsock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
					   sizeof(filt)) < 0)
			bb_error_msg_and_die("setsockopt(ICMP6_FILTER)");
	}
#endif /*ICMP6_FILTER*/

	/* enable broadcast pings */
	setsockopt_broadcast(pingsock);

	/* set recv buf for broadcast pings */
	sockopt = 48 * 1024; /* explain why 48k? */
	setsockopt(pingsock, SOL_SOCKET, SO_RCVBUF, &sockopt, sizeof(sockopt));

	sockopt = offsetof(struct icmp6_hdr, icmp6_cksum);
	if (offsetof(struct icmp6_hdr, icmp6_cksum) != 2)
		BUG_bad_offsetof_icmp6_cksum();
	setsockopt(pingsock, SOL_RAW, IPV6_CHECKSUM, &sockopt, sizeof(sockopt));

	/* request ttl info to be returned in ancillary data */
	setsockopt(pingsock, SOL_IPV6, IPV6_HOPLIMIT, &const_int_1, sizeof(const_int_1));

	if (if_index)
		pingaddr.sin6.sin6_scope_id = if_index;

	printf("PING %s (%s): %d data bytes\n", hostname, dotted, datalen);

	signal(SIGINT, pingstats);

	/* start the ping's going ... */
	sendping6(0);

	/* listen for replies */
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control_buf;
	iov.iov_base = packet;
	iov.iov_len = sizeof(packet);
	while (1) {
		int c;
		struct cmsghdr *mp;
		int hoplimit = -1;
		msg.msg_controllen = sizeof(control_buf);

		c = recvmsg(pingsock, &msg, 0);
		if (c < 0) {
			if (errno != EINTR)
				bb_perror_msg("recvfrom");
			continue;
		}
		for (mp = CMSG_FIRSTHDR(&msg); mp; mp = CMSG_NXTHDR(&msg, mp)) {
			if (mp->cmsg_level == SOL_IPV6
			 && mp->cmsg_type == IPV6_HOPLIMIT
			 /* don't check len - we trust the kernel: */
			 /* && mp->cmsg_len >= CMSG_LEN(sizeof(int)) */
			) {
				hoplimit = *(int*)CMSG_DATA(mp);
			}
		}
		unpack6(packet, c, &from, hoplimit);
		if (pingcount > 0 && nreceived >= pingcount)
			break;
	}
}
#endif

/* TODO: consolidate ether-wake.c, dnsd.c, ifupdown.c, nslookup.c
 * versions of below thing. BTW we have far too many "%u.%u.%u.%u" too...
*/
static int parse_nipquad(const char *str, struct sockaddr_in* addr)
{
	char dummy;
	unsigned i1, i2, i3, i4;
	if (sscanf(str, "%u.%u.%u.%u%c",
			   &i1, &i2, &i3, &i4, &dummy) == 4
	&& ( (i1|i2|i3|i4) <= 0xff )
	) {
		uint8_t* ptr = (uint8_t*)&addr->sin_addr;
		ptr[0] = i1;
		ptr[1] = i2;
		ptr[2] = i3;
		ptr[3] = i4;
		return 0;
	}
	return 1; /* error */
}

int ping_main(int argc, char **argv);
int ping_main(int argc, char **argv)
{
	len_and_sockaddr *lsa;
	char *opt_c, *opt_s, *opt_I;
	USE_PING6(sa_family_t af = AF_UNSPEC;)

	datalen = DEFDATALEN; /* initialized here rather than in global scope to work around gcc bug */

	/* exactly one argument needed, -v and -q don't mix */
	opt_complementary = "=1:q--v:v--q";
	getopt32(argc, argv, OPT_STRING, &opt_c, &opt_s, &opt_I);
	if (option_mask32 & OPT_c) pingcount = xatoul(opt_c); // -c
	if (option_mask32 & OPT_s) datalen = xatou16(opt_s); // -s
	if (option_mask32 & OPT_I) { // -I
		if_index = if_nametoindex(opt_I);
		if (!if_index)
			if (parse_nipquad(opt_I, &sourceaddr))
				bb_show_usage();
	}
	myid = (int16_t) getpid();
	hostname = argv[optind];
#if ENABLE_PING6
	if (option_mask32 & OPT_IPV4)
		af = AF_INET;
	if (option_mask32 & OPT_IPV6)
		af = AF_INET6;
	lsa = xhost_and_af2sockaddr(hostname, 0, af);
#else
	lsa = xhost_and_af2sockaddr(hostname, 0, AF_INET);
#endif
	dotted = xmalloc_sockaddr2dotted_noport(&lsa->sa, lsa->len);
#if ENABLE_PING6
	if (lsa->sa.sa_family == AF_INET6)
		ping6(lsa);
	else
#endif
		ping4(lsa);
	pingstats(0);
	return EXIT_SUCCESS;
}
#endif /* FEATURE_FANCY_PING */


#if ENABLE_PING6
int ping6_main(int argc, char **argv);
int ping6_main(int argc, char **argv)
{
	argv[0] = (char*)"-6";
	return ping_main(argc + 1, argv - 1);
}
#endif

/* from ping6.c:
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. <BSD Advertising Clause omitted per the July 22, 1999 licensing change
 *		ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change>
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
