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
 * $Id: route.c,v 1.26 2004/03/19 23:27:08 mjn3 Exp $
 *
 * displayroute() code added by Vladimir N. Oleynik <dzo@simtreas.ru>
 * adjustments by Larry Doolittle  <LRDoolittle@lbl.gov>
 *
 * IPV6 support added by Bart Visscher <magick@linux-fan.com>
 */

/* 2004/03/09  Manuel Novoa III <mjn3@codepoet.org>
 *
 * Rewritten to fix several bugs, add additional error checking, and
 * remove ridiculous amounts of bloat.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include <net/if.h>
#include "busybox.h"
#include "inet_common.h"

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

#if defined (SIOCADDRTOLD) || defined (RTF_IRTT)	/* route */
#define HAVE_NEW_ADDRT 1
#endif

#if HAVE_NEW_ADDRT
#define mask_in_addr(x) (((struct sockaddr_in *)&((x).rt_genmask))->sin_addr.s_addr)
#define full_mask(x) (x)
#else
#define mask_in_addr(x) ((x).rt_genmask)
#define full_mask(x) (((struct sockaddr_in *)&(x))->sin_addr.s_addr)
#endif

/* The RTACTION entries must agree with tbl_verb[] below! */
#define RTACTION_ADD		1
#define RTACTION_DEL		2

/* For the various tbl_*[] arrays, the 1st byte is the offset to
 * the next entry and the 2nd byte is return value. */

#define NET_FLAG			1
#define HOST_FLAG			2

/* We remap '-' to '#' to avoid problems with getopt. */
static const char tbl_hash_net_host[] =
	"\007\001#net\0"
/* 	"\010\002#host\0" */
	"\007\002#host"				/* Since last, we can save a byte. */
;

#define KW_TAKES_ARG		020
#define KW_SETS_FLAG		040

#define KW_IPVx_METRIC		020
#define KW_IPVx_NETMASK		021
#define KW_IPVx_GATEWAY		022
#define KW_IPVx_MSS			023
#define KW_IPVx_WINDOW		024
#define KW_IPVx_IRTT		025
#define KW_IPVx_DEVICE		026

#define KW_IPVx_FLAG_ONLY	040
#define KW_IPVx_REJECT		040
#define KW_IPVx_MOD			041
#define KW_IPVx_DYN			042
#define KW_IPVx_REINSTATE	043

static const char tbl_ipvx[] =
	/* 020 is the "takes an arg" bit */
#if HAVE_NEW_ADDRT
	"\011\020metric\0"
#endif
	"\012\021netmask\0"
	"\005\022gw\0"
	"\012\022gateway\0"
	"\006\023mss\0"
	"\011\024window\0"
#ifdef RTF_IRTT
	"\007\025irtt\0"
#endif
	"\006\026dev\0"
	"\011\026device\0"
	/* 040 is the "sets a flag" bit - MUST match flags_ipvx[] values below. */
#ifdef RTF_REJECT
	"\011\040reject\0"
#endif
	"\006\041mod\0"
	"\006\042dyn\0"
/* 	"\014\043reinstate\0" */
	"\013\043reinstate"			/* Since last, we can save a byte. */
;

static const int flags_ipvx[] = { /* MUST match tbl_ipvx[] values above. */
#ifdef RTF_REJECT
	RTF_REJECT,
#endif
	RTF_MODIFIED,
	RTF_DYNAMIC,
	RTF_REINSTATE
};

static int kw_lookup(const char *kwtbl, char ***pargs)
{
	if (**pargs) {
		do {
			if (strcmp(kwtbl+2, **pargs) == 0) { /* Found a match. */
				*pargs += 1;
				if (kwtbl[1] & KW_TAKES_ARG) {
					if (!**pargs) {	/* No more args! */
						bb_show_usage();
					}
					*pargs += 1; /* Calling routine will use args[-1]. */
				}
				return kwtbl[1];
			}
			kwtbl += *kwtbl;
		} while (*kwtbl);
	}
	return 0;
}

/* Add or delete a route, depending on action. */

