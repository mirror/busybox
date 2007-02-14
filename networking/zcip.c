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
#include <syslog.h>
#include <poll.h>
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

#define VDBG(fmt,args...) \
	do { } while (0)

static unsigned opts;
#define FOREGROUND (opts & 1)
#define QUIT (opts & 2)

/**
 * Pick a random link local IP address on 169.254/16, except that
 * the first and last 256 addresses are reserved.
 */
static void pick(struct in_addr *ip)
{
	unsigned tmp;

	/* use cheaper math than lrand48() mod N */
	do {
		tmp = (lrand48() >> 16) & IN_CLASSB_HOST;
	} while (tmp > (IN_CLASSB_HOST - 0x0200));
	ip->s_addr = htonl((LINKLOCAL_ADDR + 0x0100) + tmp);
}

/* TODO: we need a flag to direct bb_[p]error_msg output to stderr. */

/**
 * Broadcast an ARP packet.
 */
static void arp(int fd, struct sockaddr *saddr, int op,
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
	memcpy(&p.arp.arp_spa, &source_ip, sizeof(p.arp.arp_spa));
	memcpy(&p.arp.arp_tha, target_addr, ETH_ALEN);
	memcpy(&p.arp.arp_tpa, &target_ip, sizeof(p.arp.arp_tpa));

	// send it
	if (sendto(fd, &p, sizeof(p), 0, saddr, sizeof(*saddr)) < 0) {
		bb_perror_msg("sendto");
		//return -errno;
	}
	// Currently all callers ignore errors, that's why returns are
	// commented out...
	//return 0;
}

/**
 * Run a script.
 */
