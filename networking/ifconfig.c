/* ifconfig
 *
 * Similar to the standard Unix ifconfig, but with only the necessary
 * parts for AF_INET, and without any printing of if info (for now).
 *
 * Bjorn Wesen, Axis Communications AB
 *
 *
 * Authors of the original ifconfig was:      
 *              Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *
 * This program is free software; you can redistribute it
 * and/or  modify it under  the terms of  the GNU General
 * Public  License as  published  by  the  Free  Software
 * Foundation;  either  version 2 of the License, or  (at
 * your option) any later version.
 *
 * $Id: ifconfig.c,v 1.27 2003/11/14 03:04:08 andersen Exp $
 *
 */

/*
 * Heavily modified by Manuel Novoa III       Mar 6, 2001
 *
 * From initial port to busybox, removed most of the redundancy by
 * converting to a table-driven approach.  Added several (optional)
 * args missing from initial port.
 *
 * Still missing:  media, tunnel.
 *
 * 2002-04-20
 * IPV6 support added by Bart Visscher <magick@linux-fan.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* strcmp and friends */
#include <ctype.h>		/* isdigit and friends */
#include <stddef.h>		/* offsetof */
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#if __GLIBC__ >=2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_ether.h>
#endif
#include "inet_common.h"
#include "busybox.h"

#ifdef CONFIG_FEATURE_IFCONFIG_SLIP
# include <net/if_slip.h>
#endif

/* I don't know if this is needed for busybox or not.  Anyone? */
#define QUESTIONABLE_ALIAS_CASE


/* Defines for glibc2.0 users. */
#ifndef SIOCSIFTXQLEN
# define SIOCSIFTXQLEN      0x8943
# define SIOCGIFTXQLEN      0x8942
#endif

/* ifr_qlen is ifru_ivalue, but it isn't present in 2.0 kernel headers */
#ifndef ifr_qlen
# define ifr_qlen        ifr_ifru.ifru_mtu
#endif

#ifndef IFF_DYNAMIC
# define IFF_DYNAMIC     0x8000	/* dialup device with changing addresses */
#endif

#ifdef CONFIG_FEATURE_IPV6
struct in6_ifreq {
	struct in6_addr ifr6_addr;
	uint32_t ifr6_prefixlen;
	int ifr6_ifindex;
};
#endif

/*
 * Here are the bit masks for the "flags" member of struct options below.
 * N_ signifies no arg prefix; M_ signifies arg prefixed by '-'.
 * CLR clears the flag; SET sets the flag; ARG signifies (optional) arg.
 */
#define N_CLR            0x01
#define M_CLR            0x02
#define N_SET            0x04
#define M_SET            0x08
#define N_ARG            0x10
#define M_ARG            0x20

#define M_MASK           (M_CLR | M_SET | M_ARG)
#define N_MASK           (N_CLR | N_SET | N_ARG)
#define SET_MASK         (N_SET | M_SET)
#define CLR_MASK         (N_CLR | M_CLR)
#define SET_CLR_MASK     (SET_MASK | CLR_MASK)
#define ARG_MASK         (M_ARG | N_ARG)

/*
 * Here are the bit masks for the "arg_flags" member of struct options below.
 */

/*
 * cast type:
 *   00 int
 *   01 char *
 *   02 HOST_COPY in_ether
 *   03 HOST_COPY INET_resolve
 */
#define A_CAST_TYPE      0x03
/*
 * map type:
 *   00 not a map type (mem_start, io_addr, irq)
 *   04 memstart (unsigned long)
 *   08 io_addr  (unsigned short)
 *   0C irq      (unsigned char)
 */
#define A_MAP_TYPE       0x0C
#define A_ARG_REQ        0x10	/* Set if an arg is required. */
#define A_NETMASK        0x20	/* Set if netmask (check for multiple sets). */
#define A_SET_AFTER      0x40	/* Set a flag at the end. */
#define A_COLON_CHK      0x80	/* Is this needed?  See below. */
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
#define A_HOSTNAME      0x100	/* Set if it is ip addr. */
#define A_BROADCAST     0x200	/* Set if it is broadcast addr. */
#else
#define A_HOSTNAME          0
#define A_BROADCAST         0
#endif

/*
 * These defines are for dealing with the A_CAST_TYPE field.
 */