static void INET_setroute(int action, char **args)
{
	struct rtentry rt;
	const char *netmask;
	int skfd, isnet, xflag;

	assert((action == RTACTION_ADD) || (action == RTACTION_DEL));

	/* Grab the -net or -host options.  Remember they were transformed. */
	xflag = kw_lookup(tbl_hash_net_host, &args);

	/* If we did grab -net or -host, make sure we still have an arg left. */
	if (*args == NULL) {
		bb_show_usage();
	}

	/* Clean out the RTREQ structure. */
	memset((char *) &rt, 0, sizeof(struct rtentry));

	{
		const char *target = *args++;

		/* Prefer hostname lookup is -host flag (xflag==1) was given. */
 		isnet = INET_resolve(target, (struct sockaddr_in *) &rt.rt_dst,
							 (xflag & HOST_FLAG));
		if (isnet < 0) {
			bb_error_msg_and_die("resolving %s", target);
		}

	}

	if (xflag) {		/* Reinit isnet if -net or -host was specified. */
		isnet = (xflag & NET_FLAG);
	}

	/* Fill in the other fields. */
	rt.rt_flags = ((isnet) ? RTF_UP : (RTF_UP | RTF_HOST));

	netmask = bb_INET_default;

	while (*args) {
		int k = kw_lookup(tbl_ipvx, &args);
		const char *args_m1 = args[-1];

		if (k & KW_IPVx_FLAG_ONLY) {
			rt.rt_flags |= flags_ipvx[k & 3];
			continue;
		}

#if HAVE_NEW_ADDRT
		if (k == KW_IPVx_METRIC) {
			rt.rt_metric = bb_xgetularg10(args_m1) + 1;
			continue;
		}
#endif

		if (k == KW_IPVx_NETMASK) {
			struct sockaddr mask;

			if (mask_in_addr(rt)) {
				bb_show_usage();
			}

			netmask = args_m1;
			isnet = INET_resolve(netmask, (struct sockaddr_in *) &mask, 0);
			if (isnet < 0) {
				bb_error_msg_and_die("resolving %s", netmask);
			}
			rt.rt_genmask = full_mask(mask);
			continue;
		}

		if (k == KW_IPVx_GATEWAY) {
			if (rt.rt_flags & RTF_GATEWAY) {
				bb_show_usage();
			}

			isnet = INET_resolve(args_m1,
								 (struct sockaddr_in *) &rt.rt_gateway, 1);
			rt.rt_flags |= RTF_GATEWAY;

			if (isnet) {
				if (isnet < 0) {
					bb_error_msg_and_die("resolving %s", args_m1);
				}
				bb_error_msg_and_die("gateway %s is a NETWORK", args_m1);
			}
			continue;
		}

		if (k == KW_IPVx_MSS) {	/* Check valid MSS bounds. */
			rt.rt_flags |= RTF_MSS;
			rt.rt_mss = bb_xgetularg10_bnd(args_m1, 64, 32768);
			continue;
		}

		if (k == KW_IPVx_WINDOW) {	/* Check valid window bounds. */
			rt.rt_flags |= RTF_WINDOW;
			rt.rt_window = bb_xgetularg10_bnd(args_m1, 128, INT_MAX);
			continue;
		}

#ifdef RTF_IRTT
		if (k == KW_IPVx_IRTT) {
			rt.rt_flags |= RTF_IRTT;
			rt.rt_irtt = bb_xgetularg10(args_m1);
			rt.rt_irtt *= (sysconf(_SC_CLK_TCK) / 100);	/* FIXME */
#if 0					/* FIXME: do we need to check anything of this? */
			if (rt.rt_irtt < 1 || rt.rt_irtt > (120 * HZ)) {
				bb_error_msg_and_die("bad irtt");
			}
#endif
			continue;
		}
#endif

		/* Device is special in that it can be the last arg specified
		 * and doesn't requre the dev/device keyword in that case. */
		if (!rt.rt_dev && ((k == KW_IPVx_DEVICE) || (!k && !*++args))) {
			/* Don't use args_m1 here since args may have changed! */
			rt.rt_dev = args[-1];
			continue;
		}

		/* Nothing matched. */
		bb_show_usage();
	}

#ifdef RTF_REJECT
	if ((rt.rt_flags & RTF_REJECT) && !rt.rt_dev) {
		rt.rt_dev = "lo";
	}
#endif

	/* sanity checks.. */
	if (mask_in_addr(rt)) {
		unsigned long mask = mask_in_addr(rt);

		mask = ~ntohl(mask);
		if ((rt.rt_flags & RTF_HOST) && mask != 0xffffffff) {
			bb_error_msg_and_die("netmask %.8x and host route conflict",
								 (unsigned int) mask);
		}
		if (mask & (mask + 1)) {
			bb_error_msg_and_die("bogus netmask %s", netmask);
		}
		mask = ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr;
		if (mask & ~mask_in_addr(rt)) {
			bb_error_msg_and_die("netmask and route address conflict");
		}
	}

	/* Fill out netmask if still unset */
	if ((action == RTACTION_ADD) && (rt.rt_flags & RTF_HOST)) {
		mask_in_addr(rt) = 0xffffffff;
	}

	/* Create a socket to the INET kernel. */
	if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		bb_perror_msg_and_die("socket");
	}

	if (ioctl(skfd, ((action==RTACTION_ADD) ? SIOCADDRT : SIOCDELRT), &rt)<0) {
		bb_perror_msg_and_die("SIOC[ADD|DEL]RT");
	}

	/* Don't bother closing, as we're exiting after we return anyway. */
	/* close(skfd); */
}

