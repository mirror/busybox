/* route
 *
 * Similar to the standard Unix route, but with only the necessary
 * parts for AF_INET and AF_INET6
 *
 * Bjorn Wesen, Axis Communications AB
 *
 * Author of the original route:
 *              Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *              (derived from FvK's 'route.c     1.70    01/04/94')
 *
 * This program is free software; you can redistribute it
 * and/or  modify it under  the terms of  the GNU General
 * Public  License as  published  by  the  Free  Software
 * Foundation;  either  version 2 of the License, or  (at
 * your option) any later version.
 *
 * $Id: route.c,v 1.22 2003/03/19 09:12:39 mjn3 Exp $
 *
 * displayroute() code added by Vladimir N. Oleynik <dzo@simtreas.ru>
 * adjustments by Larry Doolittle  <LRDoolittle@lbl.gov>
 *
 * IPV6 support added by Bart Visscher <magick@linux-fan.com>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include "inet_common.h"
#include <net/route.h>
#include <net/if.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <ctype.h>
#include "busybox.h"

#define _(x) x

#define RTACTION_ADD   1
#define RTACTION_DEL   2
#define RTACTION_HELP  3
#define RTACTION_FLUSH 4
#define RTACTION_SHOW  5

#define E_NOTFOUND      8
#define E_SOCK          7
#define E_LOOKUP        6
#define E_VERSION       5
#define E_USAGE         4
#define E_OPTERR        3
#define E_INTERN        2
#define E_NOSUPP        1

#if defined (SIOCADDRTOLD) || defined (RTF_IRTT)	/* route */
#define HAVE_NEW_ADDRT 1
#endif
#ifdef RTF_IRTT			/* route */
#define HAVE_RTF_IRTT 1
#endif
#ifdef RTF_REJECT		/* route */
#define HAVE_RTF_REJECT 1
#endif

#if HAVE_NEW_ADDRT
#define mask_in_addr(x) (((struct sockaddr_in *)&((x).rt_genmask))->sin_addr.s_addr)
#define full_mask(x) (x)
#else
#define mask_in_addr(x) ((x).rt_genmask)
#define full_mask(x) (((struct sockaddr_in *)&(x))->sin_addr.s_addr)
#endif



/* add or delete a route depending on action */

