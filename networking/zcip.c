/*
 * RFC3927 ZeroConf IPv4 Link-Local addressing
 * (see <http://www.zeroconf.org/>)
 *
 * Copyright (C) 2003 by Arthur van Hoff (avh@strangeberry.com)
 * Copyright (C) 2004 by David Brownell
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
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

/* TODO:
 - more real-world usage/testing, especially daemon mode
 - kernel packet filters to reduce scheduling noise
 - avoid silent script failures, especially under load...
 - link status monitoring (restart on link-up; stop on link-down)
*/
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
#include "busybox.h"
#include "libbb.h"

struct arp_packet {
	struct ether_header hdr;
	/* FIXME this part is netinet/if_ether.h "struct ether_arp" */
	struct arphdr arp;
	struct ether_addr source_addr;
	struct in_addr source_ip;
	struct ether_addr target_addr;
	struct in_addr target_ip;
} __attribute__ ((__packed__));

/* 169.254.0.0 */
static const uint32_t LINKLOCAL_ADDR = 0xa9fe0000;

/* protocol timeout parameters, specified in seconds */
static const unsigned PROBE_WAIT = 1;
static const unsigned PROBE_MIN = 1;
static const unsigned PROBE_MAX = 2;
static const unsigned PROBE_NUM = 3;
static const unsigned MAX_CONFLICTS = 10;
static const unsigned RATE_LIMIT_INTERVAL = 60;
static const unsigned ANNOUNCE_WAIT = 2;
static const unsigned ANNOUNCE_NUM = 2;
static const unsigned ANNOUNCE_INTERVAL = 2;
static const time_t DEFEND_INTERVAL = 10;

#define ZCIP_VERSION     "0.75 (18 April 2005)"

static const struct in_addr null_ip = { 0 };
static const struct ether_addr null_addr = { {0, 0, 0, 0, 0, 0} };


/*
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

/*
 * Broadcast an ARP packet.
 */
static int
arp(int fd, struct sockaddr *saddr, int op,
	const struct ether_addr *source_addr, struct in_addr source_ip,
	const struct ether_addr *target_addr, struct in_addr target_ip)
{
	struct arp_packet p;

	/* ether header */
	p.hdr.ether_type = htons(ETHERTYPE_ARP);
	memcpy(p.hdr.ether_shost, source_addr, ETH_ALEN);
	memset(p.hdr.ether_dhost, 0xff, ETH_ALEN);

	/* arp request */
	p.arp.ar_hrd = htons(ARPHRD_ETHER);
	p.arp.ar_pro = htons(ETHERTYPE_IP);
	p.arp.ar_hln = ETH_ALEN;
	p.arp.ar_pln = 4;
	p.arp.ar_op = htons(op);
	memcpy(&p.source_addr, source_addr, ETH_ALEN);
	memcpy(&p.source_ip, &source_ip, sizeof (p.source_ip));
	memcpy(&p.target_addr, target_addr, ETH_ALEN);
	memcpy(&p.target_ip, &target_ip, sizeof (p.target_ip));

	/* send it */
	if (sendto(fd, &p, sizeof (p), 0, saddr, sizeof (*saddr)) < 0) {
		bb_perror_msg("sendto");
		return -errno;
	}
	return 0;
}

/*
 * Run a script.
 */
