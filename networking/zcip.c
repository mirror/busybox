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

#include <netinet/ether.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_packet.h>
#include <linux/sockios.h>

#include "libbb.h"
#include <syslog.h>

/* We don't need more than 32 bits of the counter */
#define MONOTONIC_US() ((unsigned)monotonic_us())

struct arp_packet {
	struct ether_header eth;
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

#define VDBG(...) do { } while (0)


enum {
	sock_fd = 3
};

struct globals {
	char *intf;
	struct sockaddr saddr;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define intf  (G.intf )
#define saddr (G.saddr)


/**
 * Pick a random link local IP address on 169.254/16, except that
 * the first and last 256 addresses are reserved.
 */
static void pick(struct in_addr *ip)
{
	unsigned tmp;

	do {
		tmp = rand() & IN_CLASSB_HOST;
	} while (tmp > (IN_CLASSB_HOST - 0x0200));
	ip->s_addr = htonl((LINKLOCAL_ADDR + 0x0100) + tmp);
}

/**
 * Broadcast an ARP packet.
 */
static void arp(int op,
	const struct ether_addr *source_eth, struct in_addr source_ip,
	const struct ether_addr *target_eth, struct in_addr target_ip)
{
	struct arp_packet p;
	memset(&p, 0, sizeof(p));

	// ether header
	p.eth.ether_type = htons(ETHERTYPE_ARP);
	memcpy(p.eth.ether_shost, source_eth, ETH_ALEN);
	memset(p.eth.ether_dhost, 0xff, ETH_ALEN);

	// arp request
	p.arp.arp_hrd = htons(ARPHRD_ETHER);
	p.arp.arp_pro = htons(ETHERTYPE_IP);
	p.arp.arp_hln = ETH_ALEN;
	p.arp.arp_pln = 4;
	p.arp.arp_op = htons(op);
	memcpy(&p.arp.arp_sha, source_eth, ETH_ALEN);
	memcpy(&p.arp.arp_spa, &source_ip, sizeof(p.arp.arp_spa));
	memcpy(&p.arp.arp_tha, target_eth, ETH_ALEN);
	memcpy(&p.arp.arp_tpa, &target_ip, sizeof(p.arp.arp_tpa));

	// send it
	xsendto(sock_fd, &p, sizeof(p), &saddr, sizeof(saddr));

	// Currently all callers ignore errors, that's why returns are
	// commented out...
	//return 0;
}

/**
 * Run a script. argv[2] is already NULL.
 */
static int run(char *argv[3], struct in_addr *ip)
{
	int status;
	char *addr = addr; /* for gcc */
	const char *fmt = "%s %s %s" + 3;

	VDBG("%s run %s %s\n", intf, argv[0], argv[1]);

	if (ip) {
		addr = inet_ntoa(*ip);
		setenv("ip", addr, 1);
		fmt -= 3;
	}
	bb_info_msg(fmt, argv[1], intf, addr);

	status = wait4pid(spawn(argv));
	if (status < 0) {
		bb_perror_msg("%s %s %s" + 3, argv[1], intf);
		return -errno;
	}
	if (status != 0)
		bb_error_msg("script %s %s failed, exitcode=%d", argv[0], argv[1], status);
	return status;
}

/**
 * Return milliseconds of random delay, up to "secs" seconds.
 */
static ALWAYS_INLINE unsigned random_delay_ms(unsigned secs)
{
	return rand() % (secs * 1000);
}

/**
 * main program
 */
int zcip_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int zcip_main(int argc, char **argv)
{
	int state = PROBE;
	struct ether_addr eth_addr;
	char *r_opt;
	unsigned opts;

	// ugly trick, but I want these zeroed in one go
	struct {
		const struct in_addr null_ip;
		const struct ether_addr null_addr;
		struct in_addr ip;
		struct ifreq ifr;
		char *script_av[3];
		int timeout_ms; /* must be signed */
		unsigned conflicts;
		unsigned nprobes;
		unsigned nclaims;
		int ready;
		int verbose;
	} L;
#define null_ip    (L.null_ip   )
#define null_addr  (L.null_addr )
#define ip         (L.ip        )
#define ifr        (L.ifr       )
#define script_av  (L.script_av )
#define timeout_ms (L.timeout_ms)
#define conflicts  (L.conflicts )
#define nprobes    (L.nprobes   )
#define nclaims    (L.nclaims   )
#define ready      (L.ready     )
#define verbose    (L.verbose   )

	memset(&L, 0, sizeof(L));

#define FOREGROUND (opts & 1)
#define QUIT       (opts & 2)
	// parse commandline: prog [options] ifname script
	// exactly 2 args; -v accumulates and implies -f
	opt_complementary = "=2:vv:vf";
	opts = getopt32(argv, "fqr:v", &r_opt, &verbose);
#if !BB_MMU
	// on NOMMU reexec early (or else we will rerun things twice)
	if (!FOREGROUND)
		bb_daemonize_or_rexec(0 /*was: DAEMON_CHDIR_ROOT*/, argv);
#endif
	// open an ARP socket
	// (need to do it before openlog to prevent openlog from taking
	// fd 3 (sock_fd==3))
	xmove_fd(xsocket(AF_PACKET, SOCK_PACKET, htons(ETH_P_ARP)), sock_fd);
	if (!FOREGROUND) {
		// do it before all bb_xx_msg calls
		openlog(applet_name, 0, LOG_DAEMON);
		logmode |= LOGMODE_SYSLOG;
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

	intf = argv[0];
	script_av[0] = argv[1];
	setenv("interface", intf, 1);

	// initialize the interface (modprobe, ifup, etc)
	script_av[1] = (char*)"init";
	if (run(script_av, NULL))
		return EXIT_FAILURE;

	// initialize saddr
	// saddr is: { u16 sa_family; u8 sa_data[14]; }
	//memset(&saddr, 0, sizeof(saddr));
	//TODO: are we leaving sa_family == 0 (AF_UNSPEC)?!
	safe_strncpy(saddr.sa_data, intf, sizeof(saddr.sa_data));

	// bind to the interface's ARP socket
	xbind(sock_fd, &saddr, sizeof(saddr));

	// get the interface's ethernet address
	//memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, intf, sizeof(ifr.ifr_name));
	xioctl(sock_fd, SIOCGIFHWADDR, &ifr);
	memcpy(&eth_addr, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	// start with some stable ip address, either a function of
	// the hardware address or else the last address we used.
	// we are taking low-order four bytes, as top-order ones
	// aren't random enough.
	// NOTE: the sequence of addresses we try changes only
	// depending on when we detect conflicts.
	{
		uint32_t t;
		memcpy(&t, (char*)&eth_addr + 2, 4);
		srand(t);
	}
	if (ip.s_addr == 0)
		pick(&ip);

	// FIXME cases to handle:
	//  - zcip already running!
	//  - link already has local address... just defend/update

	// daemonize now; don't delay system startup
	if (!FOREGROUND) {
#if BB_MMU
		bb_daemonize(0 /*was: DAEMON_CHDIR_ROOT*/);
#endif
		bb_info_msg("start, interface %s", intf);
	}

	// run the dynamic address negotiation protocol,
	// restarting after address conflicts:
	//  - start with some address we want to try
	//  - short random delay
	//  - arp probes to see if another host uses it
	//  - arp announcements that we're claiming it
	//  - use it
	//  - defend it, within limits
	while (1) {
		struct pollfd fds[1];
		unsigned deadline_us;
		struct arp_packet p;
		int source_ip_conflict;
		int target_ip_conflict;

		fds[0].fd = sock_fd;
		fds[0].events = POLLIN;
		fds[0].revents = 0;

		// poll, being ready to adjust current timeout
		if (!timeout_ms) {
			timeout_ms = random_delay_ms(PROBE_WAIT);
			// FIXME setsockopt(sock_fd, SO_ATTACH_FILTER, ...) to
			// make the kernel filter out all packets except
			// ones we'd care about.
		}
		// set deadline_us to the point in time when we timeout
		deadline_us = MONOTONIC_US() + timeout_ms * 1000;

		VDBG("...wait %d %s nprobes=%u, nclaims=%u\n",
				timeout_ms, intf, nprobes, nclaims);

		switch (safe_poll(fds, 1, timeout_ms)) {

		default:
			//bb_perror_msg("poll"); - done in safe_poll
			return EXIT_FAILURE;

		// timeout
		case 0:
			VDBG("state = %d\n", state);
			switch (state) {
			case PROBE:
				// timeouts in the PROBE state mean no conflicting ARP packets
				// have been received, so we can progress through the states
				if (nprobes < PROBE_NUM) {
					nprobes++;
					VDBG("probe/%u %s@%s\n",
							nprobes, intf, inet_ntoa(ip));
					arp(ARPOP_REQUEST,
							&eth_addr, null_ip,
							&null_addr, ip);
					timeout_ms = PROBE_MIN * 1000;
					timeout_ms += random_delay_ms(PROBE_MAX - PROBE_MIN);
				}
				else {
					// Switch to announce state.
					state = ANNOUNCE;
					nclaims = 0;
					VDBG("announce/%u %s@%s\n",
							nclaims, intf, inet_ntoa(ip));
					arp(ARPOP_REQUEST,
							&eth_addr, ip,
							&eth_addr, ip);
					timeout_ms = ANNOUNCE_INTERVAL * 1000;
				}
				break;
			case RATE_LIMIT_PROBE:
				// timeouts in the RATE_LIMIT_PROBE state mean no conflicting ARP packets
				// have been received, so we can move immediately to the announce state
				state = ANNOUNCE;
				nclaims = 0;
				VDBG("announce/%u %s@%s\n",
						nclaims, intf, inet_ntoa(ip));
				arp(ARPOP_REQUEST,
						&eth_addr, ip,
						&eth_addr, ip);
				timeout_ms = ANNOUNCE_INTERVAL * 1000;
				break;
			case ANNOUNCE:
				// timeouts in the ANNOUNCE state mean no conflicting ARP packets
				// have been received, so we can progress through the states
				if (nclaims < ANNOUNCE_NUM) {
					nclaims++;
					VDBG("announce/%u %s@%s\n",
							nclaims, intf, inet_ntoa(ip));
					arp(ARPOP_REQUEST,
							&eth_addr, ip,
							&eth_addr, ip);
					timeout_ms = ANNOUNCE_INTERVAL * 1000;
				}
				else {
					// Switch to monitor state.
					state = MONITOR;
					// link is ok to use earlier
					// FIXME update filters
					script_av[1] = (char*)"config";
					run(script_av, &ip);
					ready = 1;
					conflicts = 0;
					timeout_ms = -1; // Never timeout in the monitor state.

					// NOTE: all other exit paths
					// should deconfig ...
					if (QUIT)
						return EXIT_SUCCESS;
				}
				break;
			case DEFEND:
				// We won!  No ARP replies, so just go back to monitor.
				state = MONITOR;
				timeout_ms = -1;
				conflicts = 0;
				break;
			default:
				// Invalid, should never happen.  Restart the whole protocol.
				state = PROBE;
				pick(&ip);
				timeout_ms = 0;
				nprobes = 0;
				nclaims = 0;
				break;
			} // switch (state)
			break; // case 0 (timeout)

		// packets arriving, or link went down
		case 1:
			// We need to adjust the timeout in case we didn't receive
			// a conflicting packet.
			if (timeout_ms > 0) {
				unsigned diff = deadline_us - MONOTONIC_US();
				if ((int)(diff) < 0) {
					// Current time is greater than the expected timeout time.
					// Should never happen.
					VDBG("missed an expected timeout\n");
					timeout_ms = 0;
				} else {
					VDBG("adjusting timeout\n");
					timeout_ms = (diff / 1000) | 1; /* never 0 */
				}
			}

			if ((fds[0].revents & POLLIN) == 0) {
				if (fds[0].revents & POLLERR) {
					// FIXME: links routinely go down;
					// this shouldn't necessarily exit.
					bb_error_msg("iface %s is down", intf);
					if (ready) {
						script_av[1] = (char*)"deconfig";
						run(script_av, &ip);
					}
					return EXIT_FAILURE;
				}
				continue;
			}

			// read ARP packet
			if (safe_read(sock_fd, &p, sizeof(p)) < 0) {
				bb_perror_msg_and_die(bb_msg_read_error);
			}
			if (p.eth.ether_type != htons(ETHERTYPE_ARP))
				continue;
#ifdef DEBUG
			{
				struct ether_addr *sha = (struct ether_addr *) p.arp.arp_sha;
				struct ether_addr *tha = (struct ether_addr *) p.arp.arp_tha;
				struct in_addr *spa = (struct in_addr *) p.arp.arp_spa;
				struct in_addr *tpa = (struct in_addr *) p.arp.arp_tpa;
				VDBG("%s recv arp type=%d, op=%d,\n",
					intf, ntohs(p.eth.ether_type),
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

			source_ip_conflict = 0;
			target_ip_conflict = 0;

			if (memcmp(p.arp.arp_spa, &ip.s_addr, sizeof(struct in_addr)) == 0
			 && memcmp(&p.arp.arp_sha, &eth_addr, ETH_ALEN) != 0
			) {
				source_ip_conflict = 1;
			}
			if (p.arp.arp_op == htons(ARPOP_REQUEST)
			 && memcmp(p.arp.arp_tpa, &ip.s_addr, sizeof(struct in_addr)) == 0
			 && memcmp(&p.arp.arp_tha, &eth_addr, ETH_ALEN) != 0
			) {
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
						timeout_ms = RATE_LIMIT_INTERVAL * 1000;
						state = RATE_LIMIT_PROBE;
					}

					// restart the whole protocol
					pick(&ip);
					timeout_ms = 0;
					nprobes = 0;
					nclaims = 0;
				}
				break;
			case MONITOR:
				// If a conflict, we try to defend with a single ARP probe.
				if (source_ip_conflict) {
					VDBG("monitor conflict -- defending\n");
					state = DEFEND;
					timeout_ms = DEFEND_INTERVAL * 1000;
					arp(ARPOP_REQUEST,
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
					script_av[1] = (char*)"deconfig";
					run(script_av, &ip);

					// restart the whole protocol
					pick(&ip);
					timeout_ms = 0;
					nprobes = 0;
					nclaims = 0;
				}
				break;
			default:
				// Invalid, should never happen.  Restart the whole protocol.
				VDBG("invalid state -- starting over\n");
				state = PROBE;
				pick(&ip);
				timeout_ms = 0;
				nprobes = 0;
				nclaims = 0;
				break;
			} // switch state
			break; // case 1 (packets arriving)
		} // switch poll
	} // while (1)
}
