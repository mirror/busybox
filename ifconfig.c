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
 * $Id: ifconfig.c,v 1.2 2001/02/14 21:23:06 andersen Exp $
 *
 */

#include "busybox.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>   // strcmp and friends
#include <ctype.h>    // isdigit and friends
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_ether.h>

static int sockfd;  /* socket fd we use to manipulate stuff with */

/* print usage and exit */

#define _(x) x

/* Set a certain interface flag. */
static int
set_flag(char *ifname, short flag)
{
	struct ifreq ifr;
	
	strcpy(ifr.ifr_name, ifname);
	if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
		perror("SIOCGIFFLAGS"); 
		return (-1);
	}
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_flags |= flag;
	if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return -1;
	}
	return (0);
}


/* Clear a certain interface flag. */
static int
clr_flag(char *ifname, short flag)
{
	struct ifreq ifr;
	
	strcpy(ifr.ifr_name, ifname);
	if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
		perror("SIOCGIFFLAGS");
		return -1;
	}
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_flags &= ~flag;
	if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return -1;
	}
	return (0);
}

/* resolve XXX.YYY.ZZZ.QQQ -> binary */

static int
INET_resolve(char *name, struct sockaddr_in *sin)
{
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (!strcmp(name, "default")) {
		sin->sin_addr.s_addr = INADDR_ANY;
		return (1);
	}
	/* Look to see if it's a dotted quad. */
	if (inet_aton(name, &sin->sin_addr)) {
		return 0;
	}
	/* guess not.. */
	return -1;
}

/* Input an Ethernet address and convert to binary. */
static int
in_ether(char *bufp, struct sockaddr *sap)
{
	unsigned char *ptr;
	char c, *orig;
	int i;
	unsigned val;
	
	sap->sa_family = ARPHRD_ETHER;
	ptr = sap->sa_data;
	
	i = 0;
	orig = bufp;
	while ((*bufp != '\0') && (i < ETH_ALEN)) {
		val = 0;
		c = *bufp++;
		if (isdigit(c))
			val = c - '0';
		else if (c >= 'a' && c <= 'f')
			val = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			val = c - 'A' + 10;
		else {
#ifdef DEBUG
			fprintf(stderr,
				_("in_ether(%s): invalid ether address!\n"),
				orig);
#endif
			errno = EINVAL;
			return (-1);
		}
		val <<= 4;
		c = *bufp;
		if (isdigit(c))
			val |= c - '0';
		else if (c >= 'a' && c <= 'f')
			val |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			val |= c - 'A' + 10;
		else if (c == ':' || c == 0)
			val >>= 4;
		else {
#ifdef DEBUG
			fprintf(stderr,
				_("in_ether(%s): invalid ether address!\n"),
				orig);
#endif
			errno = EINVAL;
			return (-1);
		}
		if (c != 0)
			bufp++;
		*ptr++ = (unsigned char) (val & 0377);
		i++;
		
		/* We might get a semicolon here - not required. */
		if (*bufp == ':')
			bufp++;
		
	}

	if(i != ETH_ALEN) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}