#define A_CAST_CHAR_PTR  0x01
#define A_CAST_RESOLVE   0x01
#define A_CAST_HOST_COPY 0x02
#define A_CAST_HOST_COPY_IN_ETHER    A_CAST_HOST_COPY
#define A_CAST_HOST_COPY_RESOLVE     (A_CAST_HOST_COPY | A_CAST_RESOLVE)

/*
 * These defines are for dealing with the A_MAP_TYPE field.
 */
#define A_MAP_ULONG      0x04	/* memstart */
#define A_MAP_USHORT     0x08	/* io_addr */
#define A_MAP_UCHAR      0x0C	/* irq */

/*
 * Define the bit masks signifying which operations to perform for each arg.
 */

#define ARG_METRIC       (A_ARG_REQ /*| A_CAST_INT*/)
#define ARG_MTU          (A_ARG_REQ /*| A_CAST_INT*/)
#define ARG_TXQUEUELEN   (A_ARG_REQ /*| A_CAST_INT*/)
#define ARG_MEM_START    (A_ARG_REQ | A_MAP_ULONG)
#define ARG_IO_ADDR      (A_ARG_REQ | A_MAP_ULONG)
#define ARG_IRQ          (A_ARG_REQ | A_MAP_UCHAR)
#define ARG_DSTADDR      (A_ARG_REQ | A_CAST_HOST_COPY_RESOLVE)
#define ARG_NETMASK      (A_ARG_REQ | A_CAST_HOST_COPY_RESOLVE | A_NETMASK)
#define ARG_BROADCAST    (A_ARG_REQ | A_CAST_HOST_COPY_RESOLVE | A_SET_AFTER | A_BROADCAST)
#define ARG_HW           (A_ARG_REQ | A_CAST_HOST_COPY_IN_ETHER)
#define ARG_POINTOPOINT  (A_ARG_REQ | A_CAST_HOST_COPY_RESOLVE | A_SET_AFTER)
#define ARG_KEEPALIVE    (A_ARG_REQ | A_CAST_CHAR_PTR)
#define ARG_OUTFILL      (A_ARG_REQ | A_CAST_CHAR_PTR)
#define ARG_HOSTNAME     (A_CAST_HOST_COPY_RESOLVE | A_SET_AFTER | A_COLON_CHK | A_HOSTNAME)
#define ARG_ADD_DEL      (A_CAST_HOST_COPY_RESOLVE | A_SET_AFTER)


/*
 * Set up the tables.  Warning!  They must have corresponding order!
 */

struct arg1opt {
	const char *name;
	unsigned short selector;
	unsigned short ifr_offset;
};

struct options {
	const char *name;
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
	const unsigned int flags:6;
	const unsigned int arg_flags:10;
#else
	const unsigned char flags;
	const unsigned char arg_flags;
#endif
	const unsigned short selector;
};

#define ifreq_offsetof(x)  offsetof(struct ifreq, x)

static const struct arg1opt Arg1Opt[] = {
	{"SIOCSIFMETRIC",  SIOCSIFMETRIC,  ifreq_offsetof(ifr_metric)},
	{"SIOCSIFMTU",     SIOCSIFMTU,     ifreq_offsetof(ifr_mtu)},
	{"SIOCSIFTXQLEN",  SIOCSIFTXQLEN,  ifreq_offsetof(ifr_qlen)},
	{"SIOCSIFDSTADDR", SIOCSIFDSTADDR, ifreq_offsetof(ifr_dstaddr)},
	{"SIOCSIFNETMASK", SIOCSIFNETMASK, ifreq_offsetof(ifr_netmask)},
	{"SIOCSIFBRDADDR", SIOCSIFBRDADDR, ifreq_offsetof(ifr_broadaddr)},
#ifdef CONFIG_FEATURE_IFCONFIG_HW
	{"SIOCSIFHWADDR",  SIOCSIFHWADDR,  ifreq_offsetof(ifr_hwaddr)},
#endif
	{"SIOCSIFDSTADDR", SIOCSIFDSTADDR, ifreq_offsetof(ifr_dstaddr)},
#ifdef SIOCSKEEPALIVE
	{"SIOCSKEEPALIVE", SIOCSKEEPALIVE, ifreq_offsetof(ifr_data)},
#endif
#ifdef SIOCSOUTFILL
	{"SIOCSOUTFILL",   SIOCSOUTFILL,   ifreq_offsetof(ifr_data)},
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ
	{"SIOCSIFMAP",     SIOCSIFMAP,     ifreq_offsetof(ifr_map.mem_start)},
	{"SIOCSIFMAP",     SIOCSIFMAP,     ifreq_offsetof(ifr_map.base_addr)},
	{"SIOCSIFMAP",     SIOCSIFMAP,     ifreq_offsetof(ifr_map.irq)},
#endif
	/* Last entry if for unmatched (possibly hostname) arg. */
#ifdef CONFIG_FEATURE_IPV6
	{"SIOCSIFADDR",    SIOCSIFADDR,    ifreq_offsetof(ifr_addr)}, /* IPv6 version ignores the offset */
	{"SIOCDIFADDR",    SIOCDIFADDR,    ifreq_offsetof(ifr_addr)}, /* IPv6 version ignores the offset */
#endif
	{"SIOCSIFADDR",    SIOCSIFADDR,    ifreq_offsetof(ifr_addr)},
};