#ifdef CONFIG_FEATURE_IPV6

static void INET6_setroute(int action, char **args)
{
	struct sockaddr_in6 sa6;
	struct in6_rtmsg rt;
	int prefix_len, skfd;
	const char *devname;

	assert((action == RTACTION_ADD) || (action == RTACTION_DEL));

	{
		/* We know args isn't NULL from the check in route_main. */
		const char *target = *args++;

		if (strcmp(target, "default") == 0) {
			prefix_len = 0;
			memset(&sa6, 0, sizeof(sa6));
		} else {
			char *cp;
			if ((cp = strchr(target, '/'))) { /* Yes... const to non is ok. */
				*cp = 0;
				prefix_len = bb_xgetularg10_bnd(cp+1, 0, 128);
			} else {
				prefix_len = 128;
			}
			if (INET6_resolve(target, (struct sockaddr_in6 *) &sa6) < 0) {
				bb_error_msg_and_die("resolving %s", target);
			}
		}
	}

	/* Clean out the RTREQ structure. */
	memset((char *) &rt, 0, sizeof(struct in6_rtmsg));

	memcpy(&rt.rtmsg_dst, sa6.sin6_addr.s6_addr, sizeof(struct in6_addr));

	/* Fill in the other fields. */
	rt.rtmsg_dst_len = prefix_len;
	rt.rtmsg_flags = ((prefix_len == 128) ? (RTF_UP|RTF_HOST) : RTF_UP);
	rt.rtmsg_metric = 1;

	devname = NULL;

	while (*args) {
		int k = kw_lookup(tbl_ipvx, &args);
		const char *args_m1 = args[-1];

		if ((k == KW_IPVx_MOD) || (k == KW_IPVx_DYN)) {
			rt.rtmsg_flags |= flags_ipvx[k & 3];
			continue;
		}

		if (k == KW_IPVx_METRIC) {
			rt.rtmsg_metric = bb_xgetularg10(args_m1);
			continue;
		}

		if (k == KW_IPVx_GATEWAY) {
			if (rt.rtmsg_flags & RTF_GATEWAY) {
				bb_show_usage();
			}

			if (INET6_resolve(args_m1, (struct sockaddr_in6 *) &sa6) < 0) {
				bb_error_msg_and_die("resolving %s", args_m1);
			}
			memcpy(&rt.rtmsg_gateway, sa6.sin6_addr.s6_addr,
				   sizeof(struct in6_addr));
			rt.rtmsg_flags |= RTF_GATEWAY;
			continue;
		}

		/* Device is special in that it can be the last arg specified
		 * and doesn't requre the dev/device keyword in that case. */
		if (!devname && ((k == KW_IPVx_DEVICE) || (!k && !*++args))) {
			/* Don't use args_m1 here since args may have changed! */
			devname = args[-1];
			continue;
		}

		/* Nothing matched. */
		bb_show_usage();
	}

	/* Create a socket to the INET6 kernel. */
	if ((skfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		bb_perror_msg_and_die("socket");
	}

	rt.rtmsg_ifindex = 0;

	if (devname) {
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, devname);

		if (ioctl(skfd, SIOGIFINDEX, &ifr) < 0) {
			bb_perror_msg_and_die("SIOGIFINDEX");
		}
		rt.rtmsg_ifindex = ifr.ifr_ifindex;
	}

	/* Tell the kernel to accept this route. */
	if (ioctl(skfd, ((action==RTACTION_ADD) ? SIOCADDRT : SIOCDELRT), &rt)<0) {
		bb_perror_msg_and_die("SIOC[ADD|DEL]RT");
	}

	/* Don't bother closing, as we're exiting after we return anyway. */
	/* close(skfd); */
}
#endif

