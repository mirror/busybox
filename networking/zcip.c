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
 * This can build as part of BusyBox or by itself:
 *
 *	$(CROSS_COMPILE)cc -Os -Wall -DNO_BUSYBOX -DDEBUG -o zcip zcip.c
 *
 * ZCIP just manages the 169.254.*.* addresses.  That network is not
 * routed at the IP level, though various proxies or bridges can
 * certainly be used.  Its naming is built over multicast DNS.
 */

// #define      DEBUG

// TODO:
// - more real-world usage/testing, especially daemon mode
// - kernel packet filters to reduce scheduling noise
// - avoid silent script failures, especially under load...
// - link status monitoring (restart on link-up; stop on link-down)

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <linux/if_packet.h>
#include <linux/sockios.h>


struct arp_packet {
	struct ether_header hdr;
	// FIXME this part is netinet/if_ether.h "struct ether_arp"
	struct arphdr arp;
	struct ether_addr source_addr;
	struct in_addr source_ip;
	struct ether_addr target_addr;
	struct in_addr target_ip;
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

static const unsigned char ZCIP_VERSION[] = "0.75 (18 April 2005)";
static char *prog;

static const struct in_addr null_ip = { 0 };
static const struct ether_addr null_addr = { {0, 0, 0, 0, 0, 0} };

static int verbose = 0;

#ifdef DEBUG

#define DBG(fmt,args...) \
	fprintf(stderr, "%s: " fmt , prog , ## args)
#define VDBG(fmt,args...) do { \
	if (verbose) fprintf(stderr, "%s: " fmt , prog ,## args); \
	} while (0)
#else

#define DBG(fmt,args...) \
	do { } while (0)
#define VDBG	DBG
#endif				/* DEBUG */

/**
 * Pick a random link local IP address on 169.254/16, except that
 * the first and last 256 addresses are reserved.
 */
static void
pick(struct in_addr *ip)
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
static int
arp(int fd, struct sockaddr *saddr, int op,
	const struct ether_addr *source_addr, struct in_addr source_ip,
	const struct ether_addr *target_addr, struct in_addr target_ip)
{
	struct arp_packet p;

	// ether header
	p.hdr.ether_type = htons(ETHERTYPE_ARP);
	memcpy(p.hdr.ether_shost, source_addr, ETH_ALEN);
	memset(p.hdr.ether_dhost, 0xff, ETH_ALEN);

	// arp request
	p.arp.ar_hrd = htons(ARPHRD_ETHER);
	p.arp.ar_pro = htons(ETHERTYPE_IP);
	p.arp.ar_hln = ETH_ALEN;
	p.arp.ar_pln = 4;
	p.arp.ar_op = htons(op);
	memcpy(&p.source_addr, source_addr, ETH_ALEN);
	memcpy(&p.source_ip, &source_ip, sizeof (p.source_ip));
	memcpy(&p.target_addr, target_addr, ETH_ALEN);
	memcpy(&p.target_ip, &target_ip, sizeof (p.target_ip));

	// send it
	if (sendto(fd, &p, sizeof (p), 0, saddr, sizeof (*saddr)) < 0) {
		perror("sendto");
		return -errno;
	}
	return 0;
}

/**
 * Run a script.
 */
static int
run(char *script, char *arg, char *intf, struct in_addr *ip)
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
			fprintf(stderr, "%s: script %s failed, exit=%d\n",
					prog, script, WEXITSTATUS(status));
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

#ifndef	NO_BUSYBOX
#include "busybox.h"
#endif

/**
 * Print usage information.
 */
static void ATTRIBUTE_NORETURN
zcip_usage(const char *msg)
{
	fprintf(stderr, "%s: %s\n", prog, msg);
#ifdef	NO_BUSYBOX
	fprintf(stderr, "Usage: %s [OPTIONS] ifname script\n"
			"\t-f              foreground mode (implied by -v)\n"
			"\t-q              quit after address (no daemon)\n"
			"\t-r 169.254.x.x  request this address first\n"
			"\t-v              verbose; show version\n",
			prog);
	exit(0);
#else
	bb_show_usage();
#endif
}

/**
 * Return milliseconds of random delay, up to "secs" seconds.
 */
static inline unsigned
ms_rdelay(unsigned secs)
{
	return lrand48() % (secs * 1000);
}

/**
 * main program
 */

#ifdef	NO_BUSYBOX
int
main(int argc, char *argv[])
	__attribute__ ((weak, alias ("zcip_main")));
#endif

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
	time_t defend = 0;
	unsigned conflicts = 0;
	unsigned nprobes = 0;
	unsigned nclaims = 0;
	int t;

	// parse commandline: prog [options] ifname script
	prog = argv[0];
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
				zcip_usage("invalid link address");
			}
			continue;
		case 'v':
			if (!verbose)
				printf("%s: version %s\n", prog, ZCIP_VERSION);
			verbose++;
			foreground = 1;
			continue;
		default:
			zcip_usage("bad option");
		}
	}
	if (optind < argc - 1) {
		intf = argv[optind++];
		setenv("interface", intf, 1);
		script = argv[optind++];
	}
	if (optind != argc || !intf)
		zcip_usage("wrong number of arguments");
	openlog(prog, 0, LOG_DAEMON);

	// initialize the interface (modprobe, ifup, etc)
	if (run(script, "init", intf, NULL) < 0)
		return EXIT_FAILURE;

	// initialize saddr
	memset(&saddr, 0, sizeof (saddr));
	strncpy(saddr.sa_data, intf, sizeof (saddr.sa_data));

	// open an ARP socket
	if ((fd = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP))) < 0) {
		why = "open";
fail:
		foreground = 1;
		goto bad;
	}
	// bind to the interface's ARP socket
	if (bind(fd, &saddr, sizeof (saddr)) < 0) {
		why = "bind";
		goto fail;
	} else {
		struct ifreq ifr;
		unsigned short seed[3];

		// get the interface's ethernet address
		memset(&ifr, 0, sizeof (ifr));
		strncpy(ifr.ifr_name, intf, sizeof (ifr.ifr_name));
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
			why = "get ethernet address";
			goto fail;
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
	}

	// FIXME cases to handle:
	//  - zcip already running!
	//  - link already has local address... just defend/update

	// daemonize now; don't delay system startup
	if (!foreground) {
		if (daemon(0, verbose) < 0) {
			why = "daemon";
			goto bad;
		}
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

		// poll, being ready to adjust current timeout
		if (timeout > 0) {
			gettimeofday(&tv1, NULL);
			tv1.tv_usec += (timeout % 1000) * 1000;
			while (tv1.tv_usec > 1000000) {
				tv1.tv_usec -= 1000000;
				tv1.tv_sec++;
			}
			tv1.tv_sec += timeout / 1000;
		} else if (timeout == 0) {
			timeout = ms_rdelay(PROBE_WAIT);
			// FIXME setsockopt(fd, SO_ATTACH_FILTER, ...) to
			// make the kernel filter out all packets except
			// ones we'd care about.
		}
		VDBG("...wait %ld %s nprobes=%d, nclaims=%d\n",
				timeout, intf, nprobes, nclaims);
		switch (poll(fds, 1, timeout)) {

		// timeouts trigger protocol transitions
		case 0:
			// probes
			if (nprobes < PROBE_NUM) {
				nprobes++;
				VDBG("probe/%d %s@%s\n",
						nprobes, intf, inet_ntoa(ip));
				(void)arp(fd, &saddr, ARPOP_REQUEST,
						&addr, null_ip,
						&null_addr, ip);
				if (nprobes < PROBE_NUM) {
					timeout = PROBE_MIN * 1000;
					timeout += ms_rdelay(PROBE_MAX
							- PROBE_MIN);
				} else
					timeout = ANNOUNCE_WAIT * 1000;
			}
			// then announcements
			else if (nclaims < ANNOUNCE_NUM) {
				nclaims++;
				VDBG("announce/%d %s@%s\n",
						nclaims, intf, inet_ntoa(ip));
				(void)arp(fd, &saddr, ARPOP_REQUEST,
						&addr, ip,
						&addr, ip);
				if (nclaims < ANNOUNCE_NUM) {
					timeout = ANNOUNCE_INTERVAL * 1000;
				} else {
					// link is ok to use earlier
					run(script, "config", intf, &ip);
					ready = 1;
					conflicts = 0;
					timeout = -1;

					// NOTE:  all other exit paths
					// should deconfig ...
					if (quit)
						return EXIT_SUCCESS;
					// FIXME update filters
				}
			}
			break;

		// packets arriving
		case 1:
			// maybe adjust timeout
			if (timeout > 0) {
				struct timeval tv2;

				gettimeofday(&tv2, NULL);
				if (timercmp(&tv1, &tv2, <)) {
					timeout = -1;
				} else {
					timersub(&tv1, &tv2, &tv1);
					timeout = 1000 * tv1.tv_sec
							+ tv1.tv_usec / 1000;
				}
			}
			if ((fds[0].revents & POLLIN) == 0) {
				if (fds[0].revents & POLLERR) {
					// FIXME: links routinely go down;
					// this shouldn't necessarily exit.
					fprintf(stderr, "%s %s: poll error\n",
							prog, intf);
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

			VDBG("%s recv arp type=%d, op=%d,\n",
					intf, ntohs(p.hdr.ether_type),
					ntohs(p.arp.ar_op));
			VDBG("\tsource=%s %s\n",
					ether_ntoa(&p.source_addr),
					inet_ntoa(p.source_ip));
			VDBG("\ttarget=%s %s\n",
					ether_ntoa(&p.target_addr),
					inet_ntoa(p.target_ip));
			if (p.arp.ar_op != htons(ARPOP_REQUEST)
					&& p.arp.ar_op != htons(ARPOP_REPLY))
				continue;

			// some cases are always conflicts
			if ((p.source_ip.s_addr == ip.s_addr)
					&& (memcmp(&addr, &p.source_addr,
							ETH_ALEN) != 0)) {
collision:
				VDBG("%s ARP conflict from %s\n", intf,
						ether_ntoa(&p.source_addr));
				if (ready) {
					time_t now = time(0);

					if ((defend + DEFEND_INTERVAL)
							< now) {
						defend = now;
						(void)arp(fd, &saddr,
								ARPOP_REQUEST,
								&addr, ip,
								&addr, ip);
						VDBG("%s defend\n", intf);
						timeout = -1;
						continue;
					}
					defend = now;
					ready = 0;
					run(script, "deconfig", intf, &ip);
					// FIXME rm filters: setsockopt(fd,
					// SO_DETACH_FILTER, ...)
				}
				conflicts++;
				if (conflicts >= MAX_CONFLICTS) {
					VDBG("%s ratelimit\n", intf);
					sleep(RATE_LIMIT_INTERVAL);
				}
				// restart the whole protocol
				pick(&ip);
				timeout = 0;
				nprobes = 0;
				nclaims = 0;
			}
			// two hosts probing one address is a collision too
			else if (p.target_ip.s_addr == ip.s_addr
					&& nclaims == 0
					&& p.arp.ar_op == htons(ARPOP_REQUEST)
					&& memcmp(&addr, &p.target_addr,
							ETH_ALEN) != 0) {
				goto collision;
			}
			break;

		default:
			why = "poll";
			goto bad;
		}
	}
bad:
	if (foreground)
		perror(why);
	else
		syslog(LOG_ERR, "%s %s, %s error: %s",
			prog, intf, why, strerror(errno));
	return EXIT_FAILURE;
}