static int INET_setroute(int action, int options, char **args)
{
	struct rtentry rt;
	char target[128], gateway[128] = "NONE";
	const char *netmask = bb_INET_default;
	int xflag, isnet;
	int skfd;

	xflag = 0;

	if (*args == NULL)
		bb_show_usage();
	if (strcmp(*args, "-net") == 0) {
		xflag = 1;
		args++;
	} else if (strcmp(*args, "-host") == 0) {
		xflag = 2;
		args++;
	}
	if (*args == NULL)
		bb_show_usage();
	safe_strncpy(target, *args++, (sizeof target));

	/* Clean out the RTREQ structure. */
	memset((char *) &rt, 0, sizeof(struct rtentry));


	if ((isnet =
		 INET_resolve(target, (struct sockaddr_in *) &rt.rt_dst,
					  xflag != 1)) < 0) {
		bb_error_msg(_("can't resolve %s"), target);
		return EXIT_FAILURE;	/* XXX change to E_something */
	}

	switch (xflag) {
	case 1:
		isnet = 1;
		break;

	case 2:
		isnet = 0;
		break;

	default:
		break;
	}

	/* Fill in the other fields. */
	rt.rt_flags = (RTF_UP | RTF_HOST);
	if (isnet)
		rt.rt_flags &= ~RTF_HOST;

	while (*args) {
		if (strcmp(*args, "metric") == 0) {
			int metric;

			args++;
			if (!*args || !isdigit(**args))
				bb_show_usage();
			metric = atoi(*args);
#if HAVE_NEW_ADDRT
			rt.rt_metric = metric + 1;
#else
			ENOSUPP("inet_setroute", "NEW_ADDRT (metric)");	/* XXX Fixme */
#endif
			args++;
			continue;
		}

		if (strcmp(*args, "netmask") == 0) {
			struct sockaddr mask;

			args++;
			if (!*args || mask_in_addr(rt))
				bb_show_usage();
			netmask = *args;
			if ((isnet =
				 INET_resolve(netmask, (struct sockaddr_in *) &mask,
							  0)) < 0) {
				bb_error_msg(_("can't resolve netmask %s"), netmask);
				return E_LOOKUP;
			}
			rt.rt_genmask = full_mask(mask);
			args++;
			continue;
		}

		if (strcmp(*args, "gw") == 0 || strcmp(*args, "gateway") == 0) {
			args++;
			if (!*args)
				bb_show_usage();
			if (rt.rt_flags & RTF_GATEWAY)
				bb_show_usage();
			safe_strncpy(gateway, *args, (sizeof gateway));
			if ((isnet =
				 INET_resolve(gateway, (struct sockaddr_in *) &rt.rt_gateway,
							  1)) < 0) {
				bb_error_msg(_("can't resolve gw %s"), gateway);
				return E_LOOKUP;
			}
			if (isnet) {
				bb_error_msg(_("%s: cannot use a NETWORK as gateway!"), gateway);
				return E_OPTERR;
			}
			rt.rt_flags |= RTF_GATEWAY;
			args++;
			continue;
		}

		if (strcmp(*args, "mss") == 0) {
			args++;
			rt.rt_flags |= RTF_MSS;
			if (!*args)
				bb_show_usage();
			rt.rt_mss = atoi(*args);
			args++;
			if (rt.rt_mss < 64 || rt.rt_mss > 32768) {
				bb_error_msg(_("Invalid MSS."));
				return E_OPTERR;
			}
			continue;
		}

		if (strcmp(*args, "window") == 0) {
			args++;
			if (!*args)
				bb_show_usage();
			rt.rt_flags |= RTF_WINDOW;
			rt.rt_window = atoi(*args);
			args++;
			if (rt.rt_window < 128) {
				bb_error_msg(_("Invalid window."));
				return E_OPTERR;
			}
			continue;
		}

		if (strcmp(*args, "irtt") == 0) {
			args++;
			if (!*args)
				bb_show_usage();
			args++;
#if HAVE_RTF_IRTT
			rt.rt_flags |= RTF_IRTT;
			rt.rt_irtt = atoi(*(args - 1));
			rt.rt_irtt *= (sysconf(_SC_CLK_TCK) / 100);	/* FIXME */
#if 0					/* FIXME: do we need to check anything of this? */
			if (rt.rt_irtt < 1 || rt.rt_irtt > (120 * HZ)) {
				bb_error_msg(_("Invalid initial rtt."));
				return E_OPTERR;
			}
#endif
#else
			ENOSUPP("inet_setroute", "RTF_IRTT");	/* XXX Fixme */
#endif
			continue;
		}

		if (strcmp(*args, "reject") == 0) {
			args++;
#if HAVE_RTF_REJECT
			rt.rt_flags |= RTF_REJECT;
#else
			ENOSUPP("inet_setroute", "RTF_REJECT");	/* XXX Fixme */
#endif
			continue;
		}
		if (strcmp(*args, "mod") == 0) {
			args++;
			rt.rt_flags |= RTF_MODIFIED;
			continue;
		}
		if (strcmp(*args, "dyn") == 0) {
			args++;
			rt.rt_flags |= RTF_DYNAMIC;
			continue;
		}
		if (strcmp(*args, "reinstate") == 0) {
			args++;
			rt.rt_flags |= RTF_REINSTATE;
			continue;
		}
		if (strcmp(*args, "device") == 0 || strcmp(*args, "dev") == 0) {
			args++;
			if (rt.rt_dev || *args == NULL)
				bb_show_usage();
			rt.rt_dev = *args++;
			continue;
		}
		/* nothing matches */
		if (!rt.rt_dev) {
			rt.rt_dev = *args++;
			if (*args)
				bb_show_usage();	/* must be last to catch typos */
		} else {
			bb_show_usage();
		}
	}

#if HAVE_RTF_REJECT
	if ((rt.rt_flags & RTF_REJECT) && !rt.rt_dev)
		rt.rt_dev = "lo";
#endif

	/* sanity checks.. */
	if (mask_in_addr(rt)) {
		unsigned long mask = mask_in_addr(rt);

		mask = ~ntohl(mask);
		if ((rt.rt_flags & RTF_HOST) && mask != 0xffffffff) {
			bb_error_msg(_("netmask %.8x doesn't make sense with host route"),
					  (unsigned int) mask);
			return E_OPTERR;
		}
		if (mask & (mask + 1)) {
			bb_error_msg(_("bogus netmask %s"), netmask);
			return E_OPTERR;
		}
		mask = ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr;
		if (mask & ~mask_in_addr(rt)) {
			bb_error_msg(_("netmask doesn't match route address"));
			return E_OPTERR;
		}
	}
	/* Fill out netmask if still unset */
	if ((action == RTACTION_ADD) && rt.rt_flags & RTF_HOST)
		mask_in_addr(rt) = 0xffffffff;

	/* Create a socket to the INET kernel. */
	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return E_SOCK;
	}
	/* Tell the kernel to accept this route. */
	if (action == RTACTION_DEL) {
		if (ioctl(skfd, SIOCDELRT, &rt) < 0) {
			perror("SIOCDELRT");
			close(skfd);
			return E_SOCK;
		}
	} else {
		if (ioctl(skfd, SIOCADDRT, &rt) < 0) {
			perror("SIOCADDRT");
			close(skfd);
			return E_SOCK;
		}
	}

	/* Close the socket. */
	(void) close(skfd);
	return EXIT_SUCCESS;
}