static const struct options OptArray[] = {
	{"metric",      N_ARG,         ARG_METRIC,      0},
	{"mtu",         N_ARG,         ARG_MTU,         0},
	{"txqueuelen",  N_ARG,         ARG_TXQUEUELEN,  0},
	{"dstaddr",     N_ARG,         ARG_DSTADDR,     0},
	{"netmask",     N_ARG,         ARG_NETMASK,     0},
	{"broadcast",   N_ARG | M_CLR, ARG_BROADCAST,   IFF_BROADCAST},
#ifdef CONFIG_FEATURE_IFCONFIG_HW
	{"hw",          N_ARG, ARG_HW,                  0},
#endif
	{"pointopoint", N_ARG | M_CLR, ARG_POINTOPOINT, IFF_POINTOPOINT},
#ifdef SIOCSKEEPALIVE
	{"keepalive",   N_ARG,         ARG_KEEPALIVE,   0},
#endif
#ifdef SIOCSOUTFILL
	{"outfill",     N_ARG,         ARG_OUTFILL,     0},
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ
	{"mem_start",   N_ARG,         ARG_MEM_START,   0},
	{"io_addr",     N_ARG,         ARG_IO_ADDR,     0},
	{"irq",         N_ARG,         ARG_IRQ,         0},
#endif
#ifdef CONFIG_FEATURE_IPV6
	{"add",         N_ARG,         ARG_ADD_DEL,     0},
	{"del",         N_ARG,         ARG_ADD_DEL,     0},
#endif
	{"arp",         N_CLR | M_SET, 0,               IFF_NOARP},
	{"trailers",    N_CLR | M_SET, 0,               IFF_NOTRAILERS},
	{"promisc",     N_SET | M_CLR, 0,               IFF_PROMISC},
	{"multicast",   N_SET | M_CLR, 0,               IFF_MULTICAST},
	{"allmulti",    N_SET | M_CLR, 0,               IFF_ALLMULTI},
	{"dynamic",     N_SET | M_CLR, 0,               IFF_DYNAMIC},
	{"up",          N_SET,         0,               (IFF_UP | IFF_RUNNING)},
	{"down",        N_CLR,         0,               IFF_UP},
	{NULL,          0,             ARG_HOSTNAME,    (IFF_UP | IFF_RUNNING)}
};

/*
 * A couple of prototypes.
 */

#ifdef CONFIG_FEATURE_IFCONFIG_HW
static int in_ether(char *bufp, struct sockaddr *sap);
#endif

#ifdef CONFIG_FEATURE_IFCONFIG_STATUS
extern int interface_opt_a;
extern int display_interfaces(char *ifname);
#endif

/*
 * Our main function.
 */

