/* vi: set sw=4 ts=4: */
/*
 * Mini ping implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * This version of ping is adapted from the ping in netkit-base 0.10,
 * which is:
 *
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Original copyright notice is retained at the end of this file.
 *
 * This version is an adaptation of ping.c from busybox.
 * The code was modified by Bart Visscher <magick@linux-fan.com>
 */

#include <netinet/icmp6.h>
#include <net/if.h>
#include "busybox.h"

/* I see RENUMBERED constants in bits/in.h - !!?
 * What a fuck is going on with libc? Is it a glibc joke? */
#ifdef IPV6_2292HOPLIMIT
#undef IPV6_HOPLIMIT
#define IPV6_HOPLIMIT IPV6_2292HOPLIMIT
#endif

enum {
	DEFDATALEN = 56,
	MAXIPLEN = 60,
	MAXICMPLEN = 76,
	MAXPACKET = 65468,
	MAX_DUP_CHK = (8 * 128),
	MAXWAIT = 10,
	PINGINTERVAL = 1		/* second */
};

static const char *hostname;

static void ping(const char *host);

#ifndef CONFIG_FEATURE_FANCY_PING6

/* simple version */

static void noresp(int ign)
{
	printf("No response from %s\n", hostname);
	exit(EXIT_FAILURE);
}

static void ping(const char *host)
{
	len_and_sockaddr *lsa;
	struct sockaddr_in6 pingaddr;
	struct icmp6_hdr *pkt;
	int pingsock, c;
	int sockopt;
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];

	hostname = host;

	pingsock = create_icmp6_socket();

	lsa = host_and_af2sockaddr(host, 0, AF_INET6);
	pingaddr = lsa->sin6;

	pkt = (struct icmp6_hdr *) packet;
	memset(pkt, 0, sizeof(packet));
	pkt->icmp6_type = ICMP6_ECHO_REQUEST;

	sockopt = offsetof(struct icmp6_hdr, icmp6_cksum);
	setsockopt(pingsock, SOL_RAW, IPV6_CHECKSUM, &sockopt, sizeof(sockopt));

	c = sendto(pingsock, packet, DEFDATALEN + sizeof (struct icmp6_hdr), 0,
			   (struct sockaddr *) &pingaddr, sizeof(struct sockaddr_in6));

	if (c < 0) {
		if (ENABLE_FEATURE_CLEAN_UP) close(pingsock);
		bb_perror_msg_and_die("sendto");
	}

	signal(SIGALRM, noresp);
	alarm(5); /* give the host 5000ms to respond */
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
	printf("%s is alive!\n", hostname);
}

int ping6_main(int argc, char **argv)
{
	argc--;
	argv++;
	if (argc < 1)
		bb_show_usage();
	ping(*argv);
	return EXIT_SUCCESS;
}

#else /* ! CONFIG_FEATURE_FANCY_PING6 */

/* full(er) version */

#define OPT_STRING "qvc:s:I:"
enum {
	OPT_QUIET = 1 << 0,
	OPT_VERBOSE = 1 << 1,
};

static struct sockaddr_in6 pingaddr;
static int pingsock = -1;
static unsigned datalen; /* intentionally uninitialized to work around gcc bug */
static int if_index;

static unsigned long ntransmitted, nreceived, nrepeats, pingcount;
static int myid;
static unsigned long tmin = ULONG_MAX, tmax, tsum;
static char rcvd_tbl[MAX_DUP_CHK / 8];

static char *hostname;

static void sendping(int);
static void pingstats(int);
static void unpack(char *, int, struct sockaddr_in6 *, int);

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/**************************************************************************/

static void pingstats(int junk)
{
	int status;

	signal(SIGINT, SIG_IGN);

	printf("\n--- %s ping statistics ---\n", hostname);
	printf("%lu packets transmitted, ", ntransmitted);
	printf("%lu packets received, ", nreceived);
	if (nrepeats)
		printf("%lu duplicates, ", nrepeats);
	if (ntransmitted)
		printf("%lu%% packet loss\n",
			   (ntransmitted - nreceived) * 100 / ntransmitted);
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

static void sendping(int junk)
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
			   (struct sockaddr *) &pingaddr, sizeof(struct sockaddr_in6));

	if (i < 0)
		bb_perror_msg_and_die("sendto");
	if ((size_t)i != sizeof(packet))
		bb_error_msg_and_die("ping wrote %d chars; %d expected", i,
			   (int)sizeof(packet));

	signal(SIGALRM, sendping);
	if (pingcount == 0 || ntransmitted < pingcount) {	/* schedule next in 1s */
		alarm(PINGINTERVAL);
	} else {					/* done, wait for the last ping to come back */
		/* todo, don't necessarily need to wait so long... */
		signal(SIGALRM, pingstats);
		alarm(MAXWAIT);
	}
}

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
static char *icmp6_type_name(int id)
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

static void unpack(char *packet, int sz, struct sockaddr_in6 *from, int hoplimit)
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
			   inet_ntop(AF_INET6, &pingaddr.sin6_addr,
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

extern int BUG_bad_offsetof_icmp6_cksum(void);
static void ping(const char *host)
{
	len_and_sockaddr *lsa;
	char packet[datalen + MAXIPLEN + MAXICMPLEN];
	char buf[INET6_ADDRSTRLEN];
	int sockopt;
	struct msghdr msg;
	struct sockaddr_in6 from;
	struct iovec iov;
	char control_buf[CMSG_SPACE(36)];

	pingsock = create_icmp6_socket();

	lsa = host_and_af2sockaddr(host, 0, AF_INET6);
	hostname = xmalloc_sockaddr2host_noport(&lsa->sa, lsa->len);
	pingaddr = lsa->sin6;

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
		pingaddr.sin6_scope_id = if_index;

	printf("PING %s (%s): %d data bytes\n",
			hostname,
			inet_ntop(AF_INET6, &pingaddr.sin6_addr,
			buf, sizeof(buf)),
			datalen);

	signal(SIGINT, pingstats);

	/* start the ping's going ... */
	sendping(0);

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
		unpack(packet, c, &from, hoplimit);
		if (pingcount > 0 && nreceived >= pingcount)
			break;
	}
	pingstats(0);
}

int ping6_main(int argc, char **argv)
{
	char *opt_c, *opt_s, *opt_I;

	datalen = DEFDATALEN; /* initialized here rather than in global scope to work around gcc bug */

	/* exactly one argument needed, -v and -q don't mix */
	opt_complementary = "=1:q--v:v--q";
	getopt32(argc, argv, OPT_STRING, &opt_c, &opt_s, &opt_I);
	if (option_mask32 & 4) pingcount = xatoul(opt_c); // -c
	if (option_mask32 & 8) datalen = xatou16(opt_s); // -s
	if (option_mask32 & 0x10) { // -I
		if_index = if_nametoindex(opt_I);
		if (!if_index)
			bb_error_msg_and_die(
				"%s: invalid interface name", opt_I);
	}

	myid = (int16_t) getpid();
	ping(argv[optind]);
	return EXIT_SUCCESS;
}
#endif /* ! CONFIG_FEATURE_FANCY_PING6 */

/*
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