#ifdef CONFIG_FEATURE_IPV6
static int INET6_setroute(int action, int options, char **args)
{
	struct in6_rtmsg rt;
	struct ifreq ifr;
	struct sockaddr_in6 sa6;
	char target[128], gateway[128] = "NONE";
	int metric, prefix_len;
	char *devname = NULL;
	char *cp;
	int skfd;

	if (*args == NULL)
		bb_show_usage();

	strcpy(target, *args++);
	if (!strcmp(target, "default")) {
		prefix_len = 0;
		memset(&sa6, 0, sizeof(sa6));
	} else {
		if ((cp = strchr(target, '/'))) {
			prefix_len = atol(cp + 1);
			if ((prefix_len < 0) || (prefix_len > 128))
				bb_show_usage();
			*cp = 0;
		} else {
			prefix_len = 128;
		}
		if (INET6_resolve(target, (struct sockaddr_in6 *) &sa6) < 0) {
			bb_error_msg(_("can't resolve %s"), target);
			return EXIT_FAILURE;	/* XXX change to E_something */
		}
	}

	/* Clean out the RTREQ structure. */
	memset((char *) &rt, 0, sizeof(struct in6_rtmsg));

	memcpy(&rt.rtmsg_dst, sa6.sin6_addr.s6_addr, sizeof(struct in6_addr));

	/* Fill in the other fields. */
	rt.rtmsg_flags = RTF_UP;
	if (prefix_len == 128)
		rt.rtmsg_flags |= RTF_HOST;
	rt.rtmsg_metric = 1;
	rt.rtmsg_dst_len = prefix_len;

	while (*args) {
		if (!strcmp(*args, "metric")) {

			args++;
			if (!*args || !isdigit(**args))
				bb_show_usage();
			metric = atoi(*args);
			rt.rtmsg_metric = metric;
			args++;
			continue;
		}
		if (!strcmp(*args, "gw") || !strcmp(*args, "gateway")) {
			args++;
			if (!*args)
				bb_show_usage();
			if (rt.rtmsg_flags & RTF_GATEWAY)
				bb_show_usage();
			strcpy(gateway, *args);
			if (INET6_resolve(gateway, (struct sockaddr_in6 *) &sa6) < 0) {
				bb_error_msg(_("can't resolve gw %s"), gateway);
				return (E_LOOKUP);
			}
			memcpy(&rt.rtmsg_gateway, sa6.sin6_addr.s6_addr,
				   sizeof(struct in6_addr));
			rt.rtmsg_flags |= RTF_GATEWAY;
			args++;
			continue;
		}
		if (!strcmp(*args, "mod")) {
			args++;
			rt.rtmsg_flags |= RTF_MODIFIED;
			continue;
		}
		if (!strcmp(*args, "dyn")) {
			args++;
			rt.rtmsg_flags |= RTF_DYNAMIC;
			continue;
		}
		if (!strcmp(*args, "device") || !strcmp(*args, "dev")) {
			args++;
			if (!*args)
				bb_show_usage();
		} else if (args[1])
			bb_show_usage();

		devname = *args;
		args++;
	}

	/* Create a socket to the INET6 kernel. */
	if ((skfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return (E_SOCK);
	}
	if (devname) {
		memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, devname);

		if (ioctl(skfd, SIOGIFINDEX, &ifr) < 0) {
			perror("SIOGIFINDEX");
			return (E_SOCK);
		}
		rt.rtmsg_ifindex = ifr.ifr_ifindex;
	} else
		rt.rtmsg_ifindex = 0;

	/* Tell the kernel to accept this route. */
	if (action == RTACTION_DEL) {
		if (ioctl(skfd, SIOCDELRT, &rt) < 0) {
			perror("SIOCDELRT");
			close(skfd);
			return (E_SOCK);
		}
	} else {
		if (ioctl(skfd, SIOCADDRT, &rt) < 0) {
			perror("SIOCADDRT");
			close(skfd);
			return (E_SOCK);
		}
	}

	/* Close the socket. */
	(void) close(skfd);
	return (0);
}
#endif

