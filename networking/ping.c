/* vi: set sw=4 ts=4: */
/*
 * $Id: ping.c,v 1.55 2003/07/22 08:56:51 andersen Exp $
 * Mini ping implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
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
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/signal.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "busybox.h"


static const int DEFDATALEN = 56;
static const int MAXIPLEN = 60;
static const int MAXICMPLEN = 76;
static const int MAXPACKET = 65468;
#define	MAX_DUP_CHK	(8 * 128)
static const int MAXWAIT = 10;
static const int PINGINTERVAL = 1;		/* second */

#define O_QUIET         (1 << 0)

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

static void ping(const char *host);

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
	return (ans);
}

/* simple version */
#ifndef CONFIG_FEATURE_FANCY_PING
static char *hostname = NULL;
static void noresp(int ign)
{
	printf("No response from %s\n", hostname);
	exit(EXIT_FAILURE);
}

static void ping(const char *host)
{
	struct hostent *h;
	struct sockaddr_in pingaddr;
	struct icmp *pkt;
	int pingsock, c;
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];

	pingsock = create_icmp_socket();

	memset(&pingaddr, 0, sizeof(struct sockaddr_in));

	pingaddr.sin_family = AF_INET;
	h = xgethostbyname(host);
	memcpy(&pingaddr.sin_addr, h->h_addr, sizeof(pingaddr.sin_addr));
	hostname = h->h_name;

	pkt = (struct icmp *) packet;
	memset(pkt, 0, sizeof(packet));
	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, sizeof(packet));

	c = sendto(pingsock, packet, sizeof(packet), 0,
			   (struct sockaddr *) &pingaddr, sizeof(struct sockaddr_in));

	if (c < 0 || c != sizeof(packet))
		bb_perror_msg_and_die("sendto");

	signal(SIGALRM, noresp);
	alarm(5);					/* give the host 5000ms to respond */
	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		size_t fromlen = sizeof(from);

		if ((c = recvfrom(pingsock, packet, sizeof(packet), 0,
						  (struct sockaddr *) &from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
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
	printf("%s is alive!\n", hostname);
	return;
}

extern int ping_main(int argc, char **argv)
{
	argc--;
	argv++;
	if (argc < 1)
		bb_show_usage();
	ping(*argv);
	return EXIT_SUCCESS;
}

#else /* ! CONFIG_FEATURE_FANCY_PING */
/* full(er) version */
static struct sockaddr_in pingaddr;
static int pingsock = -1;
static int datalen; /* intentionally uninitialized to work around gcc bug */

static long ntransmitted, nreceived, nrepeats, pingcount;
static int myid, options;
static unsigned long tmin = ULONG_MAX, tmax, tsum;
static char rcvd_tbl[MAX_DUP_CHK / 8];

struct hostent *hostent;

static void sendping(int);
static void pingstats(int);
static void unpack(char *, int, struct sockaddr_in *);

/**************************************************************************/

static void pingstats(int junk)
{
	int status;

	signal(SIGINT, SIG_IGN);

	printf("\n--- %s ping statistics ---\n", hostent->h_name);
	printf("%ld packets transmitted, ", ntransmitted);
	printf("%ld packets received, ", nreceived);
	if (nrepeats)
		printf("%ld duplicates, ", nrepeats);
	if (ntransmitted)
		printf("%ld%% packet loss\n",
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
	struct icmp *pkt;
	int i;
	char packet[datalen + 8];

	pkt = (struct icmp *) packet;

	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_code = 0;
	pkt->icmp_cksum = 0;
	pkt->icmp_seq = ntransmitted++;
	pkt->icmp_id = myid;
	CLR(pkt->icmp_seq % MAX_DUP_CHK);

	gettimeofday((struct timeval *) &packet[8], NULL);
	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, sizeof(packet));

	i = sendto(pingsock, packet, sizeof(packet), 0,
			   (struct sockaddr *) &pingaddr, sizeof(struct sockaddr_in));

	if (i < 0)
		bb_perror_msg_and_die("sendto");
	else if ((size_t)i != sizeof(packet))
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

static char *icmp_type_name (int id)
{
	switch (id) {
	case ICMP_ECHOREPLY: 		return "Echo Reply";
	case ICMP_DEST_UNREACH: 	return "Destination Unreachable";
	case ICMP_SOURCE_QUENCH: 	return "Source Quench";
	case ICMP_REDIRECT: 		return "Redirect (change route)";
	case ICMP_ECHO: 			return "Echo Request";
	case ICMP_TIME_EXCEEDED: 	return "Time Exceeded";
	case ICMP_PARAMETERPROB: 	return "Parameter Problem";
	case ICMP_TIMESTAMP: 		return "Timestamp Request";
	case ICMP_TIMESTAMPREPLY: 	return "Timestamp Reply";
	case ICMP_INFO_REQUEST: 	return "Information Request";
	case ICMP_INFO_REPLY: 		return "Information Reply";
	case ICMP_ADDRESS: 			return "Address Mask Request";
	case ICMP_ADDRESSREPLY: 	return "Address Mask Reply";
	default: 					return "unknown ICMP type";
	}
}

static void unpack(char *buf, int sz, struct sockaddr_in *from)
{
	struct icmp *icmppkt;
	struct iphdr *iphdr;
	struct timeval tv, *tp;
	int hlen, dupflag;
	unsigned long triptime;

	gettimeofday(&tv, NULL);

	/* check IP header */
	iphdr = (struct iphdr *) buf;
	hlen = iphdr->ihl << 2;
	/* discard if too short */
	if (sz < (datalen + ICMP_MINLEN))
		return;

	sz -= hlen;
	icmppkt = (struct icmp *) (buf + hlen);

	if (icmppkt->icmp_id != myid)
	    return;				/* not our ping */

	if (icmppkt->icmp_type == ICMP_ECHOREPLY) {
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

		if (TST(icmppkt->icmp_seq % MAX_DUP_CHK)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(icmppkt->icmp_seq % MAX_DUP_CHK);
			dupflag = 0;
		}

		if (options & O_QUIET)
			return;

		printf("%d bytes from %s: icmp_seq=%u", sz,
			   inet_ntoa(*(struct in_addr *) &from->sin_addr.s_addr),
			   icmppkt->icmp_seq);
		printf(" ttl=%d", iphdr->ttl);
		printf(" time=%lu.%lu ms", triptime / 10, triptime % 10);
		if (dupflag)
			printf(" (DUP!)");
		printf("\n");
	} else 
		if (icmppkt->icmp_type != ICMP_ECHO)
			bb_error_msg("Warning: Got ICMP %d (%s)",
					icmppkt->icmp_type, icmp_type_name (icmppkt->icmp_type));
}

static void ping(const char *host)
{
	char packet[datalen + MAXIPLEN + MAXICMPLEN];
	int sockopt;

	pingsock = create_icmp_socket();

	memset(&pingaddr, 0, sizeof(struct sockaddr_in));

	pingaddr.sin_family = AF_INET;
	hostent = xgethostbyname(host);
	if (hostent->h_addrtype != AF_INET)
		bb_error_msg_and_die("unknown address type; only AF_INET is currently supported.");

	memcpy(&pingaddr.sin_addr, hostent->h_addr, sizeof(pingaddr.sin_addr));

	/* enable broadcast pings */
	sockopt = 1;
	setsockopt(pingsock, SOL_SOCKET, SO_BROADCAST, (char *) &sockopt,
			   sizeof(sockopt));

	/* set recv buf for broadcast pings */
	sockopt = 48 * 1024;
	setsockopt(pingsock, SOL_SOCKET, SO_RCVBUF, (char *) &sockopt,
			   sizeof(sockopt));

	printf("PING %s (%s): %d data bytes\n",
	           hostent->h_name,
		   inet_ntoa(*(struct in_addr *) &pingaddr.sin_addr.s_addr),
		   datalen);

	signal(SIGINT, pingstats);

	/* start the ping's going ... */
	sendping(0);

	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		socklen_t fromlen = (socklen_t) sizeof(from);
		int c;

		if ((c = recvfrom(pingsock, packet, sizeof(packet), 0,
						  (struct sockaddr *) &from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
			bb_perror_msg("recvfrom");
			continue;
		}
		unpack(packet, c, &from);
		if (pingcount > 0 && nreceived >= pingcount)
			break;
	}
	pingstats(0);
}

extern int ping_main(int argc, char **argv)
{
	char *thisarg;

	datalen = DEFDATALEN; /* initialized here rather than in global scope to work around gcc bug */

	argc--;
	argv++;
	options = 0;
	/* Parse any options */
	while (argc >= 1 && **argv == '-') {
		thisarg = *argv;
		thisarg++;
		switch (*thisarg) {
		case 'q':
			options |= O_QUIET;
			break;
		case 'c':
			if (--argc <= 0)
			        bb_show_usage();
			argv++;
			pingcount = atoi(*argv);
			break;
		case 's':
			if (--argc <= 0)
			        bb_show_usage();
			argv++;
			datalen = atoi(*argv);
			break;
		default:
			bb_show_usage();
		}
		argc--;
		argv++;
	}
	if (argc < 1)
		bb_show_usage();

	myid = getpid() & 0xFFFF;
	ping(*argv);
	return EXIT_SUCCESS;
}
#endif /* ! CONFIG_FEATURE_FANCY_PING */

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