static const unsigned int flagvals[] = { /* Must agree with flagchars[]. */
	RTF_GATEWAY,
	RTF_HOST,
	RTF_REINSTATE,
	RTF_DYNAMIC,
	RTF_MODIFIED,
#ifdef CONFIG_FEATURE_IPV6
	RTF_DEFAULT,
	RTF_ADDRCONF,
	RTF_CACHE
#endif
};

#define IPV4_MASK (RTF_GATEWAY|RTF_HOST|RTF_REINSTATE|RTF_DYNAMIC|RTF_MODIFIED)
#define IPV6_MASK (RTF_GATEWAY|RTF_HOST|RTF_DEFAULT|RTF_ADDRCONF|RTF_CACHE)

static const char flagchars[] = 		/* Must agree with flagvals[]. */
	"GHRDM"
#ifdef CONFIG_FEATURE_IPV6
	"DAC"
#endif
;

static
#ifndef CONFIG_FEATURE_IPV6
__inline
#endif
void set_flags(char *flagstr, int flags)
{
	int i;

	*flagstr++ = 'U';

	for (i=0 ; (*flagstr = flagchars[i]) != 0 ; i++) {
		if (flags & flagvals[i]) {
			++flagstr;
		}
	}
}

void displayroutes(int noresolve, int netstatfmt)
{
	char devname[64], flags[16], sdest[16], sgw[16];
	unsigned long int d, g, m;
	int flgs, ref, use, metric, mtu, win, ir;
	struct sockaddr_in s_addr;
	struct in_addr mask;

	FILE *fp = bb_xfopen("/proc/net/route", "r");

	bb_printf("Kernel IP routing table\n"
			  "Destination     Gateway         Genmask"
			  "         Flags %s Iface\n",
			  netstatfmt ? "  MSS Window  irtt" : "Metric Ref    Use");

	if (fscanf(fp, "%*[^\n]\n") < 0) { /* Skip the first line. */
		goto ERROR;		   /* Empty or missing line, or read error. */
	}
	while (1) {
		int r;
		r = fscanf(fp, "%63s%lx%lx%X%d%d%d%lx%d%d%d\n",
				   devname, &d, &g, &flgs, &ref, &use, &metric, &m,
				   &mtu, &win, &ir);
		if (r != 11) {
			if ((r < 0) && feof(fp)) { /* EOF with no (nonspace) chars read. */
				break;
			}
		ERROR:
			bb_error_msg_and_die("fscanf");
		}

		if (!(flgs & RTF_UP)) { /* Skip interfaces that are down. */
			continue;
		}

		set_flags(flags, (flgs & IPV4_MASK));
#ifdef RTF_REJECT
		if (flgs & RTF_REJECT) {
			flags[0] = '!';
		}
#endif

		memset(&s_addr, 0, sizeof(struct sockaddr_in));
		s_addr.sin_family = AF_INET;
		s_addr.sin_addr.s_addr = d;
		INET_rresolve(sdest, sizeof(sdest), &s_addr,
					  (noresolve | 0x8000), m);	/* Default instead of *. */

		s_addr.sin_addr.s_addr = g;
		INET_rresolve(sgw, sizeof(sgw), &s_addr,
					  (noresolve | 0x4000), m);	/* Host instead of net. */

		mask.s_addr = m;
		bb_printf("%-16s%-16s%-16s%-6s", sdest, sgw, inet_ntoa(mask), flags);
		if (netstatfmt) {
			bb_printf("%5d %-5d %6d %s\n", mtu, win, ir, devname);
		} else {
			bb_printf("%-6d %-2d %7d %s\n", metric, ref, use, devname);
		}
	}
}

#ifdef CONFIG_FEATURE_IPV6

