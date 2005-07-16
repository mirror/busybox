/* route
 *
 * Similar to the standard Unix route, but with only the necessary
 * parts for AF_INET
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
 * displayroute() code added by Vladimir N. Oleynik <dzo@simtreas.ru>
 * adjustments by Larry Doolittle  <LRDoolittle@lbl.gov>
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/route.h>
#include <linux/param.h>  // HZ
#include <netinet/in.h>
#include <arpa/inet.h>
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

/* resolve XXX.YYY.ZZZ.QQQ -> binary */

static int
INET_resolve(char *name, struct sockaddr *sa)
{
	struct sockaddr_in *s_in = (struct sockaddr_in *)sa;

	s_in->sin_family = AF_INET;
	s_in->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (strcmp(name, "default")==0) {
		s_in->sin_addr.s_addr = INADDR_ANY;
		return 1;
	}
	/* Look to see if it's a dotted quad. */
	if (inet_aton(name, &s_in->sin_addr)) {
		return 0;
	}
	/* guess not.. */
	return -1;
}

#if defined (SIOCADDRTOLD) || defined (RTF_IRTT)        /* route */
#define HAVE_NEW_ADDRT 1
#endif
#ifdef RTF_IRTT                 /* route */
#define HAVE_RTF_IRTT 1
#endif
#ifdef RTF_REJECT               /* route */
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

