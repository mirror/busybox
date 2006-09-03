/* vi: set sw=4 ts=4: */
/*
 * RFC3927 ZeroConf IPv4 Link-Local addressing
 * (see <http://www.zeroconf.org/>)
 *
 * Copyright (C) 2003 by Arthur van Hoff (avh@strangeberry.com)
 * Copyright (C) 2004 by David Brownell
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/*
 * ZCIP just manages the 169.254.*.* addresses.  That network is not
 * routed at the IP level, though various proxies or bridges can
 * certainly be used.  Its naming is built over multicast DNS.
 */

//#define DEBUG

// TODO:
// - more real-world usage/testing, especially daemon mode
// - kernel packet filters to reduce scheduling noise
// - avoid silent script failures, especially under load...
// - link status monitoring (restart on link-up; stop on link-down)

#include "busybox.h"
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <poll.h>
#include <time.h>

#include <sys/wait.h>

#include <netinet/ether.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <linux/if_packet.h>
#include <linux/sockios.h>


struct arp_packet {
	struct ether_header hdr;
	struct ether_arp arp;
} ATTRIBUTE_PACKED;

enum {
/* 169.254.0.0 */
	LINKLOCAL_ADDR = 0xa9fe0000,

/* protocol timeout parameters, specified in seconds */
	PROBE_WAIT = 1,
	PROBE_MIN = 1,
	PROBE_MAX = 2,
	PROBE_NUM = 3,
	MAX_CONFLICTS = 10,
	RATE_LIMIT_INTERVAL = 60,
	ANNOUNCE_WAIT = 2,
	ANNOUNCE_NUM = 2,
	ANNOUNCE_INTERVAL = 2,
	DEFEND_INTERVAL = 10
};

/* States during the configuration process. */
enum {
	PROBE = 0,
	RATE_LIMIT_PROBE,
	ANNOUNCE,
	MONITOR,
	DEFEND
};

/* Implicitly zero-initialized */
static const struct in_addr null_ip;
static const struct ether_addr null_addr;
static int verbose;

#define DBG(fmt,args...) \
	do { } while (0)
#define VDBG	DBG

/**
 * Pick a random link local IP address on 169.254/16, except that
 * the first and last 256 addresses are reserved.
 */
static void pick(struct in_addr *ip)
{
	unsigned	tmp;

	/* use cheaper math than lrand48() mod N */
	do {
		tmp = (lrand48() >> 16) & IN_CLASSB_HOST;
	} while (tmp > (IN_CLASSB_HOST - 0x0200));
	ip->s_addr = htonl((LINKLOCAL_ADDR + 0x0100) + tmp);
}

/**
 * Broadcast an ARP packet.
 */
static int arp(int fd, struct sockaddr *saddr, int op,
	const struct ether_addr *source_addr, struct in_addr source_ip,
	const struct ether_addr *target_addr, struct in_addr target_ip)
{
	struct arp_packet p;
	memset(&p, 0, sizeof(p));

	// ether header
	p.hdr.ether_type = htons(ETHERTYPE_ARP);
	memcpy(p.hdr.ether_shost, source_addr, ETH_ALEN);
	memset(p.hdr.ether_dhost, 0xff, ETH_ALEN);

	// arp request
	p.arp.arp_hrd = htons(ARPHRD_ETHER);
	p.arp.arp_pro = htons(ETHERTYPE_IP);
	p.arp.arp_hln = ETH_ALEN;
	p.arp.arp_pln = 4;
	p.arp.arp_op = htons(op);
	memcpy(&p.arp.arp_sha, source_addr, ETH_ALEN);
	memcpy(&p.arp.arp_spa, &source_ip, sizeof (p.arp.arp_spa));
	memcpy(&p.arp.arp_tha, target_addr, ETH_ALEN);
	memcpy(&p.arp.arp_tpa, &target_ip, sizeof (p.arp.arp_tpa));

	// send it
	if (sendto(fd, &p, sizeof (p), 0, saddr, sizeof (*saddr)) < 0) {
		perror("sendto");
		return -errno;
	}
	return 0;
}

/**
 * Run a script.
 * TODO: sort out stderr/syslog reporting.
 */