static int
run(char *script, char *arg, char *intf, struct in_addr *ip)
{
	int pid, status;
	char *why;

	if (script != NULL) {
		if (ip != NULL) {
			char *addr = inet_ntoa(*ip);
			xsetenv("ip", addr);
			syslog(LOG_INFO, "%s %s %s", arg, intf, addr);
		}

		pid = vfork();
		if (pid < 0) {			/* error */
			why = "vfork";
			goto bad;
		} else if (pid == 0) {		/* child */
			execl(script, script, arg, NULL);
			bb_perror_msg_and_die("execl");
		} 

		if (waitpid(pid, &status, 0) <= 0) {
			why = "waitpid";
			goto bad;
		}
		if (WEXITSTATUS(status) != 0) {
			bb_perror_msg("script %s failed, exit=%d", script, WEXITSTATUS(status));
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

/*
 * Return milliseconds of random delay, up to "secs" seconds.
 */
static inline unsigned
ms_rdelay(unsigned secs)
{
	return lrand48() % (secs * 1000);
}

/*
 * main program
 */

#define FOREGROUND        1
#define QUIT              2
#define REQUEST           4
#define VERBOSE           8

int zcip_main(int argc, char *argv[])
{
	char *intf = NULL;
	char *script = NULL;
	char *why;
	struct sockaddr saddr;
	struct ether_addr addr;
	struct in_addr ip = { 0 };
	int fd;
	int ready = 0;
	suseconds_t timeout = 0;	/* milliseconds */
	time_t defend = 0;
	unsigned conflicts = 0;
	unsigned nprobes = 0;
	unsigned nclaims = 0;
	unsigned long t;

	bb_opt_complementaly = "vf";
	/* parse commandline: prog [options] ifname script */
	t = bb_getopt_ulflags(argc, argv, "fqr:v", &why); /* reuse char* why */
	
	argc -= optind;
	argv += optind;
	
	if ((t & 0x80000000UL) || (argc < 1) || (argc > 2)) {
		bb_show_usage();
	}
	
	if (t & VERBOSE) {
		bb_printf("%s: version %s\n", bb_applet_name, ZCIP_VERSION);
	}
	if ((t & REQUEST) && (inet_aton(why, &ip) == 0 || (ntohl(ip.s_addr) & IN_CLASSB_NET) != LINKLOCAL_ADDR)) {
		bb_perror_msg_and_die("invalid link address");
	}
	
	intf = argv[0];
	xsetenv("interface", intf);
	script = argv[1]; /* Could be NULL ? */
	
	openlog(bb_applet_name, 0, LOG_DAEMON);

	/* initialize the interface (modprobe, ifup, etc) */
	if (run(script, "init", intf, NULL) < 0)
		return EXIT_FAILURE;

	/* initialize saddr */
	memset(&saddr, 0, sizeof (saddr));
	strncpy(saddr.sa_data, intf, sizeof (saddr.sa_data));

	/* open an ARP socket */
	if ((fd = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP))) < 0) {
		why = "open";
fail:
		t |= FOREGROUND;
		goto bad;
	}
	/* bind to the interface's ARP socket */
	if (bind(fd, &saddr, sizeof (saddr)) < 0) {
		why = "bind";
		goto fail;
	} else {
		struct ifreq ifr;
		short seed[3];

		/* get the interface's ethernet address */
		memset(&ifr, 0, sizeof (ifr));
		strncpy(ifr.ifr_name, intf, sizeof (ifr.ifr_name));
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
			why = "get ethernet address";
			goto fail;
		}
		memcpy(&addr, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);

		/* start with some stable ip address, either a function of
		   the hardware address or else the last address we used.
		   NOTE: the sequence of addresses we try changes only
		   depending on when we detect conflicts. */
		memcpy(seed, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);
		seed48(seed);
		if (ip.s_addr == 0)
			pick(&ip);
	}

	/* FIXME cases to handle:
	    - zcip already running!
	    - link already has local address... just defend/update */

	/* daemonize now; don't delay system startup */
	if (!(t & FOREGROUND)) {
		if (daemon(0, (t & VERBOSE)) < 0) {
			why = "daemon";
			goto bad;
		}
		syslog(LOG_INFO, "start, interface %s", intf);
	}

	/* run the dynamic address negotiation protocol,
	   restarting after address conflicts:
	    - start with some address we want to try
	    - short random delay
	    - arp probes to see if another host else uses it
	    - arp announcements that we're claiming it
	    - use it
	    - defend it, within limits */
	while (1) {
		struct pollfd fds[1];
		struct timeval tv1;
		struct arp_packet p;

		fds[0].fd = fd;
		fds[0].events = POLLIN;
		fds[0].revents = 0;

		/* poll, being ready to adjust current timeout */ 
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
			/* FIXME setsockopt(fd, SO_ATTACH_FILTER, ...) to
			   make the kernel filter out all packets except
			   ones we'd care about. */
		}
		switch (poll(fds, 1, timeout)) {

		/* timeouts trigger protocol transitions */
		case 0:
			/* probes */
			if (nprobes < PROBE_NUM) {
				nprobes++;
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
			/* then announcements */
			else if (nclaims < ANNOUNCE_NUM) {
				nclaims++;
				(void)arp(fd, &saddr, ARPOP_REQUEST,
						&addr, ip,
						&addr, ip);
				if (nclaims < ANNOUNCE_NUM) {
					timeout = ANNOUNCE_INTERVAL * 1000;
				} else {
					/* link is ok to use earlier */
					run(script, "config", intf, &ip);
					ready = 1;
					conflicts = 0;
					timeout = -1;

					/* NOTE:  all other exit paths
					   should deconfig ... */
					if (t & QUIT)
						return EXIT_SUCCESS;
					/* FIXME update filters */
				}
			}
			break;

		/* packets arriving */
		case 1:
			/* maybe adjust timeout */
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
					/* FIXME: links routinely go down;
					   this shouldn't necessarily exit. */
					bb_perror_msg("%s: poll error", intf);
					if (ready) {
						run(script, "deconfig", intf, &ip);
					}
					return EXIT_FAILURE;
				}
				continue;
			}
			/* read ARP packet */
			if (recv(fd, &p, sizeof (p), 0) < 0) {
				why = "recv";
				goto bad;
			}
			if (p.hdr.ether_type != htons(ETHERTYPE_ARP))
				continue;
			
			if (p.arp.ar_op != htons(ARPOP_REQUEST)
					&& p.arp.ar_op != htons(ARPOP_REPLY))
				continue;

			/* some cases are always conflicts */ 
			if ((p.source_ip.s_addr == ip.s_addr)
					&& (memcmp(&addr, &p.source_addr,
							ETH_ALEN) != 0)) {
collision:
				if (ready) {
					time_t now = time(0);

					if ((defend + DEFEND_INTERVAL)
							< now) {
						defend = now;
						(void)arp(fd, &saddr,
								ARPOP_REQUEST,
								&addr, ip,
								&addr, ip);
						timeout = -1;
						continue;
					}
					defend = now;
					ready = 0;
					run(script, "deconfig", intf, &ip);
					/* FIXME rm filters: setsockopt(fd,
					   SO_DETACH_FILTER, ...) */
				}
				conflicts++;
				if (conflicts >= MAX_CONFLICTS) {
					sleep(RATE_LIMIT_INTERVAL);
				}
				/* restart the whole protocol */
				pick(&ip);
				timeout = 0;
				nprobes = 0;
				nclaims = 0;
			}
			/* two hosts probing one address is a collision too */
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
	if ( t & FOREGROUND) {
		bb_perror_msg(why);
	} else { 
		syslog(LOG_ERR, "%s %s, %s error: %s",
			bb_applet_name, intf, why, strerror(errno));
	}
	return EXIT_FAILURE;
}