static int
INET_setroute(int action, int options, char **args)
{
	struct rtentry rt;
	char target[128], gateway[128] = "NONE", netmask[128] = "default";
	int xflag, isnet;
	int skfd;

	xflag = 0;

	if (*args == NULL)
	    show_usage();
	if (strcmp(*args, "-net")==0) {
		xflag = 1;
		args++;
	} else if (strcmp(*args, "-host")==0) {
		xflag = 2;
		args++;
	}
	safe_strncpy(target, *args++, (sizeof target));

	/* Clean out the RTREQ structure. */
	memset((char *) &rt, 0, sizeof(struct rtentry));


	if ((isnet = INET_resolve(target, &rt.rt_dst)) < 0) {
		error_msg(_("can't resolve %s"), target);
		return EXIT_FAILURE;   /* XXX change to E_something */
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
		if (strcmp(*args, "metric")==0) {
			int metric;

			args++;
			if (!*args || !isdigit(**args))
				show_usage();
			metric = atoi(*args);
#if HAVE_NEW_ADDRT
			rt.rt_metric = metric + 1;
#else
			ENOSUPP("inet_setroute", "NEW_ADDRT (metric)");  /* XXX Fixme */
#endif
			args++;
			continue;
		}

		if (strcmp(*args, "netmask")==0) {
			struct sockaddr mask;

			args++;
			if (!*args || mask_in_addr(rt))
				show_usage();
			safe_strncpy(netmask, *args, (sizeof netmask));
			if ((isnet = INET_resolve(netmask, &mask)) < 0) {
				error_msg(_("can't resolve netmask %s"), netmask);
				return E_LOOKUP;
			}
			rt.rt_genmask = full_mask(mask);
			args++;
			continue;
		}

		if (strcmp(*args, "gw")==0 || strcmp(*args, "gateway")==0) {
			args++;
			if (!*args)
				show_usage();
			if (rt.rt_flags & RTF_GATEWAY)
				show_usage();
			safe_strncpy(gateway, *args, (sizeof gateway));
			if ((isnet = INET_resolve(gateway, &rt.rt_gateway)) < 0) {
				error_msg(_("can't resolve gw %s"), gateway);
				return E_LOOKUP;
			}
			if (isnet) {
				error_msg(
					_("%s: cannot use a NETWORK as gateway!"),
					gateway);
				return E_OPTERR;
			}
			rt.rt_flags |= RTF_GATEWAY;
			args++;
			continue;
		}

		if (strcmp(*args, "mss")==0) {
			args++;
			rt.rt_flags |= RTF_MSS;
			if (!*args)
				show_usage();
			rt.rt_mss = atoi(*args);
			args++;
			if (rt.rt_mss < 64 || rt.rt_mss > 32768) {
				error_msg(_("Invalid MSS."));
				return E_OPTERR;
			}
			continue;
		}

		if (strcmp(*args, "window")==0) {
			args++;
			if (!*args)
				show_usage();
			rt.rt_flags |= RTF_WINDOW;
			rt.rt_window = atoi(*args);
			args++;
			if (rt.rt_window < 128) {
				error_msg(_("Invalid window."));
				return E_OPTERR;
			}
			continue;
		}

		if (strcmp(*args, "irtt")==0) {
			args++;
			if (!*args)
				show_usage();
			args++;
#if HAVE_RTF_IRTT
			rt.rt_flags |= RTF_IRTT;
			rt.rt_irtt = atoi(*(args - 1));
			rt.rt_irtt *= (HZ / 100);       /* FIXME */
#if 0                           /* FIXME: do we need to check anything of this? */
			if (rt.rt_irtt < 1 || rt.rt_irtt > (120 * HZ)) {
				error_msg(_("Invalid initial rtt."));
				return E_OPTERR;
			}
#endif
#else
			ENOSUPP("inet_setroute", "RTF_IRTT"); /* XXX Fixme */
#endif
			continue;
		}

		if (strcmp(*args, "reject")==0) {
			args++;
#if HAVE_RTF_REJECT
			rt.rt_flags |= RTF_REJECT;
#else
			ENOSUPP("inet_setroute", "RTF_REJECT"); /* XXX Fixme */
#endif
			continue;
		}
		if (strcmp(*args, "mod")==0) {
			args++;
			rt.rt_flags |= RTF_MODIFIED;
			continue;
		}
		if (strcmp(*args, "dyn")==0) {
			args++;
			rt.rt_flags |= RTF_DYNAMIC;
			continue;
		}
		if (strcmp(*args, "reinstate")==0) {
			args++;
			rt.rt_flags |= RTF_REINSTATE;
			continue;
		}
		if (strcmp(*args, "device")==0 || strcmp(*args, "dev")==0) {
			args++;
			if (rt.rt_dev || *args == NULL)
				show_usage();
			rt.rt_dev = *args++;
			continue;
		}
		/* nothing matches */
		if (!rt.rt_dev) {
			rt.rt_dev = *args++;
			if (*args)
				show_usage();   /* must be last to catch typos */
		} else {
			show_usage();
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
			error_msg(
				_("netmask %.8x doesn't make sense with host route"),
				(unsigned int)mask);
			return E_OPTERR;
		}
		if (mask & (mask + 1)) {
			error_msg(_("bogus netmask %s"), netmask);
			return E_OPTERR;
		}
		mask = ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr;
		if (mask & ~mask_in_addr(rt)) {
			error_msg(_("netmask doesn't match route address"));
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

#ifndef RTF_UP
/* Keep this in sync with /usr/src/linux/include/linux/route.h */
#define RTF_UP          0x0001          /* route usable                 */
#define RTF_GATEWAY     0x0002          /* destination is a gateway     */
#define RTF_HOST        0x0004          /* host entry (net otherwise)   */
#define RTF_REINSTATE   0x0008          /* reinstate route after tmout  */
#define RTF_DYNAMIC     0x0010          /* created dyn. (by redirect)   */
#define RTF_MODIFIED    0x0020          /* modified dyn. (by redirect)  */
#define RTF_MTU         0x0040          /* specific MTU for this route  */
#ifndef RTF_MSS
#define RTF_MSS         RTF_MTU         /* Compatibility :-(            */
#endif
#define RTF_WINDOW      0x0080          /* per route window clamping    */
#define RTF_IRTT        0x0100          /* Initial round trip time      */
#define RTF_REJECT      0x0200          /* Reject route                 */
#endif

static void displayroutes(int numeric)
{
	char buff[256];
	int  nl = 0 ;
	struct in_addr dest;
	struct in_addr gw;
	struct in_addr mask;
	int flgs, ref, use, metric;
	char flags[64];
	unsigned long int d,g,m;

	char sdest[16], sgw[16];


	FILE *fp = xfopen("/proc/net/route", "r");

	while( fgets(buff, sizeof(buff), fp) != NULL ) {
		if(nl) {
			int ifl = 0;
			while(buff[ifl]!=' ' && buff[ifl]!='\t' && buff[ifl]!='\0')
				ifl++;
			buff[ifl]=0;    /* interface */
			if(sscanf(buff+ifl+1, "%lx%lx%X%d%d%d%lx",
			   &d, &g, &flgs, &ref, &use, &metric, &m)!=7) {
				error_msg_and_die( "Unsuported kernel route format\n");
			}
			if(nl==1)
			printf("Kernel IP routing table\n"
				"Destination     Gateway         Genmask         Flags Metric Ref    Use Iface\n");

			ifl = 0;        /* parse flags */
 			if(flgs&RTF_UP) {
 				if(flgs&RTF_REJECT)
 					flags[ifl++]='!';
 				else
 					flags[ifl++]='U';
 				if(flgs&RTF_GATEWAY)
 					flags[ifl++]='G';
 				if(flgs&RTF_HOST)
 					flags[ifl++]='H';
 				if(flgs&RTF_REINSTATE)
 					flags[ifl++]='R';
 				if(flgs&RTF_DYNAMIC)
 					flags[ifl++]='D';
 				if(flgs&RTF_MODIFIED)
 					flags[ifl++]='M';
 				flags[ifl]=0;
 				dest.s_addr = d;
 				gw.s_addr   = g;
 				mask.s_addr = m;
 				strcpy(sdest,  (numeric == 0 && dest.s_addr==0 ? "default" :
  					inet_ntoa(dest)));
 				strcpy(sgw,    (numeric == 0 && gw.s_addr==0   ? "*"       :
  					inet_ntoa(gw)));
 				printf("%-16s%-16s%-16s%-6s%-6d %-2d %7d %s\n",
 					sdest, sgw,
 					inet_ntoa(mask),
 					flags, metric, ref, use, buff);
			}
 		}
		nl++;
  	}
}

int route_main(int argc, char **argv)
{
	int what = 0;
	int numeric = 0;

	argc--;
	argv++;

	if (argc>0 && strcmp(argv[0], "-n")==0) {
		numeric = 1;
		argc--;
		argv++;
	}

	if (*argv == NULL) {
		displayroutes(numeric);
		return EXIT_SUCCESS;
	} else {
		/* check verb */
		if (strcmp(*argv, "add")==0)
			what = RTACTION_ADD;
		else if (strcmp(*argv, "del")==0 || strcmp(*argv, "delete")==0)
			what = RTACTION_DEL;
		else if (strcmp(*argv, "flush")==0)
			what = RTACTION_FLUSH;
		else
			show_usage();
	}

	return INET_setroute(what, 0, ++argv);
}