int ifconfig_main(int argc, char **argv)
{
	struct ifreq ifr;
	struct sockaddr_in sa;
	struct sockaddr sa2;
	char **spp;
	int goterr = 0;
	int r, didnetmask = 0;
	char host[128];

	if(argc < 2) {
		show_usage();
	}

	/* Create a channel to the NET kernel. */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/* skip argv[0] */

	argc--;
	argv++;

	spp = argv;

	/* get interface name */

	safe_strncpy(ifr.ifr_name, *spp++, IFNAMSIZ);

	/* Process the remaining arguments. */
	while (*spp != (char *) NULL) {
		if (!strcmp(*spp, "arp")) {
			goterr |= clr_flag(ifr.ifr_name, IFF_NOARP);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "-arp")) {
			goterr |= set_flag(ifr.ifr_name, IFF_NOARP);
			spp++;
			continue;
		}
		
		if (!strcmp(*spp, "trailers")) {
			goterr |= clr_flag(ifr.ifr_name, IFF_NOTRAILERS);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "-trailers")) {
			goterr |= set_flag(ifr.ifr_name, IFF_NOTRAILERS);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "promisc")) {
			goterr |= set_flag(ifr.ifr_name, IFF_PROMISC);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "-promisc")) {
			goterr |= clr_flag(ifr.ifr_name, IFF_PROMISC);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "multicast")) {
			goterr |= set_flag(ifr.ifr_name, IFF_MULTICAST);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "-multicast")) {
			goterr |= clr_flag(ifr.ifr_name, IFF_MULTICAST);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "allmulti")) {
			goterr |= set_flag(ifr.ifr_name, IFF_ALLMULTI);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "-allmulti")) {
			goterr |= clr_flag(ifr.ifr_name, IFF_ALLMULTI);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "up")) {
			goterr |= set_flag(ifr.ifr_name, (IFF_UP | IFF_RUNNING));
			spp++;
			continue;
		}
		if (!strcmp(*spp, "down")) {
			goterr |= clr_flag(ifr.ifr_name, IFF_UP);
			spp++;
			continue;
		}

		if (!strcmp(*spp, "metric")) {
			if (*++spp == NULL)
				show_usage();
			ifr.ifr_metric = atoi(*spp);
			if (ioctl(sockfd, SIOCSIFMETRIC, &ifr) < 0) {
				fprintf(stderr, "SIOCSIFMETRIC: %s\n", strerror(errno));
				goterr++;
			}
			spp++;
			continue;
		}
		if (!strcmp(*spp, "mtu")) {
			if (*++spp == NULL)
				show_usage();
			ifr.ifr_mtu = atoi(*spp);
			if (ioctl(sockfd, SIOCSIFMTU, &ifr) < 0) {
				fprintf(stderr, "SIOCSIFMTU: %s\n", strerror(errno));
				goterr++;
			}
			spp++;
			continue;
		}
#ifdef SIOCSKEEPALIVE
		if (!strcmp(*spp, "keepalive")) {
			if (*++spp == NULL)
				show_usage();
			ifr.ifr_data = (caddr_t) atoi(*spp);
			if (ioctl(sockfd, SIOCSKEEPALIVE, &ifr) < 0) {
				fprintf(stderr, "SIOCSKEEPALIVE: %s\n", strerror(errno));
				goterr++;
			}
			spp++;
			continue;
		}
#endif

#ifdef SIOCSOUTFILL
		if (!strcmp(*spp, "outfill")) {
			if (*++spp == NULL)
				show_usage();
			ifr.ifr_data = (caddr_t) atoi(*spp);
			if (ioctl(sockfd, SIOCSOUTFILL, &ifr) < 0) {
				fprintf(stderr, "SIOCSOUTFILL: %s\n", strerror(errno));
				goterr++;
			}
			spp++;
			continue;
		}