int ifconfig_main(int argc, char **argv)
{
	struct ifreq ifr;
	struct sockaddr_in sai;
#ifdef CONFIG_FEATURE_IPV6
	struct sockaddr_in6 sai6;
#endif
#ifdef CONFIG_FEATURE_IFCONFIG_HW
	struct sockaddr sa;
#endif
	const struct arg1opt *a1op;
	const struct options *op;
	int sockfd;			/* socket fd we use to manipulate stuff with */
	int goterr;
	int selector;
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
	unsigned int mask;
	unsigned int did_flags;
	unsigned int sai_hostname, sai_netmask;
#else
	unsigned char mask;
	unsigned char did_flags;
#endif
	char *p;
	char host[128];

	goterr = 0;
	did_flags = 0;
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
	sai_hostname = 0;
	sai_netmask = 0;
#endif

	/* skip argv[0] */
	++argv;
	--argc;

#ifdef CONFIG_FEATURE_IFCONFIG_STATUS
	if ((argc > 0) && (((*argv)[0] == '-') && ((*argv)[1] == 'a') && !(*argv)[2])) {
		interface_opt_a = 1;
		--argc;
		++argv;
	}
#endif

	if (argc <= 1) {
#ifdef CONFIG_FEATURE_IFCONFIG_STATUS
		return display_interfaces(argc ? *argv : NULL);
#else
		bb_error_msg_and_die
			("ifconfig was not compiled with interface status display support.");
#endif
	}

	/* Create a channel to the NET kernel. */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		bb_perror_msg_and_die("socket");
	}

	/* get interface name */
	safe_strncpy(ifr.ifr_name, *argv, IFNAMSIZ);

	/* Process the remaining arguments. */
	while (*++argv != (char *) NULL) {
		p = *argv;
		mask = N_MASK;
		if (*p == '-') {	/* If the arg starts with '-'... */
			++p;		/*    advance past it and */
			mask = M_MASK;	/*    set the appropriate mask. */
		}
		for (op = OptArray; op->name; op++) {	/* Find table entry. */
			if (strcmp(p, op->name) == 0) {	/* If name matches... */
				if ((mask &= op->flags)) {	/* set the mask and go. */
					goto FOUND_ARG;;
				}
				/* If we get here, there was a valid arg with an */
				/* invalid '-' prefix. */
				++goterr;
				goto LOOP;
			}
		}

		/* We fell through, so treat as possible hostname. */
		a1op = Arg1Opt + (sizeof(Arg1Opt) / sizeof(Arg1Opt[0])) - 1;
		mask = op->arg_flags;
		goto HOSTNAME;

	  FOUND_ARG:
		if (mask & ARG_MASK) {
			mask = op->arg_flags;
			a1op = Arg1Opt + (op - OptArray);
			if (mask & A_NETMASK & did_flags) {
				bb_show_usage();
			}
			if (*++argv == NULL) {
				if (mask & A_ARG_REQ) {
					bb_show_usage();
				} else {
					--argv;
					mask &= A_SET_AFTER;	/* just for broadcast */
				}
			} else {	/* got an arg so process it */
			  HOSTNAME:
				did_flags |= (mask & (A_NETMASK|A_HOSTNAME));
				if (mask & A_CAST_HOST_COPY) {
#ifdef CONFIG_FEATURE_IFCONFIG_HW
					if (mask & A_CAST_RESOLVE) {
#endif
#ifdef CONFIG_FEATURE_IPV6
						char *prefix;
						int prefix_len = 0;
#endif

						safe_strncpy(host, *argv, (sizeof host));
#ifdef CONFIG_FEATURE_IPV6
						if ((prefix = strchr(host, '/'))) {
							prefix_len = atol(prefix + 1);
							if ((prefix_len < 0) || (prefix_len > 128)) {
								++goterr;
								goto LOOP;
							}
							*prefix = 0;
						}
#endif

						sai.sin_family = AF_INET;
						sai.sin_port = 0;
						if (!strcmp(host, bb_INET_default)) {
							/* Default is special, meaning 0.0.0.0. */
							sai.sin_addr.s_addr = INADDR_ANY;
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
						} else if (((host[0] == '+') && !host[1]) && (mask & A_BROADCAST) &&
								   (did_flags & (A_NETMASK|A_HOSTNAME)) == (A_NETMASK|A_HOSTNAME)) {
							/* + is special, meaning broadcast is derived. */
							sai.sin_addr.s_addr = (~sai_netmask) | (sai_hostname & sai_netmask);
#endif
#ifdef CONFIG_FEATURE_IPV6
						} else if (inet_pton(AF_INET6, host, &sai6.sin6_addr) > 0) {
							int sockfd6;
							struct in6_ifreq ifr6;

							memcpy((char *) &ifr6.ifr6_addr,
								   (char *) &sai6.sin6_addr,
								   sizeof(struct in6_addr));

							/* Create a channel to the NET kernel. */
							if ((sockfd6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
								bb_perror_msg_and_die("socket6");
							}
							if (ioctl(sockfd6, SIOGIFINDEX, &ifr) < 0) {
								perror("SIOGIFINDEX");
								++goterr;
								continue;
							}
							ifr6.ifr6_ifindex = ifr.ifr_ifindex;
							ifr6.ifr6_prefixlen = prefix_len;
							if (ioctl(sockfd6, a1op->selector, &ifr6) < 0) {
								perror(a1op->name);
								++goterr;
							}
							continue;
#endif
						} else if (inet_aton(host, &sai.sin_addr) == 0) {
							/* It's not a dotted quad. */
							++goterr;
							continue;
						}
#ifdef CONFIG_FEATURE_IFCONFIG_BROADCAST_PLUS
						if (mask & A_HOSTNAME) {
							sai_hostname = sai.sin_addr.s_addr;
						}
						if (mask & A_NETMASK) {
							sai_netmask = sai.sin_addr.s_addr;
						}
#endif
						p = (char *) &sai;
#ifdef CONFIG_FEATURE_IFCONFIG_HW
					} else {	/* A_CAST_HOST_COPY_IN_ETHER */
						/* This is the "hw" arg case. */
						if (strcmp("ether", *argv) || (*++argv == NULL)) {
							bb_show_usage();
						}
						safe_strncpy(host, *argv, (sizeof host));
						if (in_ether(host, &sa)) {
							bb_error_msg("invalid hw-addr %s", host);
							++goterr;
							continue;
						}
						p = (char *) &sa;
					}
#endif
					memcpy((((char *) (&ifr)) + a1op->ifr_offset),
						   p, sizeof(struct sockaddr));
				} else {
					unsigned int i = strtoul(*argv, NULL, 0);

					p = ((char *) (&ifr)) + a1op->ifr_offset;
#ifdef CONFIG_FEATURE_IFCONFIG_MEMSTART_IOADDR_IRQ
					if (mask & A_MAP_TYPE) {
						if (ioctl(sockfd, SIOCGIFMAP, &ifr) < 0) {
							++goterr;
							continue;
						}
						if ((mask & A_MAP_UCHAR) == A_MAP_UCHAR) {
							*((unsigned char *) p) = i;
						} else if (mask & A_MAP_USHORT) {
							*((unsigned short *) p) = i;
						} else {
							*((unsigned long *) p) = i;
						}
					} else
#endif
					if (mask & A_CAST_CHAR_PTR) {
						*((caddr_t *) p) = (caddr_t) i;
					} else {	/* A_CAST_INT */
						*((int *) p) = i;
					}
				}

				if (ioctl(sockfd, a1op->selector, &ifr) < 0) {
					perror(a1op->name);
					++goterr;
					continue;
				}
#ifdef QUESTIONABLE_ALIAS_CASE
				if (mask & A_COLON_CHK) {
					/*
					 * Don't do the set_flag() if the address is an alias with
					 * a - at the end, since it's deleted already! - Roman
					 *
					 * Should really use regex.h here, not sure though how well
					 * it'll go with the cross-platform support etc. 
					 */
					char *ptr;
					short int found_colon = 0;

					for (ptr = ifr.ifr_name; *ptr; ptr++) {
						if (*ptr == ':') {
							found_colon++;
						}
					}

					if (found_colon && *(ptr - 1) == '-') {
						continue;
					}
				}
#endif
			}
			if (!(mask & A_SET_AFTER)) {
				continue;
			}
			mask = N_SET;
		}

		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
			perror("SIOCGIFFLAGS");
			++goterr;
		} else {
			selector = op->selector;
			if (mask & SET_MASK) {
				ifr.ifr_flags |= selector;
			} else {
				ifr.ifr_flags &= ~selector;
			}
			if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
				perror("SIOCSIFFLAGS");
				++goterr;
			}
		}
	  LOOP:
		continue;
	}					/* end of while-loop */

	return goterr;
}

#ifdef CONFIG_FEATURE_IFCONFIG_HW
/* Input an Ethernet address and convert to binary. */
static int in_ether(char *bufp, struct sockaddr *sap)
{
	unsigned char *ptr;
	int i, j;
	unsigned char val;
	unsigned char c;

	sap->sa_family = ARPHRD_ETHER;
	ptr = sap->sa_data;

	i = 0;
	do {
		j = val = 0;

		/* We might get a semicolon here - not required. */
		if (i && (*bufp == ':')) {
			bufp++;
		}

		do {
			c = *bufp;
			if (((unsigned char)(c - '0')) <= 9) {
				c -= '0';
			} else if (((unsigned char)((c|0x20) - 'a')) <= 5) {
				c = (c|0x20) - ('a'-10);
			} else if (j && (c == ':' || c == 0)) {
				break;
			} else {
				return -1;
			}
			++bufp;
			val <<= 4;
			val += c;
		} while (++j < 2);
		*ptr++ = val;
	} while (++i < ETH_ALEN);

	return (int) (*bufp);	/* Error if we don't end at end of string. */
}
#endif
