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
 * $Id: ifconfig.c,v 1.4 2001/03/06 00:48:59 andersen Exp $
 *
 * Majorly hacked up by Larry Doolittle <ldoolitt@recycle.lbl.gov> 
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

#define TESTME 0
#if TESTME
#define ioctl test_ioctl
char *saddr_to_a(struct sockaddr *s)
{
	if (s->sa_family == ARPHRD_ETHER) {
		static char hw[18];
		sprintf(hw, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",
			s->sa_data[0], s->sa_data[1], s->sa_data[2],
			s->sa_data[3], s->sa_data[4], s->sa_data[5]);
		return hw;
	} else if (s->sa_family == AF_INET) {
		struct sockaddr_in *ss = (struct sockaddr_in *) s;
		return inet_ntoa(ss->sin_addr);
	} else {
		return NULL;
	}
}

int test_ioctl(int __fd, unsigned long int __request, void *param)
{
	struct ifreq *i=(struct ifreq *)param;
	printf("ioctl fd=%d, request=%ld\n", __fd, __request);
	
	switch(__request) {
		case SIOCGIFFLAGS:   printf("  SIOCGIFFLAGS\n");       i->ifr_flags = 0;   break;
		case SIOCSIFFLAGS:   printf("  SIOCSIFFLAGS, %x\n",    i->ifr_flags);     break;
		case SIOCSIFMETRIC:  printf("  SIOCSIFMETRIC, %d\n",   i->ifr_metric);    break;
		case SIOCSIFMTU:     printf("  SIOCSIFMTU, %d\n",      i->ifr_mtu);       break;
		case SIOCSIFBRDADDR: printf("  SIOCSIFBRDADDR, %s\n",  saddr_to_a(&(i->ifr_broadaddr))); break;
		case SIOCSIFDSTADDR: printf("  SIOCSIFDSTADDR, %s\n",  saddr_to_a(&(i->ifr_dstaddr  ))); break;
		case SIOCSIFNETMASK: printf("  SIOCSIFNETMASK, %s\n",  saddr_to_a(&(i->ifr_netmask  ))); break;
		case SIOCSIFADDR:    printf("  SIOCSIFADDR, %s\n",     saddr_to_a(&(i->ifr_addr     ))); break;
		case SIOCSIFHWADDR:  printf("  SIOCSIFHWADDR, %s\n",   saddr_to_a(&(i->ifr_hwaddr   ))); break;  /* broken */
		default:
	}
	return 0;
}
#endif


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
		return -1;
	}
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_flags |= flag;
	if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
		perror("SIOCSIFFLAGS");
		return -1;
	}
	return 0;
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
	return 0;
}

/* which element in struct ifreq to frob */
enum frob {
	L_METRIC,
	L_MTU,
	L_DATA,
	L_BROAD,
	L_DEST,
	L_MASK,
	L_HWAD,
};


struct flag_map {
	char *name;
	enum frob frob;
	int flag;
	int sflag;
	int action;
};

/* action:
 *  2   set
 *  4   clear
 *  6   set/clear
 *  8   clear/set
 *  10  numeric
 *  12  address
 *  14  address/clear
 */
const static struct flag_map flag_table[] = {
	{"arp",         0,  IFF_NOARP,             0, 6},
	{"trailers",    0,  IFF_NOTRAILERS,        0, 6},
	{"promisc",     0,  IFF_PROMISC,           0, 8},
	{"multicast",   0,  IFF_MULTICAST,         0, 8},
	{"allmulti",    0,  IFF_ALLMULTI,          0, 8},
	{"up",          0, (IFF_UP | IFF_RUNNING), 0, 2},
	{"down",        0,  IFF_UP,                0, 4},
	{"metric",      L_METRIC,              0, SIOCSIFMETRIC,  10},
	{"mtu",         L_MTU,                 0, SIOCSIFMTU,     10},
#ifdef SIOCSKEEPALIVE
	{"keepalive",   L_DATA,                0, SIOCSKEEPALIVE, 10},
#endif
#ifdef SIOCSOUTFILL
	{"outfill",     L_DATA,                0, SIOCSOUTFILL,   10},
#endif
	{"broadcast",   L_BROAD, IFF_BROADCAST,   SIOCSIFBRDADDR, 14},
	{"dstaddr",     L_DEST,                0, SIOCSIFDSTADDR, 12},
	{"netmask",     L_MASK,                0, SIOCSIFNETMASK, 12},
	{"pointopoint", L_DEST,  IFF_POINTOPOINT, SIOCSIFDSTADDR, 14},
	{"hw",          L_HWAD,                0, SIOCSIFHWADDR,  14},
};