#ifndef RTF_UP
/* Keep this in sync with /usr/src/linux/include/linux/route.h */
#define RTF_UP          0x0001	/* route usable                 */
#define RTF_GATEWAY     0x0002	/* destination is a gateway     */
#define RTF_HOST        0x0004	/* host entry (net otherwise)   */
#define RTF_REINSTATE   0x0008	/* reinstate route after tmout  */
#define RTF_DYNAMIC     0x0010	/* created dyn. (by redirect)   */
#define RTF_MODIFIED    0x0020	/* modified dyn. (by redirect)  */
#define RTF_MTU         0x0040	/* specific MTU for this route  */
#ifndef RTF_MSS
#define RTF_MSS         RTF_MTU	/* Compatibility :-(            */
#endif
#define RTF_WINDOW      0x0080	/* per route window clamping    */
#define RTF_IRTT        0x0100	/* Initial round trip time      */
#define RTF_REJECT      0x0200	/* Reject route                 */
#endif

void displayroutes(int noresolve, int netstatfmt)
{
	char buff[256];
	int nl = 0;
	struct in_addr dest;
	struct in_addr gw;
	struct in_addr mask;
	int flgs, ref, use, metric, mtu, win, ir;
	char flags[64];
	unsigned long int d, g, m;

	char sdest[16], sgw[16];

	FILE *fp = bb_xfopen("/proc/net/route", "r");

	if (noresolve)
		noresolve = 0x0fff;

	printf("Kernel IP routing table\n");
	printf
		("Destination     Gateway         Genmask         Flags %s Iface\n",
		 netstatfmt ? "  MSS Window  irtt" : "Metric Ref    Use");

	while (fgets(buff, sizeof(buff), fp) != NULL) {
		if (nl) {
			int ifl = 0;
			int numeric;
			struct sockaddr_in s_addr;

			while (buff[ifl] != ' ' && buff[ifl] != '\t' && buff[ifl] != '\0')
				ifl++;
			buff[ifl] = 0;	/* interface */
			if (sscanf(buff + ifl + 1, "%lx%lx%X%d%d%d%lx%d%d%d",
					   &d, &g, &flgs, &ref, &use, &metric, &m, &mtu, &win,
					   &ir) != 10) {
				bb_error_msg_and_die("Unsuported kernel route format\n");
			}
			ifl = 0;	/* parse flags */
			if (flgs & RTF_UP) {
				if (flgs & RTF_REJECT)
					flags[ifl++] = '!';
				else
					flags[ifl++] = 'U';
				if (flgs & RTF_GATEWAY)
					flags[ifl++] = 'G';
				if (flgs & RTF_HOST)
					flags[ifl++] = 'H';
				if (flgs & RTF_REINSTATE)
					flags[ifl++] = 'R';
				if (flgs & RTF_DYNAMIC)
					flags[ifl++] = 'D';
				if (flgs & RTF_MODIFIED)
					flags[ifl++] = 'M';
				flags[ifl] = 0;
				dest.s_addr = d;
				gw.s_addr = g;
				mask.s_addr = m;
				memset(&s_addr, 0, sizeof(struct sockaddr_in));
				s_addr.sin_family = AF_INET;
				s_addr.sin_addr = dest;
				numeric = noresolve | 0x8000;	/* default instead of * */
				INET_rresolve(sdest, sizeof(sdest), &s_addr, numeric, m);
				numeric = noresolve | 0x4000;	/* host instead of net */
				s_addr.sin_addr = gw;
				INET_rresolve(sgw, sizeof(sgw), &s_addr, numeric, m);

				printf("%-16s%-16s%-16s%-6s", sdest, sgw, inet_ntoa(mask),
					   flags);
				if (netstatfmt)
					printf("%5d %-5d %6d %s\n", mtu, win, ir, buff);
				else
					printf("%-6d %-2d %7d %s\n", metric, ref, use, buff);
			}
		}
		nl++;
	}
}