#endif

		if (!strcmp(*spp, "-broadcast")) {
			goterr |= clr_flag(ifr.ifr_name, IFF_BROADCAST);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "broadcast")) {
			if (*++spp != NULL) {
				safe_strncpy(host, *spp, (sizeof host));
				if (INET_resolve(host, &sa) < 0) {
					goterr++;
					spp++;
					continue;
				}
				memcpy((char *) &ifr.ifr_broadaddr,
				       (char *) &sa,
				       sizeof(struct sockaddr));
				if (ioctl(sockfd, SIOCSIFBRDADDR, &ifr) < 0) {
					perror("SIOCSIFBRDADDR");
					goterr++;
				}
				spp++;
			}
			goterr |= set_flag(ifr.ifr_name, IFF_BROADCAST);
			continue;
		}
		if (!strcmp(*spp, "dstaddr")) {
			if (*++spp == NULL)
				show_usage();
			safe_strncpy(host, *spp, (sizeof host));
			if (INET_resolve(host, &sa) < 0) {
				goterr++;
				spp++;
				continue;
			}
			memcpy((char *) &ifr.ifr_dstaddr, (char *) &sa,
			       sizeof(struct sockaddr));
			if (ioctl(sockfd, SIOCSIFDSTADDR, &ifr) < 0) {
				fprintf(stderr, "SIOCSIFDSTADDR: %s\n",
					strerror(errno));
				goterr++;
			}
			spp++;
			continue;
		}
		if (!strcmp(*spp, "netmask")) {
			if (*++spp == NULL || didnetmask)
				show_usage();
			safe_strncpy(host, *spp, (sizeof host));
			if (INET_resolve(host, &sa) < 0) {
				goterr++;
				spp++;
				continue;
			}
			didnetmask++;
			memcpy((char *) &ifr.ifr_netmask, (char *) &sa,
			       sizeof(struct sockaddr));
			if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0) {
				perror("SIOCSIFNETMASK");
				goterr++;
			}
			spp++;
			continue;
		}

		if (!strcmp(*spp, "-pointopoint")) {
			goterr |= clr_flag(ifr.ifr_name, IFF_POINTOPOINT);
			spp++;
			continue;
		}
		if (!strcmp(*spp, "pointopoint")) {
			if (*(spp + 1) != NULL) {
				spp++;
				safe_strncpy(host, *spp, (sizeof host));
				if (INET_resolve(host, &sa)) {
					goterr++;
					spp++;
					continue;
				}
				memcpy((char *) &ifr.ifr_dstaddr, (char *) &sa,
				       sizeof(struct sockaddr));
				if (ioctl(sockfd, SIOCSIFDSTADDR, &ifr) < 0) {
					perror("SIOCSIFDSTADDR");
					goterr++;
				}
			}
			goterr |= set_flag(ifr.ifr_name, IFF_POINTOPOINT);
			spp++;
			continue;
		};

		if (!strcmp(*spp, "hw")) {
			if (*++spp == NULL || strcmp("ether", *spp)) {
				show_usage();
			}
				
			if (*++spp == NULL) {
				/* silently ignore it if no address */
				continue;
			}

			safe_strncpy(host, *spp, (sizeof host));
			if (in_ether(host, &sa2) < 0) {
				fprintf(stderr, "invalid hw-addr %s\n", host);
				goterr++;
				spp++;
				continue;
			}
			memcpy((char *) &ifr.ifr_hwaddr, (char *) &sa2,
			       sizeof(struct sockaddr));
			if (ioctl(sockfd, SIOCSIFHWADDR, &ifr) < 0) {
				perror("SIOCSIFHWADDR");
				goterr++;
			}
			spp++;
			continue;
		}

		/* If the next argument is a valid hostname, assume OK. */
		safe_strncpy(host, *spp, (sizeof host));

		if (INET_resolve(host, &sa) < 0) {
			show_usage();
		}
		memcpy((char *) &ifr.ifr_addr,
		       (char *) &sa, sizeof(struct sockaddr));

		r = ioctl(sockfd, SIOCSIFADDR, &ifr);

		if (r < 0) {
			perror("SIOCSIFADDR");
			goterr++;
		}

		/*
		 * Don't do the set_flag() if the address is an alias with a - at the
		 * end, since it's deleted already! - Roman
		 *
		 * Should really use regex.h here, not sure though how well it'll go
		 * with the cross-platform support etc. 
		 */
		{
			char *ptr;
			short int found_colon = 0;
			for (ptr = ifr.ifr_name; *ptr; ptr++ )
				if (*ptr == ':') found_colon++;
			
			if (!(found_colon && *(ptr - 1) == '-'))
				goterr |= set_flag(ifr.ifr_name, (IFF_UP | IFF_RUNNING));
		}
		
		spp++;

	} /* end of while-loop */

        exit(0);
}