static int run(char *script, char *arg, char *intf, struct in_addr *ip)
{
	int pid, status;
	char *why;

	if (script != NULL) {
		VDBG("%s run %s %s\n", intf, script, arg);
		if (ip != NULL) {
			char *addr = inet_ntoa(*ip);
			setenv("ip", addr, 1);
			syslog(LOG_INFO, "%s %s %s", arg, intf, addr);
		}

		pid = vfork();
		if (pid < 0) {			// error
			why = "vfork";
			goto bad;
		} else if (pid == 0) {		// child
			execl(script, script, arg, NULL);
			perror("execl");
			_exit(EXIT_FAILURE);
		}

		if (waitpid(pid, &status, 0) <= 0) {
			why = "waitpid";
			goto bad;
		}
		if (WEXITSTATUS(status) != 0) {
			bb_error_msg("script %s failed, exit=%d\n",
					script, WEXITSTATUS(status));
			return -errno;
		}
	}
	return 0;
bad:
	status = -errno;
	syslog(LOG_ERR, "%s %s, %s error: %s",
		arg, intf, why, strerror(errno));
	return status;
}


/**
 * Return milliseconds of random delay, up to "secs" seconds.
 */
static unsigned ATTRIBUTE_ALWAYS_INLINE ms_rdelay(unsigned secs)
{
	return lrand48() % (secs * 1000);
}

/**
 * main program
 */