#ifdef CONFIG_FEATURE_IPV6
static void INET6_displayroutes(int noresolve)
{
	char buff[256];
	char iface[16], flags[16];
	char addr6[128], naddr6[128];
	struct sockaddr_in6 saddr6, snaddr6;
	int iflags, metric, refcnt, use, prefix_len, slen;
	int numeric;

	char addr6p[8][5], saddr6p[8][5], naddr6p[8][5];

	FILE *fp = bb_xfopen("/proc/net/ipv6_route", "r");

	flags[0] = 'U';

	if (noresolve)
		noresolve = 0x0fff;
	numeric = noresolve | 0x8000;	/* default instead of * */

	printf("Kernel IPv6 routing table\n"
		   "Destination                                 "
		   "Next Hop                                "
		   "Flags Metric Ref    Use Iface\n");

	while (fgets(buff, sizeof(buff), fp) != NULL) {
		int ifl;

		if (sscanf(buff, "%4s%4s%4s%4s%4s%4s%4s%4s %02x "
				   "%4s%4s%4s%4s%4s%4s%4s%4s %02x "
				   "%4s%4s%4s%4s%4s%4s%4s%4s %08x %08x %08x %08x %s\n",
				   addr6p[0], addr6p[1], addr6p[2], addr6p[3],
				   addr6p[4], addr6p[5], addr6p[6], addr6p[7],
				   &prefix_len,
				   saddr6p[0], saddr6p[1], saddr6p[2], saddr6p[3],
				   saddr6p[4], saddr6p[5], saddr6p[6], saddr6p[7],
				   &slen,
				   naddr6p[0], naddr6p[1], naddr6p[2], naddr6p[3],
				   naddr6p[4], naddr6p[5], naddr6p[6], naddr6p[7],
				   &metric, &use, &refcnt, &iflags, iface) != 31) {
			bb_error_msg_and_die("Unsuported kernel route format\n");
		}

		ifl = 1;		/* parse flags */
		if (!(iflags & RTF_UP))
			continue;
		if (iflags & RTF_GATEWAY)
			flags[ifl++] = 'G';
		if (iflags & RTF_HOST)
			flags[ifl++] = 'H';
		if (iflags & RTF_DEFAULT)
			flags[ifl++] = 'D';
		if (iflags & RTF_ADDRCONF)
			flags[ifl++] = 'A';
		if (iflags & RTF_CACHE)
			flags[ifl++] = 'C';
		flags[ifl] = 0;

		/* Fetch and resolve the target address. */
		snprintf(addr6, sizeof(addr6), "%s:%s:%s:%s:%s:%s:%s:%s",
				 addr6p[0], addr6p[1], addr6p[2], addr6p[3],
				 addr6p[4], addr6p[5], addr6p[6], addr6p[7]);
		inet_pton(AF_INET6, addr6, (struct sockaddr *) &saddr6.sin6_addr);
		saddr6.sin6_family = AF_INET6;

		INET6_rresolve(addr6, sizeof(addr6), (struct sockaddr_in6 *) &saddr6,
					   numeric);
		snprintf(addr6, sizeof(addr6), "%s/%d", addr6, prefix_len);

		/* Fetch and resolve the nexthop address. */
		snprintf(naddr6, sizeof(naddr6), "%s:%s:%s:%s:%s:%s:%s:%s",
				 naddr6p[0], naddr6p[1], naddr6p[2], naddr6p[3],
				 naddr6p[4], naddr6p[5], naddr6p[6], naddr6p[7]);
		inet_pton(AF_INET6, naddr6, (struct sockaddr *) &snaddr6.sin6_addr);
		snaddr6.sin6_family = AF_INET6;

		INET6_rresolve(naddr6, sizeof(naddr6),
					   (struct sockaddr_in6 *) &snaddr6, numeric);

		/* Print the info. */
		printf("%-43s %-39s %-5s %-6d %-2d %7d %-8s\n",
			   addr6, naddr6, flags, metric, refcnt, use, iface);
	}
}
#endif

