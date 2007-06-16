/*
 * Pscan is a mini port scanner implementation for busybox
 *
 * Copyright 2007 Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

/* debugging */
#ifdef DEBUG_PSCAN
#define DMSG(...) bb_error_msg(__VA_ARGS__)
#define DERR(...) bb_perror_msg(__VA_ARGS__)
#else
#define DMSG(...) ((void)0)
#define DERR(...) ((void)0)
#endif

/* return time in usec */
// TODO: move to libbb and use in traceroute, zcip, arping etc
// (maybe also use absolute monotonic clock - no time warps
// due to admin resetting date/time?)
static unsigned gettimeofday_us(void)
{
	struct timeval now;
	
	if (gettimeofday(&now, NULL))
		return 0;
	return (now.tv_sec * 1000000 + now.tv_usec);
}

static const char *port_name(unsigned port)
{
	struct servent *server;

	server = getservbyport(htons(port), NULL);
	if (server)
		return server->s_name;
	return "unknown";
}

int pscan_main(int argc, char **argv);
int pscan_main(int argc, char **argv)
{
	const char *opt_max_port = "1024";      /* -P: default max port */
	const char *opt_min_port = "1";         /* -p: default min port */
	const char *opt_timeout = "5000";       /* -t: default timeout */
	/* We estimate rtt and wait rtt*4 before concluding that port is 
	 * totally blocked. min rtt of 5 ms may be too low if you are
	 * scanning an Internet host behind saturated/traffic shaped link.
	 * Rule of thumb: with min_rtt of N msec, scanning 1000 ports
	 * will take N seconds at absolute minimum */
	const char *opt_min_rtt = "5";          /* -T: default min rtt */
	len_and_sockaddr *lsap;
	int s;
	unsigned port, max_port, nports;
	unsigned closed_ports = 0;
	unsigned open_ports = 0;
	/* all in usec */
	unsigned timeout;
	unsigned min_rtt;
	unsigned rtt_4;
	unsigned start;

	opt_complementary = "=1"; /* exactly one non-option */
	getopt32(argc, argv, "p:P:t:T:", &opt_min_port, &opt_max_port, &opt_timeout, &opt_min_rtt);
	argv += optind;
	max_port = xatou_range(opt_max_port, 1, 65535);
	port = xatou_range(opt_min_port, 1, 65535);
	nports = max_port - port + 1;
	if ((int)nports <= 0)
		bb_show_usage();
	rtt_4 = timeout = xatou_range(opt_timeout, 1, INT_MAX/1000 / 4) * 1000;
	min_rtt = xatou_range(opt_min_rtt, 1, INT_MAX/1000 / 4) * 1000;

	DMSG("min_rtt %u timeout %u", min_rtt, timeout);

	lsap = xhost2sockaddr(*argv, port);
	printf("Scanning %s ports %u to %u\n Port\tProto\tState\tService\n",
			*argv, port, max_port);

	for (; port <= max_port; port++) {
		DMSG("rtt %u", rtt_4);

		/* The SOCK_STREAM socket type is implemented on the TCP/IP protocol. */
		set_nport(lsap, htons(port));
		s = xsocket(lsap->sa.sa_family, SOCK_STREAM, 0);

		/* We need unblocking socket so we don't need to wait for ETIMEOUT. */
		/* Nonblocking connect typically "fails" with errno == EINPROGRESS */
		ndelay_on(s);
		DMSG("connect to port %u", port);
		start = gettimeofday_us();
		if (connect(s, &lsap->sa, lsap->len) == 0) {
			/* Unlikely, for me even localhost fails :) */
			DMSG("connect succeeded");
			goto open;
		}
		/* Check for untypical errors... */
		if (errno != EAGAIN && errno != EINPROGRESS
		 && errno != ECONNREFUSED
		) {
			bb_perror_nomsg_and_die();
		}

		while (1) {
			if (errno == ECONNREFUSED) {
				DMSG("port %u: ECONNREFUSED", port);
				closed_ports++;
				break;
			}
			DERR("port %u errno %d @%u", port, errno, gettimeofday_us() - start);
			if ((gettimeofday_us() - start) > rtt_4)
				break;
			/* Can sleep (much) longer than specified delay.
			 * We check rtt BEFORE we usleep, otherwise
			 * on localhost we'll do zero writes done (!)
			 * before we exceed (rather small) rtt */
			usleep(rtt_4/8);
			DMSG("write to port %u @%u", port, gettimeofday_us() - start);
			if (write(s, " ", 1) >= 0) { /* We were able to write to the socket */
 open:
				open_ports++;
				printf("%5u\ttcp\topen\t%s\n", port, port_name(port));
				break;
			}
		}
		DMSG("out of loop @%u", gettimeofday_us() - start);

		/* Estimate new rtt - we don't want to wait entire timeout
		 * for each port. *4 allows for rise in net delay.
		 * We increase rtt quickly (*4), decrease slowly (4/8 == 1/2)
		 * because we don't want to accidentally miss ports. */
		rtt_4 = (gettimeofday_us() - start) * 4;
		if (rtt_4 < min_rtt)
			rtt_4 = min_rtt;
		if (rtt_4 > timeout)
			rtt_4 = timeout;
		/* Clean up */
		close(s);
	}
	if (ENABLE_FEATURE_CLEAN_UP) free(lsap);

	printf("%d closed, %d open, %d timed out ports\n",
					closed_ports,
					open_ports,
					nports - (closed_ports + open_ports));
	return EXIT_SUCCESS;
}