int zcip_main(int argc, char *argv[])
{
	char *intf = NULL;
	char *script = NULL;
	int quit = 0;
	int foreground = 0;

	char *why;
	struct sockaddr saddr;
	struct ether_addr addr;
	struct in_addr ip = { 0 };
	int fd;
	int ready = 0;
	suseconds_t timeout = 0;	// milliseconds
	unsigned conflicts = 0;
	unsigned nprobes = 0;
	unsigned nclaims = 0;
	int t;
	int state = PROBE;

	struct ifreq ifr;
	unsigned short seed[3];

	// parse commandline: prog [options] ifname script
	while ((t = getopt(argc, argv, "fqr:v")) != EOF) {
		switch (t) {
		case 'f':
			foreground = 1;
			continue;
		case 'q':
			quit = 1;
			continue;
		case 'r':
			if (inet_aton(optarg, &ip) == 0
					|| (ntohl(ip.s_addr) & IN_CLASSB_NET)
						!= LINKLOCAL_ADDR) {
				bb_error_msg_and_die("invalid link address");
			}
			continue;
		case 'v':
			verbose++;
			foreground = 1;
			continue;
		default:
			bb_error_msg_and_die("bad option");
		}
	}
	if (optind < argc - 1) {
		intf = argv[optind++];
		setenv("interface", intf, 1);
		script = argv[optind++];
	}
	if (optind != argc || !intf)
		bb_show_usage();
	openlog(bb_applet_name, 0, LOG_DAEMON);

	// initialize the interface (modprobe, ifup, etc)
	if (run(script, "init", intf, NULL) < 0)
		return EXIT_FAILURE;

	// initialize saddr
	memset(&saddr, 0, sizeof (saddr));
	safe_strncpy(saddr.sa_data, intf, sizeof (saddr.sa_data));

	// open an ARP socket
	fd = xsocket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP));
	// bind to the interface's ARP socket
	xbind(fd, &saddr, sizeof (saddr);

	// get the interface's ethernet address
	memset(&ifr, 0, sizeof (ifr));
	strncpy(ifr.ifr_name, intf, sizeof (ifr.ifr_name));
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		foreground = 1;
		why = "get ethernet address";
		goto bad;
	}
	memcpy(&addr, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	// start with some stable ip address, either a function of
	// the hardware address or else the last address we used.
	// NOTE: the sequence of addresses we try changes only
	// depending on when we detect conflicts.
	memcpy(seed, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);
	seed48(seed);
	if (ip.s_addr == 0)
		pick(&ip);

	// FIXME cases to handle:
	//  - zcip already running!
	//  - link already has local address... just defend/update

	// daemonize now; don't delay system startup
	if (!foreground) {
		xdaemon(0, verbose);
		syslog(LOG_INFO, "start, interface %s", intf);
	}

	// run the dynamic address negotiation protocol,
	// restarting after address conflicts:
	//  - start with some address we want to try
	//  - short random delay
	//  - arp probes to see if another host else uses it
	//  - arp announcements that we're claiming it
	//  - use it
	//  - defend it, within limits
	while (1) {
		struct pollfd fds[1];
		struct timeval tv1;
		struct arp_packet p;

		fds[0].fd = fd;
		fds[0].events = POLLIN;
		fds[0].revents = 0;

		int source_ip_conflict = 0;
		int target_ip_conflict = 0;

		// poll, being ready to adjust current timeout
		if (!timeout) {
			timeout = ms_rdelay(PROBE_WAIT);
			// FIXME setsockopt(fd, SO_ATTACH_FILTER, ...) to
			// make the kernel filter out all packets except
			// ones we'd care about.
		}
		// set tv1 to the point in time when we timeout
		gettimeofday(&tv1, NULL);
		tv1.tv_usec += (timeout % 1000) * 1000;
		while (tv1.tv_usec > 1000000) {
			tv1.tv_usec -= 1000000;
			tv1.tv_sec++;
		}
		tv1.tv_sec += timeout / 1000;
	
		VDBG("...wait %ld %s nprobes=%d, nclaims=%d\n",
				timeout, intf, nprobes, nclaims);
		switch (poll(fds, 1, timeout)) {

		// timeout
		case 0:
			VDBG("state = %d\n", state);
			switch (state) {
			case PROBE:
				// timeouts in the PROBE state means no conflicting ARP packets
				// have been received, so we can progress through the states
				if (nprobes < PROBE_NUM) {
					nprobes++;
					VDBG("probe/%d %s@%s\n",
							nprobes, intf, inet_ntoa(ip));
					(void)arp(fd, &saddr, ARPOP_REQUEST,
							&addr, null_ip,
							&null_addr, ip);
					timeout = PROBE_MIN * 1000;
					timeout += ms_rdelay(PROBE_MAX
							- PROBE_MIN);
				}
				else {
					// Switch to announce state.
					state = ANNOUNCE;
					nclaims = 0;
					VDBG("announce/%d %s@%s\n",
							nclaims, intf, inet_ntoa(ip));
					(void)arp(fd, &saddr, ARPOP_REQUEST,
							&addr, ip,
							&addr, ip);
					timeout = ANNOUNCE_INTERVAL * 1000;
				}
				break;
			case RATE_LIMIT_PROBE:
				// timeouts in the RATE_LIMIT_PROBE state means no conflicting ARP packets
				// have been received, so we can move immediately to the announce state
				state = ANNOUNCE;
				nclaims = 0;
				VDBG("announce/%d %s@%s\n",
						nclaims, intf, inet_ntoa(ip));
				(void)arp(fd, &saddr, ARPOP_REQUEST,
						&addr, ip,
						&addr, ip);
				timeout = ANNOUNCE_INTERVAL * 1000;
				break;
			case ANNOUNCE:
				// timeouts in the ANNOUNCE state means no conflicting ARP packets
				// have been received, so we can progress through the states
				if (nclaims < ANNOUNCE_NUM) {
					nclaims++;
					VDBG("announce/%d %s@%s\n",
							nclaims, intf, inet_ntoa(ip));
					(void)arp(fd, &saddr, ARPOP_REQUEST,
							&addr, ip,
							&addr, ip);
					timeout = ANNOUNCE_INTERVAL * 1000;
				}
				else {
					// Switch to monitor state.
					state = MONITOR;
					// link is ok to use earlier
					// FIXME update filters
					run(script, "config", intf, &ip);
					ready = 1;
					conflicts = 0;
					timeout = -1; // Never timeout in the monitor state.

					// NOTE:  all other exit paths
					// should deconfig ...
					if (quit)
						return EXIT_SUCCESS;
				}
				break;
			case DEFEND:
				// We won!  No ARP replies, so just go back to monitor.
				state = MONITOR;
				timeout = -1;
				conflicts = 0;
				break;
			default:
				// Invalid, should never happen.  Restart the whole protocol.
				state = PROBE;
				pick(&ip);
				timeout = 0;
				nprobes = 0;
				nclaims = 0;
				break;
			} // switch (state)
			break; // case 0 (timeout)
		// packets arriving
		case 1:
			// We need to adjust the timeout in case we didn't receive
			// a conflicting packet.
			if (timeout > 0) {
				struct timeval tv2;

				gettimeofday(&tv2, NULL);
				if (timercmp(&tv1, &tv2, <)) {
					// Current time is greater than the expected timeout time.
					// Should never happen.
					VDBG("missed an expected timeout\n");
					timeout = 0;
				} else {
					VDBG("adjusting timeout\n");
					timersub(&tv1, &tv2, &tv1);
					timeout = 1000 * tv1.tv_sec
							+ tv1.tv_usec / 1000;
				}
			}

			if ((fds[0].revents & POLLIN) == 0) {
				if (fds[0].revents & POLLERR) {
					// FIXME: links routinely go down;
					// this shouldn't necessarily exit.
					bb_error_msg("%s: poll error\n", intf);
					if (ready) {
						run(script, "deconfig",
								intf, &ip);
					}
					return EXIT_FAILURE;
				}
				continue;
			}

			// read ARP packet
			if (recv(fd, &p, sizeof (p), 0) < 0) {
				why = "recv";
				goto bad;
			}
			if (p.hdr.ether_type != htons(ETHERTYPE_ARP))
				continue;

#ifdef DEBUG
			{
				struct ether_addr * sha = (struct ether_addr *) p.arp.arp_sha;
				struct ether_addr * tha = (struct ether_addr *) p.arp.arp_tha;
				struct in_addr * spa = (struct in_addr *) p.arp.arp_spa;
				struct in_addr * tpa = (struct in_addr *) p.arp.arp_tpa;
				VDBG("%s recv arp type=%d, op=%d,\n",
					intf, ntohs(p.hdr.ether_type),
					ntohs(p.arp.arp_op));
				VDBG("\tsource=%s %s\n",
					ether_ntoa(sha),
					inet_ntoa(*spa));
				VDBG("\ttarget=%s %s\n",
					ether_ntoa(tha),
					inet_ntoa(*tpa));
			}
#endif
			if (p.arp.arp_op != htons(ARPOP_REQUEST)
					&& p.arp.arp_op != htons(ARPOP_REPLY))
				continue;

			if (memcmp(p.arp.arp_spa, &ip.s_addr, sizeof(struct in_addr)) == 0 &&
				memcmp(&addr, &p.arp.arp_sha, ETH_ALEN) != 0) {
				source_ip_conflict = 1;
			}
			if (memcmp(p.arp.arp_tpa, &ip.s_addr, sizeof(struct in_addr)) == 0 &&
				p.arp.arp_op == htons(ARPOP_REQUEST) &&
				memcmp(&addr, &p.arp.arp_tha, ETH_ALEN) != 0) {
				target_ip_conflict = 1;
			}

			VDBG("state = %d, source ip conflict = %d, target ip conflict = %d\n", 
				state, source_ip_conflict, target_ip_conflict);
			switch (state) {
			case PROBE:
			case ANNOUNCE:
				// When probing or announcing, check for source IP conflicts
				// and other hosts doing ARP probes (target IP conflicts).
				if (source_ip_conflict || target_ip_conflict) {
					conflicts++;
					if (conflicts >= MAX_CONFLICTS) {
						VDBG("%s ratelimit\n", intf);
						timeout = RATE_LIMIT_INTERVAL * 1000;
						state = RATE_LIMIT_PROBE;
					}

					// restart the whole protocol
					pick(&ip);
					timeout = 0;
					nprobes = 0;
					nclaims = 0;
				}
				break;
			case MONITOR:
				// If a conflict, we try to defend with a single ARP probe.
				if (source_ip_conflict) {
					VDBG("monitor conflict -- defending\n");
					state = DEFEND;
					timeout = DEFEND_INTERVAL * 1000;
					(void)arp(fd, &saddr,
							ARPOP_REQUEST,
							&addr, ip,
							&addr, ip);
				}
				break;
			case DEFEND:
				// Well, we tried.  Start over (on conflict).
				if (source_ip_conflict) {
					state = PROBE;
					VDBG("defend conflict -- starting over\n");
					ready = 0;
					run(script, "deconfig", intf, &ip);

					// restart the whole protocol
					pick(&ip);
					timeout = 0;
					nprobes = 0;
					nclaims = 0;
				}
				break;
			default:
				// Invalid, should never happen.  Restart the whole protocol.
				VDBG("invalid state -- starting over\n");
				state = PROBE;
				pick(&ip);
				timeout = 0;
				nprobes = 0;
				nclaims = 0;
				break;
			} // switch state

			break; // case 1 (packets arriving)
		default:
			why = "poll";
			goto bad;
		} // switch poll
	}
bad:
	if (foreground)
		perror(why);
	else
		syslog(LOG_ERR, "%s %s, %s error: %s",
			bb_applet_name, intf, why, strerror(errno));
	return EXIT_FAILURE;
}