static int run(const char *script, const char *arg, const char *intf, struct in_addr *ip)
{
	int pid, status;
	const char *why;

	if(1) { //always true: if (script != NULL)
		VDBG("%s run %s %s\n", intf, script, arg);
		if (ip != NULL) {
			char *addr = inet_ntoa(*ip);
			setenv("ip", addr, 1);
			bb_info_msg("%s %s %s", arg, intf, addr);
		}

		pid = vfork();
		if (pid < 0) {			// error
			why = "vfork";
			goto bad;
		} else if (pid == 0) {		// child
			execl(script, script, arg, NULL);
			bb_perror_msg("execl");
			_exit(EXIT_FAILURE);
		}

		if (waitpid(pid, &status, 0) <= 0) {
			why = "waitpid";
			goto bad;
		}
		if (WEXITSTATUS(status) != 0) {
			bb_error_msg("script %s failed, exit=%d",
				script, WEXITSTATUS(status));
			return -errno;
		}
	}
	return 0;
bad:
	status = -errno;
	bb_perror_msg("%s %s, %s", arg, intf, why);
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

/* Used to be auto variables on main() stack, but
 * most of them were zero-inited. Moving them to bss
 * is more space-efficient.
 */
static	const struct in_addr null_ip; // = { 0 };
static	const struct ether_addr null_addr; // = { {0, 0, 0, 0, 0, 0} };

static	struct sockaddr saddr; // memset(0);
static	struct in_addr ip; // = { 0 };
static	struct ifreq ifr; //memset(0);

static	char *intf; // = NULL;
static	char *script; // = NULL;
static	suseconds_t timeout; // = 0;	// milliseconds
static	unsigned conflicts; // = 0;
static	unsigned nprobes; // = 0;
static	unsigned nclaims; // = 0;
static	int ready; // = 0;
static	int verbose; // = 0;
static	int state = PROBE;

int zcip_main(int argc, char *argv[]);
int zcip_main(int argc, char *argv[])
{
	struct ether_addr eth_addr;
	const char *why;
	int fd;

	// parse commandline: prog [options] ifname script
	char *r_opt;
	opt_complementary = "vv:vf"; // -v accumulates and implies -f
	opts = getopt32(argc, argv, "fqr:v", &r_opt, &verbose);
	if (!FOREGROUND) {
		/* Do it early, before all bb_xx_msg calls */
		logmode = LOGMODE_SYSLOG;
		openlog(applet_name, 0, LOG_DAEMON);
	}
	if (opts & 4) { // -r n.n.n.n
		if (inet_aton(r_opt, &ip) == 0
		 || (ntohl(ip.s_addr) & IN_CLASSB_NET) != LINKLOCAL_ADDR
		) {
			bb_error_msg_and_die("invalid link address");
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2)
		bb_show_usage();
	intf = argv[0];
	script = argv[1];
	setenv("interface", intf, 1);

	// initialize the interface (modprobe, ifup, etc)
	if (run(script, "init", intf, NULL) < 0)
		return EXIT_FAILURE;

	// initialize saddr
	//memset(&saddr, 0, sizeof(saddr));
	safe_strncpy(saddr.sa_data, intf, sizeof(saddr.sa_data));

	// open an ARP socket
	fd = xsocket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP));
	// bind to the interface's ARP socket
	xbind(fd, &saddr, sizeof(saddr));

	// get the interface's ethernet address
	//memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, intf, sizeof(ifr.ifr_name));
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		bb_perror_msg_and_die("get ethernet address");
	}
	memcpy(&eth_addr, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	// start with some stable ip address, either a function of
	// the hardware address or else the last address we used.
	// NOTE: the sequence of addresses we try changes only
	// depending on when we detect conflicts.
	// (SVID 3 bogon: who says that "short" is always 16 bits?)
	seed48( (unsigned short*)&ifr.ifr_hwaddr.sa_data );
	if (ip.s_addr == 0)
		pick(&ip);

	// FIXME cases to handle:
	//  - zcip already running!
	//  - link already has local address... just defend/update

	// daemonize now; don't delay system startup
	if (!FOREGROUND) {
		/* bb_daemonize(); - bad, will close fd! */
		xdaemon(0, 0);
		bb_info_msg("start, interface %s", intf);
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

		int source_ip_conflict = 0;
		int target_ip_conflict = 0;

		fds[0].fd = fd;
		fds[0].events = POLLIN;
		fds[0].revents = 0;

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
				// timeouts in the PROBE state mean no conflicting ARP packets
				// have been received, so we can progress through the states
				if (nprobes < PROBE_NUM) {
					nprobes++;
					VDBG("probe/%d %s@%s\n",
							nprobes, intf, inet_ntoa(ip));
					arp(fd, &saddr, ARPOP_REQUEST,
							&eth_addr, null_ip,
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
					arp(fd, &saddr, ARPOP_REQUEST,
							&eth_addr, ip,
							&eth_addr, ip);
					timeout = ANNOUNCE_INTERVAL * 1000;
				}
				break;
			case RATE_LIMIT_PROBE:
				// timeouts in the RATE_LIMIT_PROBE state mean no conflicting ARP packets
				// have been received, so we can move immediately to the announce state
				state = ANNOUNCE;
				nclaims = 0;
				VDBG("announce/%d %s@%s\n",
						nclaims, intf, inet_ntoa(ip));
				arp(fd, &saddr, ARPOP_REQUEST,
						&eth_addr, ip,
						&eth_addr, ip);
				timeout = ANNOUNCE_INTERVAL * 1000;
				break;
			case ANNOUNCE:
				// timeouts in the ANNOUNCE state mean no conflicting ARP packets
				// have been received, so we can progress through the states
				if (nclaims < ANNOUNCE_NUM) {
					nclaims++;
					VDBG("announce/%d %s@%s\n",
							nclaims, intf, inet_ntoa(ip));
					arp(fd, &saddr, ARPOP_REQUEST,
							&eth_addr, ip,
							&eth_addr, ip);
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

					// NOTE: all other exit paths
					// should deconfig ...
					if (QUIT)
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
					bb_error_msg("%s: poll error", intf);
					if (ready) {
						run(script, "deconfig",
								intf, &ip);
					}
					return EXIT_FAILURE;
				}
				continue;
			}

			// read ARP packet
			if (recv(fd, &p, sizeof(p), 0) < 0) {
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
				memcmp(&eth_addr, &p.arp.arp_sha, ETH_ALEN) != 0) {
				source_ip_conflict = 1;
			}
			if (memcmp(p.arp.arp_tpa, &ip.s_addr, sizeof(struct in_addr)) == 0 &&
				p.arp.arp_op == htons(ARPOP_REQUEST) &&
				memcmp(&eth_addr, &p.arp.arp_tha, ETH_ALEN) != 0) {
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
					arp(fd, &saddr,
							ARPOP_REQUEST,
							&eth_addr, ip,
							&eth_addr, ip);
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
	bb_perror_msg("%s, %s", intf, why);
	return EXIT_FAILURE;
}