static void INET6_displayroutes(int noresolve)
{
	char addr6[128], naddr6[128];
	/* In addr6x, we store both 40-byte ':'-delimited ipv6 addresses.
	 * We read the non-delimited strings into the tail of the buffer
	 * using fscanf and then modify the buffer by shifting forward
	 * while inserting ':'s and the nul terminator for the first string.
	 * Hence the strings are at addr6x and addr6x+40.  This generates
	 * _much_ less code than the previous (upstream) approach. */
	char addr6x[80];
	char iface[16], flags[16];
	int iflags, metric, refcnt, use, prefix_len, slen;
	struct sockaddr_in6 snaddr6;

	FILE *fp = bb_xfopen("/proc/net/ipv6_route", "r");

	bb_printf("Kernel IPv6 routing table\n%-44s%-40s"
			  "Flags Metric Ref    Use Iface\n",
			  "Destination", "Next Hop");

	while (1) {
		int r;
		r = fscanf(fp, "%32s%x%*s%x%32s%x%x%x%x%s\n",
				   addr6x+14, &prefix_len, &slen, addr6x+40+7,
				   &metric, &use, &refcnt, &iflags, iface);
		if (r != 9) {
			if ((r < 0) && feof(fp)) { /* EOF with no (nonspace) chars read. */
				break;
			}
		ERROR:
			bb_error_msg_and_die("fscanf");
		}

		/* Do the addr6x shift-and-insert changes to ':'-delimit addresses.
		 * For now, always do this to validate the proc route format, even
		 * if the interface is down. */
		{
			int i = 0;
			char *p = addr6x+14;

			do {
				if (!*p) {
					if (i==40) { /* nul terminator for 1st address? */
						addr6x[39] = 0;	/* Fixup... need 0 instead of ':'. */
						++p;	/* Skip and continue. */
						continue;
					}
					goto ERROR;
				}
				addr6x[i++] = *p++;
				if (!((i+1)%5)) {
					addr6x[i++] = ':';
				}
			} while (i < 40+28+7);
		}

		if (!(iflags & RTF_UP)) { /* Skip interfaces that are down. */
			continue;
		}

		set_flags(flags, (iflags & IPV6_MASK));

		r = 0;
		do {
			inet_pton(AF_INET6, addr6x + r,
					  (struct sockaddr *) &snaddr6.sin6_addr);
			snaddr6.sin6_family = AF_INET6;
			INET6_rresolve(naddr6, sizeof(naddr6),
						   (struct sockaddr_in6 *) &snaddr6,
#if 0
						   (noresolve | 0x8000) /* Default instead of *. */
#else
						   0x0fff /* Apparently, upstream never resolves. */
#endif
						   );

			if (!r) {			/* 1st pass */
				snprintf(addr6, sizeof(addr6), "%s/%d", naddr6, prefix_len);
				r += 40;
			} else {			/* 2nd pass */
				/* Print the info. */
				bb_printf("%-43s %-39s %-5s %-6d %-2d %7d %-8s\n",
						  addr6, naddr6, flags, metric, refcnt, use, iface);
				break;
			}
		} while (1);
	}
}

#endif

#define ROUTE_OPT_A			0x01
#define ROUTE_OPT_n			0x02
#define ROUTE_OPT_e			0x04
#define ROUTE_OPT_INET6		0x08 /* Not an actual option. See below. */

/* 1st byte is offset to next entry offset.  2nd byte is return value. */
static const char tbl_verb[] = 	/* 2nd byte matches RTACTION_* code */
	"\006\001add\0"
	"\006\002del\0"
/* 	"\011\002delete\0" */
	"\010\002delete"			/* Since last, we can save a byte. */
;

int route_main(int argc, char **argv)
{
	unsigned long opt;
	int what;
	char *family;

	/* First, remap '-net' and '-host' to avoid getopt problems. */
	{
		char **p = argv;

		while (*++p) {
			if ((strcmp(*p, "-net") == 0) || (strcmp(*p, "-host") == 0)) {
				p[0][0] = '#';
			}
		}
	}

	opt = bb_getopt_ulflags(argc, argv, "A:ne", &family);

	if ((opt & ROUTE_OPT_A) && strcmp(family, "inet")) {
#ifdef CONFIG_FEATURE_IPV6
		if (strcmp(family, "inet6") == 0) {
			opt |= ROUTE_OPT_INET6;	/* Set flag for ipv6. */
		} else
#endif
		bb_show_usage();
	}

	argv += optind;

	/* No more args means display the routing table. */
	if (!*argv) {
		int noresolve = (opt & ROUTE_OPT_n) ? 0x0fff : 0;
#ifdef CONFIG_FEATURE_IPV6
		if (opt & ROUTE_OPT_INET6)
			INET6_displayroutes(noresolve);
		else
#endif
			displayroutes(noresolve, opt & ROUTE_OPT_e);

		bb_xferror_stdout();
		bb_fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	/* Check verb.  At the moment, must be add, del, or delete. */
	what = kw_lookup(tbl_verb, &argv);
	if (!what || !*argv) {		/* Unknown verb or no more args. */
		bb_show_usage();
	}

#ifdef CONFIG_FEATURE_IPV6
	if (opt & ROUTE_OPT_INET6)
		INET6_setroute(what, argv);
	else
#endif
		INET_setroute(what, argv);

	return EXIT_SUCCESS;
}