/* resolve XXX.YYY.ZZZ.QQQ -> binary */

static int
INET_resolve(char *name, struct sockaddr_in *sin)
{
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (strcmp(name, "default")==0) {
		sin->sin_addr.s_addr = INADDR_ANY;
		return 1;
	}
	/* Look to see if it's a dotted quad. */
	if (inet_aton(name, &sin->sin_addr)) {
		return 0;
	}
	/* guess not.. */
	errno = EINVAL;
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
			error_msg(
				_("in_ether(%s): invalid ether address!"),
				orig);
#endif
			errno = EINVAL;
			return -1;
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
			error_msg(
				_("in_ether(%s): invalid ether address!"),
				orig);
#endif
			errno = EINVAL;
			return -1;
		}
		if (c != 0)
			bufp++;
		*ptr++ = (unsigned char) (val & 0377);
		i++;
		
		/* optional colon already handled, don't swallow a second */
	}

	if(i != ETH_ALEN) {
		errno = EINVAL;
		return -1;
	}

	return 0;
}
		
#ifdef BB_FEATURE_IFCONFIG_STATUS
extern int display_interfaces(void);
#else
int display_interfaces(void)
{
    show_usage();
}
#endif

int ifconfig_main(int argc, char **argv)
{
	struct ifreq ifr;
	struct sockaddr_in sa;
	char **spp, *cmd;
	int goterr = 0;
	int r;
	/* int didnetmask = 0;   special case input error detection no longer implemented */
	char host[128];
	const struct flag_map *ft;
	int i, sense;
	int a, ecode;
	struct sockaddr *d;

	if(argc < 2) {
		return(display_interfaces());
	}

	/* Create a channel to the NET kernel. */
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror_msg_and_die("socket");
	}

	/* skip argv[0] */

	argc--;
	argv++;

	spp = argv;

	/* get interface name */

	safe_strncpy(ifr.ifr_name, *spp++, IFNAMSIZ);

	/* Process the remaining arguments. */
	while (*spp != (char *) NULL) {
		cmd = *spp;
		sense=0;
		if (*cmd=='-') {
			sense=1;
			cmd++;
		}
		ft = NULL;
		for (i=0; i<(sizeof(flag_table)/sizeof(struct flag_map)); i++) {
			if (strcmp(cmd, flag_table[i].name)==0) {
				ft=flag_table+i;
				spp++;
				break;
			}
		}
		if (ft) {
			switch (ft->action+sense) {
			case 4:
			case 7:
			case 8:
			case 15:
				goterr |= clr_flag(ifr.ifr_name, ft->flag);
				break;
			case 2:
			case 6:
			case 9:
				goterr |= set_flag(ifr.ifr_name, ft->flag);
				break;
			case 10:
				if (*spp == NULL)
					show_usage();
				a = atoi(*spp++);
				switch (ft->frob) {
					case L_METRIC: ifr.ifr_metric = a; break;
					case L_MTU:    ifr.ifr_mtu    = a; break;
					case L_DATA:   ifr.ifr_data   = (caddr_t) a; break;
					default: error_msg_and_die("bugaboo");
				}

				if (ioctl(sockfd, ft->sflag, &ifr) < 0) {
					perror(ft->name);  /* imperfect */
					goterr++;
				}
				break;
			case 12:
			case 14:
				if (ft->action+sense==10 && *spp == NULL) {
					show_usage();
					break;
				}
				if (*spp != NULL) {
					safe_strncpy(host, *spp, (sizeof host));
					spp++;
					if (ft->frob == L_HWAD) {
						ecode = in_ether(host, &ifr.ifr_hwaddr);
					} else {
						switch (ft->frob) {
							case L_BROAD: d = &ifr.ifr_broadaddr; break;
							case L_DEST:  d = &ifr.ifr_dstaddr;   break;
							case L_MASK:  d = &ifr.ifr_netmask;   break;
							default: error_msg_and_die("bugaboo");
						}
						ecode = INET_resolve(host, (struct sockaddr_in *) d);
					}
					if (ecode < 0 || ioctl(sockfd, ft->sflag, &ifr) < 0) {
						perror(ft->name);  /* imperfect */
						goterr++;
					}
				}
				if (ft->flag != 0) {
					goterr |= set_flag(ifr.ifr_name, ft->flag);
				}
				break;
			default:
				show_usage();
			} /* end of switch */
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

        return goterr;
}