int route_main(int argc, char **argv)
{
	int opt;
	int what = 0;

#ifdef CONFIG_FEATURE_IPV6
	int af = AF_INET;
#endif

	if (!argv[1] || (argv[1][0] == '-')) {
		/* check options */
		int noresolve = 0;
		int extended = 0;

		while ((opt = getopt(argc, argv, "A:ne")) > 0) {
			switch (opt) {
			case 'n':
				noresolve = 1;
				break;
			case 'e':
				extended = 1;
				break;
			case 'A':
#ifdef CONFIG_FEATURE_IPV6
				if (strcmp(optarg, "inet6") == 0)
					af = AF_INET6;
				break;
#endif
			default:
				bb_show_usage();
			}
		}

#ifdef CONFIG_FEATURE_IPV6
		if (af == AF_INET6)
			INET6_displayroutes(*argv != NULL);
		else
#endif
			displayroutes(noresolve, extended);
		return EXIT_SUCCESS;
	} else {
		/* check verb */
		if (strcmp(argv[1], "add") == 0)
			what = RTACTION_ADD;
		else if (strcmp(argv[1], "del") == 0
				 || strcmp(argv[1], "delete") == 0)
			what = RTACTION_DEL;
		else if (strcmp(argv[1], "flush") == 0)
			what = RTACTION_FLUSH;
		else
			bb_show_usage();
	}

#ifdef CONFIG_FEATURE_IPV6
	if (af == AF_INET6)
		return INET6_setroute(what, 0, argv + 2);
#endif
	return INET_setroute(what, 0, argv + 2);
}
