/*
 * stolen from net-tools-1.59 and stripped down for busybox by
 *			Erik Andersen <andersen@codepoet.org>
 *
 * Heavily modified by Manuel Novoa III       Mar 12, 2001
 *
 * Pruned unused code using KEEP_UNUSED define.
 * Added print_bytes_scaled function to reduce code size.
 * Added some (potentially) missing defines.
 * Improved display support for -a and for a named interface.
 *
 * -----------------------------------------------------------
 *
 * ifconfig   This file contains an implementation of the command
 *              that either displays or sets the characteristics of
 *              one or more of the system's networking interfaces.
 *
 * Version:     $Id: interface.c,v 1.21 2004/03/15 08:28:42 andersen Exp $
 *
 * Author:      Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *              and others.  Copyright 1993 MicroWalt Corporation
 *
 *              This program is free software; you can redistribute it
 *              and/or  modify it under  the terms of  the GNU General
 *              Public  License as  published  by  the  Free  Software
 *              Foundation;  either  version 2 of the License, or  (at
 *              your option) any later version.
 *
 * Patched to support 'add' and 'del' keywords for INET(4) addresses
 * by Mrs. Brisby <mrs.brisby@nimh.org>
 *
 * {1.34} - 19980630 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *                     - gettext instead of catgets for i18n
 *          10/1998  - Andi Kleen. Use interface list primitives.
 *	    20001008 - Bernd Eckenfels, Patch from RH for setting mtu
 *			(default AF was wrong)
 */

/* #define KEEP_UNUSED */

/*
 *
 * Protocol Families.
 *
 */
#define HAVE_AFINET 1
#undef HAVE_AFIPX
#undef HAVE_AFATALK
#undef HAVE_AFNETROM
#undef HAVE_AFX25
#undef HAVE_AFECONET
#undef HAVE_AFASH

/*
 *
 * Device Hardware types.
 *
 */
#define HAVE_HWETHER	1
#define HAVE_HWPPP	1
#undef HAVE_HWSLIP


#include "inet_common.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_arp.h>
#include "libbb.h"

#ifdef CONFIG_FEATURE_IPV6
# define HAVE_AFINET6 1
#else
# undef HAVE_AFINET6
#endif

#define _(x) x
#define _PATH_PROCNET_DEV               "/proc/net/dev"
#define _PATH_PROCNET_IFINET6           "/proc/net/if_inet6"
#define new(p) ((p) = xcalloc(1,sizeof(*(p))))
#define KRELEASE(maj,min,patch) ((maj) * 65536 + (min)*256 + (patch))

#ifdef HAVE_HWSLIP
#include <net/if_slip.h>
#endif

#if HAVE_AFINET6

#ifndef _LINUX_IN6_H
/*
 *    This is in linux/include/net/ipv6.h.
 */

struct in6_ifreq {
	struct in6_addr ifr6_addr;
	uint32_t ifr6_prefixlen;
	unsigned int ifr6_ifindex;
};

#endif

#endif							/* HAVE_AFINET6 */

#if HAVE_AFIPX
#if (__GLIBC__ >= 2) && (__GLIBC_MINOR__ >= 1)
#include <netipx/ipx.h>
#else
#include "ipx.h"
#endif
#endif

/* Defines for glibc2.0 users. */
#ifndef SIOCSIFTXQLEN
#define SIOCSIFTXQLEN      0x8943
#define SIOCGIFTXQLEN      0x8942
#endif

/* ifr_qlen is ifru_ivalue, but it isn't present in 2.0 kernel headers */
#ifndef ifr_qlen
#define ifr_qlen        ifr_ifru.ifru_mtu
#endif

#ifndef HAVE_TXQUEUELEN
#define HAVE_TXQUEUELEN 1
#endif

#ifndef IFF_DYNAMIC
#define IFF_DYNAMIC     0x8000	/* dialup device with changing addresses */
#endif

/* This structure defines protocol families and their handlers. */
struct aftype {
	const char *name;
	const char *title;
	int af;
	int alen;
	char *(*print) (unsigned char *);
	char *(*sprint) (struct sockaddr *, int numeric);
	int (*input) (int type, char *bufp, struct sockaddr *);
	void (*herror) (char *text);
	int (*rprint) (int options);
	int (*rinput) (int typ, int ext, char **argv);

	/* may modify src */
	int (*getmask) (char *src, struct sockaddr * mask, char *name);

	int fd;
	char *flag_file;
};

#ifdef KEEP_UNUSED

static int flag_unx;

#ifdef HAVE_AFIPX
static int flag_ipx;
#endif
#ifdef HAVE_AFX25
static int flag_ax25;
#endif
#ifdef HAVE_AFATALK
static int flag_ddp;
#endif
#ifdef HAVE_AFNETROM
static int flag_netrom;
#endif
static int flag_inet;

#ifdef HAVE_AFINET6
static int flag_inet6;
#endif
#ifdef HAVE_AFECONET
static int flag_econet;
#endif
#ifdef HAVE_AFX25
static int flag_x25 = 0;
#endif
#ifdef HAVE_AFASH
static int flag_ash;
#endif


static struct aftrans_t {
	char *alias;
	char *name;
	int *flag;
} aftrans[] = {

#ifdef HAVE_AFX25
	{
	"ax25", "ax25", &flag_ax25},
#endif
	{
	"ip", "inet", &flag_inet},
#ifdef HAVE_AFINET6
	{
	"ip6", "inet6", &flag_inet6},
#endif
#ifdef HAVE_AFIPX
	{
	"ipx", "ipx", &flag_ipx},
#endif
#ifdef HAVE_AFATALK
	{
	"appletalk", "ddp", &flag_ddp},
#endif
#ifdef HAVE_AFNETROM
	{
	"netrom", "netrom", &flag_netrom},
#endif
	{
	"inet", "inet", &flag_inet},
#ifdef HAVE_AFINET6
	{
	"inet6", "inet6", &flag_inet6},
#endif
#ifdef HAVE_AFATALK
	{
	"ddp", "ddp", &flag_ddp},
#endif
	{
	"unix", "unix", &flag_unx}, {
	"tcpip", "inet", &flag_inet},
#ifdef HAVE_AFECONET
	{
	"econet", "ec", &flag_econet},
#endif
#ifdef HAVE_AFX25
	{
	"x25", "x25", &flag_x25},
#endif
#ifdef HAVE_AFASH
	{
	"ash", "ash", &flag_ash},
#endif
	{
	0, 0, 0}
};

static char afname[256] = "";
#endif							/* KEEP_UNUSED */

#if HAVE_AFUNIX

/* Display a UNIX domain address. */
static char *UNIX_print(unsigned char *ptr)
{
	return (ptr);
}


/* Display a UNIX domain address. */
static char *UNIX_sprint(struct sockaddr *sap, int numeric)
{
	static char buf[64];

	if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
		return safe_strncpy(buf, _("[NONE SET]"), sizeof(buf));
	return (UNIX_print(sap->sa_data));
}


static struct aftype unix_aftype = {
	"unix", "UNIX Domain", AF_UNIX, 0,
	UNIX_print, UNIX_sprint, NULL, NULL,
	NULL, NULL, NULL,
	-1,
	"/proc/net/unix"
};
#endif							/* HAVE_AFUNIX */

#if HAVE_AFINET

#ifdef KEEP_UNUSED
static void INET_reserror(char *text)
{
	herror(text);
}

/* Display an Internet socket address. */
static char *INET_print(unsigned char *ptr)
{
	return (inet_ntoa((*(struct in_addr *) ptr)));
}
#endif							/* KEEP_UNUSED */

/* Display an Internet socket address. */
static char *INET_sprint(struct sockaddr *sap, int numeric)
{
	static char buff[128];

	if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
		return safe_strncpy(buff, _("[NONE SET]"), sizeof(buff));

	if (INET_rresolve(buff, sizeof(buff), (struct sockaddr_in *) sap,
					  numeric, 0xffffff00) != 0)
		return (NULL);

	return (buff);
}

#ifdef KEEP_UNUSED
static char *INET_sprintmask(struct sockaddr *sap, int numeric,
							 unsigned int netmask)
{
	static char buff[128];

	if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
		return safe_strncpy(buff, _("[NONE SET]"), sizeof(buff));
	if (INET_rresolve(buff, sizeof(buff), (struct sockaddr_in *) sap,
					  numeric, netmask) != 0)
		return (NULL);
	return (buff);
}

static int INET_getsock(char *bufp, struct sockaddr *sap)
{
	char *sp = bufp, *bp;
	unsigned int i;
	unsigned val;
	struct sockaddr_in *sin;

	sin = (struct sockaddr_in *) sap;
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	val = 0;
	bp = (char *) &val;
	for (i = 0; i < sizeof(sin->sin_addr.s_addr); i++) {
		*sp = toupper(*sp);

		if ((*sp >= 'A') && (*sp <= 'F'))
			bp[i] |= (int) (*sp - 'A') + 10;
		else if ((*sp >= '0') && (*sp <= '9'))
			bp[i] |= (int) (*sp - '0');
		else
			return (-1);

		bp[i] <<= 4;
		sp++;
		*sp = toupper(*sp);

		if ((*sp >= 'A') && (*sp <= 'F'))
			bp[i] |= (int) (*sp - 'A') + 10;
		else if ((*sp >= '0') && (*sp <= '9'))
			bp[i] |= (int) (*sp - '0');
		else
			return (-1);

		sp++;
	}
	sin->sin_addr.s_addr = htonl(val);

	return (sp - bufp);
}

static int INET_input(int type, char *bufp, struct sockaddr *sap)
{
	switch (type) {
	case 1:
		return (INET_getsock(bufp, sap));
	case 256:
		return (INET_resolve(bufp, (struct sockaddr_in *) sap, 1));
	default:
		return (INET_resolve(bufp, (struct sockaddr_in *) sap, 0));
	}
}

static int INET_getnetmask(char *adr, struct sockaddr *m, char *name)
{
	struct sockaddr_in *mask = (struct sockaddr_in *) m;
	char *slash, *end;
	int prefix;

	if ((slash = strchr(adr, '/')) == NULL)
		return 0;

	*slash++ = '\0';
	prefix = strtoul(slash, &end, 0);
	if (*end != '\0')
		return -1;

	if (name) {
		sprintf(name, "/%d", prefix);
	}
	mask->sin_family = AF_INET;
	mask->sin_addr.s_addr = htonl(~(0xffffffffU >> prefix));
	return 1;
}
#endif							/* KEEP_UNUSED */

static struct aftype inet_aftype = {
	"inet", "DARPA Internet", AF_INET, sizeof(unsigned long),
	NULL /* UNUSED INET_print */ , INET_sprint,
	NULL /* UNUSED INET_input */ , NULL /* UNUSED INET_reserror */ ,
	NULL /*INET_rprint */ , NULL /*INET_rinput */ ,
	NULL /* UNUSED INET_getnetmask */ ,
	-1,
	NULL
};

#endif							/* HAVE_AFINET */

#if HAVE_AFINET6

#ifdef KEEP_UNUSED
static void INET6_reserror(char *text)
{
	herror(text);
}

/* Display an Internet socket address. */
static char *INET6_print(unsigned char *ptr)
{
	static char name[80];

	inet_ntop(AF_INET6, (struct in6_addr *) ptr, name, 80);
	return name;
}
#endif							/* KEEP_UNUSED */

/* Display an Internet socket address. */
/* dirty! struct sockaddr usually doesn't suffer for inet6 addresses, fst. */
static char *INET6_sprint(struct sockaddr *sap, int numeric)
{
	static char buff[128];

	if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
		return safe_strncpy(buff, _("[NONE SET]"), sizeof(buff));
	if (INET6_rresolve
		(buff, sizeof(buff), (struct sockaddr_in6 *) sap, numeric) != 0)
		return safe_strncpy(buff, _("[UNKNOWN]"), sizeof(buff));
	return (buff);
}

#ifdef KEEP_UNUSED
static int INET6_getsock(char *bufp, struct sockaddr *sap)
{
	struct sockaddr_in6 *sin6;

	sin6 = (struct sockaddr_in6 *) sap;
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = 0;

	if (inet_pton(AF_INET6, bufp, sin6->sin6_addr.s6_addr) <= 0)
		return (-1);

	return 16;			/* ?;) */
}

static int INET6_input(int type, char *bufp, struct sockaddr *sap)
{
	switch (type) {
	case 1:
		return (INET6_getsock(bufp, sap));
	default:
		return (INET6_resolve(bufp, (struct sockaddr_in6 *) sap));
	}
}
#endif							/* KEEP_UNUSED */

static struct aftype inet6_aftype = {
	"inet6", "IPv6", AF_INET6, sizeof(struct in6_addr),
	NULL /* UNUSED INET6_print */ , INET6_sprint,
	NULL /* UNUSED INET6_input */ , NULL /* UNUSED INET6_reserror */ ,
	NULL /*INET6_rprint */ , NULL /*INET6_rinput */ ,
	NULL /* UNUSED INET6_getnetmask */ ,
	-1,
	NULL
};

#endif							/* HAVE_AFINET6 */

/* Display an UNSPEC address. */
static char *UNSPEC_print(unsigned char *ptr)
{
	static char buff[sizeof(struct sockaddr) * 3 + 1];
	char *pos;
	unsigned int i;

	pos = buff;
	for (i = 0; i < sizeof(struct sockaddr); i++) {
		/* careful -- not every libc's sprintf returns # bytes written */
		sprintf(pos, "%02X-", (*ptr++ & 0377));
		pos += 3;
	}
	/* Erase trailing "-".  Works as long as sizeof(struct sockaddr) != 0 */
	*--pos = '\0';
	return (buff);
}

/* Display an UNSPEC socket address. */
static char *UNSPEC_sprint(struct sockaddr *sap, int numeric)
{
	static char buf[64];

	if (sap->sa_family == 0xFFFF || sap->sa_family == 0)
		return safe_strncpy(buf, _("[NONE SET]"), sizeof(buf));
	return (UNSPEC_print(sap->sa_data));
}

static struct aftype unspec_aftype = {
	"unspec", "UNSPEC", AF_UNSPEC, 0,
	UNSPEC_print, UNSPEC_sprint, NULL, NULL,
	NULL,
};

static struct aftype *aftypes[] = {
#if HAVE_AFUNIX
	&unix_aftype,
#endif
#if HAVE_AFINET
	&inet_aftype,
#endif
#if HAVE_AFINET6
	&inet6_aftype,
#endif
#if HAVE_AFAX25
	&ax25_aftype,
#endif
#if HAVE_AFNETROM
	&netrom_aftype,
#endif
#if HAVE_AFROSE
	&rose_aftype,
#endif
#if HAVE_AFIPX
	&ipx_aftype,
#endif
#if HAVE_AFATALK
	&ddp_aftype,
#endif
#if HAVE_AFECONET
	&ec_aftype,
#endif
#if HAVE_AFASH
	&ash_aftype,
#endif
#if HAVE_AFX25
	&x25_aftype,
#endif
	&unspec_aftype,
	NULL
};

#ifdef KEEP_UNUSED
static short sVafinit = 0;

static void afinit()
{
	unspec_aftype.title = _("UNSPEC");
#if HAVE_AFUNIX
	unix_aftype.title = _("UNIX Domain");
#endif
#if HAVE_AFINET
	inet_aftype.title = _("DARPA Internet");
#endif
#if HAVE_AFINET6
	inet6_aftype.title = _("IPv6");
#endif
#if HAVE_AFAX25
	ax25_aftype.title = _("AMPR AX.25");
#endif
#if HAVE_AFNETROM
	netrom_aftype.title = _("AMPR NET/ROM");
#endif
#if HAVE_AFIPX
	ipx_aftype.title = _("Novell IPX");
#endif
#if HAVE_AFATALK
	ddp_aftype.title = _("Appletalk DDP");
#endif
#if HAVE_AFECONET
	ec_aftype.title = _("Econet");
#endif
#if HAVE_AFX25
	x25_aftype.title = _("CCITT X.25");
#endif
#if HAVE_AFROSE
	rose_aftype.title = _("AMPR ROSE");
#endif
#if HAVE_AFASH
	ash_aftype.title = _("Ash");
#endif
	sVafinit = 1;
}

static int aftrans_opt(const char *arg)
{
	struct aftrans_t *paft;
	char *tmp1, *tmp2;
	char buf[256];

	safe_strncpy(buf, arg, sizeof(buf));

	tmp1 = buf;

	while (tmp1) {

		tmp2 = strchr(tmp1, ',');

		if (tmp2)
			*(tmp2++) = '\0';

		paft = aftrans;
		for (paft = aftrans; paft->alias; paft++) {
			if (strcmp(tmp1, paft->alias))
				continue;
			if (strlen(paft->name) + strlen(afname) + 1 >= sizeof(afname)) {
				bb_error_msg(_("Too many address family arguments."));
				return (0);
			}
			if (paft->flag)
				(*paft->flag)++;
			if (afname[0])
				strcat(afname, ",");
			strcat(afname, paft->name);
			break;
		}
		if (!paft->alias) {
			bb_error_msg(_("Unknown address family `%s'."), tmp1);
			return (1);
		}
		tmp1 = tmp2;
	}

	return (0);
}

/* set the default AF list from the program name or a constant value    */
static void aftrans_def(char *tool, char *argv0, char *dflt)
{
	char *tmp;
	char *buf;

	strcpy(afname, dflt);

	if (!(tmp = strrchr(argv0, '/')))
		tmp = argv0;	/* no slash?! */
	else
		tmp++;

	if (!(buf = strdup(tmp)))
		return;

	if (strlen(tool) >= strlen(tmp)) {
		free(buf);
		return;
	}
	tmp = buf + (strlen(tmp) - strlen(tool));

	if (strcmp(tmp, tool) != 0) {
		free(buf);
		return;
	}
	*tmp = '\0';
	if ((tmp = strchr(buf, '_')))
		*tmp = '\0';

	afname[0] = '\0';
	if (aftrans_opt(buf))
		strcpy(afname, buf);

	free(buf);
}

/* Check our protocol family table for this family. */
static struct aftype *get_aftype(const char *name)
{
	struct aftype **afp;

#ifdef KEEP_UNUSED
	if (!sVafinit)
		afinit();
#endif							/* KEEP_UNUSED */

	afp = aftypes;
	while (*afp != NULL) {
		if (!strcmp((*afp)->name, name))
			return (*afp);
		afp++;
	}
	if (strchr(name, ','))
		bb_error_msg(_("Please don't supply more than one address family."));
	return (NULL);
}
#endif							/* KEEP_UNUSED */

/* Check our protocol family table for this family. */
static struct aftype *get_afntype(int af)
{
	struct aftype **afp;

#ifdef KEEP_UNUSED
	if (!sVafinit)
		afinit();
#endif							/* KEEP_UNUSED */

	afp = aftypes;
	while (*afp != NULL) {
		if ((*afp)->af == af)
			return (*afp);
		afp++;
	}
	return (NULL);
}

/* Check our protocol family table for this family and return its socket */
static int get_socket_for_af(int af)
{
	struct aftype **afp;

#ifdef KEEP_UNUSED
	if (!sVafinit)
		afinit();
#endif							/* KEEP_UNUSED */

	afp = aftypes;
	while (*afp != NULL) {
		if ((*afp)->af == af)
			return (*afp)->fd;
		afp++;
	}
	return -1;
}

#ifdef KEEP_UNUSED
/* type: 0=all, 1=getroute */
static void print_aflist(int type)
{
	int count = 0;
	char *txt;
	struct aftype **afp;

#ifdef KEEP_UNUSED
	if (!sVafinit)
		afinit();
#endif							/* KEEP_UNUSED */

	afp = aftypes;
	while (*afp != NULL) {
		if ((type == 1 && ((*afp)->rprint == NULL)) || ((*afp)->af == 0)) {
			afp++;
			continue;
		}
		if ((count % 3) == 0)
			fprintf(stderr, count ? "\n    " : "    ");
		txt = (*afp)->name;
		if (!txt)
			txt = "..";
		fprintf(stderr, "%s (%s) ", txt, _((*afp)->title));
		count++;
		afp++;
	}
	fprintf(stderr, "\n");
}
#endif							/* KEEP_UNUSED */

struct user_net_device_stats {
	unsigned long long rx_packets;	/* total packets received       */
	unsigned long long tx_packets;	/* total packets transmitted    */
	unsigned long long rx_bytes;	/* total bytes received         */
	unsigned long long tx_bytes;	/* total bytes transmitted      */
	unsigned long rx_errors;	/* bad packets received         */
	unsigned long tx_errors;	/* packet transmit problems     */
	unsigned long rx_dropped;	/* no space in linux buffers    */
	unsigned long tx_dropped;	/* no space available in linux  */
	unsigned long rx_multicast;	/* multicast packets received   */
	unsigned long rx_compressed;
	unsigned long tx_compressed;
	unsigned long collisions;

	/* detailed rx_errors: */
	unsigned long rx_length_errors;
	unsigned long rx_over_errors;	/* receiver ring buff overflow  */
	unsigned long rx_crc_errors;	/* recved pkt with crc error    */
	unsigned long rx_frame_errors;	/* recv'd frame alignment error */
	unsigned long rx_fifo_errors;	/* recv'r fifo overrun          */
	unsigned long rx_missed_errors;	/* receiver missed packet     */
	/* detailed tx_errors */
	unsigned long tx_aborted_errors;
	unsigned long tx_carrier_errors;
	unsigned long tx_fifo_errors;
	unsigned long tx_heartbeat_errors;
	unsigned long tx_window_errors;
};

struct interface {
	struct interface *next, *prev;
	char name[IFNAMSIZ];	/* interface name        */
	short type;			/* if type               */
	short flags;		/* various flags         */
	int metric;			/* routing metric        */
	int mtu;			/* MTU value             */
	int tx_queue_len;	/* transmit queue length */
	struct ifmap map;	/* hardware setup        */
	struct sockaddr addr;	/* IP address            */
	struct sockaddr dstaddr;	/* P-P IP address        */
	struct sockaddr broadaddr;	/* IP broadcast address  */
	struct sockaddr netmask;	/* IP network mask       */
	struct sockaddr ipxaddr_bb;	/* IPX network address   */
	struct sockaddr ipxaddr_sn;	/* IPX network address   */
	struct sockaddr ipxaddr_e3;	/* IPX network address   */
	struct sockaddr ipxaddr_e2;	/* IPX network address   */
	struct sockaddr ddpaddr;	/* Appletalk DDP address */
	struct sockaddr ecaddr;	/* Econet address        */
	int has_ip;
	int has_ipx_bb;
	int has_ipx_sn;
	int has_ipx_e3;
	int has_ipx_e2;
	int has_ax25;
	int has_ddp;
	int has_econet;
	char hwaddr[32];	/* HW address            */
	int statistics_valid;
	struct user_net_device_stats stats;	/* statistics            */
	int keepalive;		/* keepalive value for SLIP */
	int outfill;		/* outfill value for SLIP */
};


int interface_opt_a = 0;	/* show all interfaces          */

#ifdef KEEP_UNUSED
static int opt_i = 0;	/* show the statistics          */
static int opt_v = 0;	/* debugging output flag        */

static int addr_family = 0;	/* currently selected AF        */
#endif							/* KEEP_UNUSED */

static struct interface *int_list, *int_last;
static int skfd = -1;	/* generic raw socket desc.     */


static int sockets_open(int family)
{
	struct aftype **aft;
	int sfd = -1;
	static int force = -1;

	if (force < 0) {
		force = 0;
		if (get_kernel_revision() < KRELEASE(2, 1, 0))
			force = 1;
		if (access("/proc/net", R_OK))
			force = 1;
	}
	for (aft = aftypes; *aft; aft++) {
		struct aftype *af = *aft;
		int type = SOCK_DGRAM;

		if (af->af == AF_UNSPEC)
			continue;
		if (family && family != af->af)
			continue;
		if (af->fd != -1) {
			sfd = af->fd;
			continue;
		}
		/* Check some /proc file first to not stress kmod */
		if (!family && !force && af->flag_file) {
			if (access(af->flag_file, R_OK))
				continue;
		}
#if HAVE_AFNETROM
		if (af->af == AF_NETROM)
			type = SOCK_SEQPACKET;
#endif
#if HAVE_AFX25
		if (af->af == AF_X25)
			type = SOCK_SEQPACKET;
#endif
		af->fd = socket(af->af, type, 0);
		if (af->fd >= 0)
			sfd = af->fd;
	}
	if (sfd < 0) {
		bb_error_msg(_("No usable address families found."));
	}
	return sfd;
}

/* like strcmp(), but knows about numbers */
static int nstrcmp(const char *astr, const char *b)
{
	const char *a = astr;

	while (*a == *b) {
		if (*a == '\0')
			return 0;
		a++;
		b++;
	}
	if (isdigit(*a)) {
		if (!isdigit(*b))
			return -1;
		while (a > astr) {
			a--;
			if (!isdigit(*a)) {
				a++;
				break;
			}
			if (!isdigit(*b))
				return -1;
			b--;
		}
		return atoi(a) > atoi(b) ? 1 : -1;
	}
	return *a - *b;
}

static struct interface *add_interface(char *name)
{
	struct interface *ife, **nextp, *new;

	for (ife = int_last; ife; ife = ife->prev) {
		int n = nstrcmp(ife->name, name);

		if (n == 0)
			return ife;
		if (n < 0)
			break;
	}
	new(new);
	safe_strncpy(new->name, name, IFNAMSIZ);
	nextp = ife ? &ife->next : &int_list;
	new->prev = ife;
	new->next = *nextp;
	if (new->next)
		new->next->prev = new;
	else
		int_last = new;
	*nextp = new;
	return new;
}


static int if_readconf(void)
{
	int numreqs = 30;
	struct ifconf ifc;
	struct ifreq *ifr;
	int n, err = -1;
	int skfd2;

	/* SIOCGIFCONF currently seems to only work properly on AF_INET sockets
	   (as of 2.1.128) */
	skfd2 = get_socket_for_af(AF_INET);
	if (skfd2 < 0) {
		bb_perror_msg(("warning: no inet socket available"));
		/* Try to soldier on with whatever socket we can get hold of.  */
		skfd2 = sockets_open(0);
		if (skfd2 < 0)
			return -1;
	}

	ifc.ifc_buf = NULL;
	for (;;) {
		ifc.ifc_len = sizeof(struct ifreq) * numreqs;
		ifc.ifc_buf = xrealloc(ifc.ifc_buf, ifc.ifc_len);

		if (ioctl(skfd2, SIOCGIFCONF, &ifc) < 0) {
			perror("SIOCGIFCONF");
			goto out;
		}
		if (ifc.ifc_len == sizeof(struct ifreq) * numreqs) {
			/* assume it overflowed and try again */
			numreqs += 10;
			continue;
		}
		break;
	}

	ifr = ifc.ifc_req;
	for (n = 0; n < ifc.ifc_len; n += sizeof(struct ifreq)) {
		add_interface(ifr->ifr_name);
		ifr++;
	}
	err = 0;

  out:
	free(ifc.ifc_buf);
	return err;
}

static char *get_name(char *name, char *p)
{
	while (isspace(*p))
		p++;
	while (*p) {
		if (isspace(*p))
			break;
		if (*p == ':') {	/* could be an alias */
			char *dot = p, *dotname = name;

			*name++ = *p++;
			while (isdigit(*p))
				*name++ = *p++;
			if (*p != ':') {	/* it wasn't, backup */
				p = dot;
				name = dotname;
			}
			if (*p == '\0')
				return NULL;
			p++;
			break;
		}
		*name++ = *p++;
	}
	*name++ = '\0';
	return p;
}

/* If scanf supports size qualifiers for %n conversions, then we can
 * use a modified fmt that simply stores the position in the fields
 * having no associated fields in the proc string.  Of course, we need
 * to zero them again when we're done.  But that is smaller than the
 * old approach of multiple scanf occurrences with large numbers of
 * args. */

/* static const char *ss_fmt[] = { */
/* 	"%Ln%Lu%lu%lu%lu%lu%ln%ln%Ln%Lu%lu%lu%lu%lu%lu", */
/* 	"%Lu%Lu%lu%lu%lu%lu%ln%ln%Lu%Lu%lu%lu%lu%lu%lu", */
/* 	"%Lu%Lu%lu%lu%lu%lu%lu%lu%Lu%Lu%lu%lu%lu%lu%lu%lu" */
/* }; */

	/* Lie about the size of the int pointed to for %n. */
#if INT_MAX == LONG_MAX
static const char *ss_fmt[] = {
	"%n%Lu%u%u%u%u%n%n%n%Lu%u%u%u%u%u",
	"%Lu%Lu%u%u%u%u%n%n%Lu%Lu%u%u%u%u%u",
	"%Lu%Lu%u%u%u%u%u%u%Lu%Lu%u%u%u%u%u%u"
};
#else
static const char *ss_fmt[] = {
	"%n%Lu%lu%lu%lu%lu%n%n%n%Lu%lu%lu%lu%lu%lu",
	"%Lu%Lu%lu%lu%lu%lu%n%n%Lu%Lu%lu%lu%lu%lu%lu",
	"%Lu%Lu%lu%lu%lu%lu%lu%lu%Lu%Lu%lu%lu%lu%lu%lu%lu"
};

#endif

static void get_dev_fields(char *bp, struct interface *ife, int procnetdev_vsn)
{
	memset(&ife->stats, 0, sizeof(struct user_net_device_stats));

	sscanf(bp, ss_fmt[procnetdev_vsn],
		   &ife->stats.rx_bytes, /* missing for 0 */
		   &ife->stats.rx_packets,
		   &ife->stats.rx_errors,
		   &ife->stats.rx_dropped,
		   &ife->stats.rx_fifo_errors,
		   &ife->stats.rx_frame_errors,
		   &ife->stats.rx_compressed, /* missing for <= 1 */
		   &ife->stats.rx_multicast, /* missing for <= 1 */
		   &ife->stats.tx_bytes, /* missing for 0 */
		   &ife->stats.tx_packets,
		   &ife->stats.tx_errors,
		   &ife->stats.tx_dropped,
		   &ife->stats.tx_fifo_errors,
		   &ife->stats.collisions,
		   &ife->stats.tx_carrier_errors,
		   &ife->stats.tx_compressed /* missing for <= 1 */
		   );

	if (procnetdev_vsn <= 1) {
		if (procnetdev_vsn == 0) {
			ife->stats.rx_bytes = 0;
			ife->stats.tx_bytes = 0;
		}
		ife->stats.rx_multicast = 0;
		ife->stats.rx_compressed = 0;
		ife->stats.tx_compressed = 0;
	}
}

static inline int procnetdev_version(char *buf)
{
	if (strstr(buf, "compressed"))
		return 2;
	if (strstr(buf, "bytes"))
		return 1;
	return 0;
}

static int if_readlist_proc(char *target)
{
	static int proc_read;
	FILE *fh;
	char buf[512];
	struct interface *ife;
	int err, procnetdev_vsn;

	if (proc_read)
		return 0;
	if (!target)
		proc_read = 1;

	fh = fopen(_PATH_PROCNET_DEV, "r");
	if (!fh) {
		bb_perror_msg(_("Warning: cannot open %s. Limited output."), _PATH_PROCNET_DEV);
		return if_readconf();
	}
	fgets(buf, sizeof buf, fh);	/* eat line */
	fgets(buf, sizeof buf, fh);

	procnetdev_vsn = procnetdev_version(buf);

	err = 0;
	while (fgets(buf, sizeof buf, fh)) {
		char *s, name[128];

		s = get_name(name, buf);
		ife = add_interface(name);
		get_dev_fields(s, ife, procnetdev_vsn);
		ife->statistics_valid = 1;
		if (target && !strcmp(target, name))
			break;
	}
	if (ferror(fh)) {
		perror(_PATH_PROCNET_DEV);
		err = -1;
		proc_read = 0;
	}
	fclose(fh);
	return err;
}

static int if_readlist(void)
{
	int err = if_readlist_proc(NULL);

	if (!err)
		err = if_readconf();
	return err;
}

static int for_all_interfaces(int (*doit) (struct interface *, void *),
							  void *cookie)
{
	struct interface *ife;

	if (!int_list && (if_readlist() < 0))
		return -1;
	for (ife = int_list; ife; ife = ife->next) {
		int err = doit(ife, cookie);

		if (err)
			return err;
	}
	return 0;
}

/* Support for fetching an IPX address */

#if HAVE_AFIPX
static int ipx_getaddr(int sock, int ft, struct ifreq *ifr)
{
	((struct sockaddr_ipx *) &ifr->ifr_addr)->sipx_type = ft;
	return ioctl(sock, SIOCGIFADDR, ifr);
}
#endif


/* Fetch the interface configuration from the kernel. */
static int if_fetch(struct interface *ife)
{
	struct ifreq ifr;
	int fd;
	char *ifname = ife->name;

	strcpy(ifr.ifr_name, ifname);
	if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
		return (-1);
	ife->flags = ifr.ifr_flags;

	strcpy(ifr.ifr_name, ifname);
	if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0)
		memset(ife->hwaddr, 0, 32);
	else
		memcpy(ife->hwaddr, ifr.ifr_hwaddr.sa_data, 8);

	ife->type = ifr.ifr_hwaddr.sa_family;

	strcpy(ifr.ifr_name, ifname);
	if (ioctl(skfd, SIOCGIFMETRIC, &ifr) < 0)
		ife->metric = 0;
	else
		ife->metric = ifr.ifr_metric;

	strcpy(ifr.ifr_name, ifname);
	if (ioctl(skfd, SIOCGIFMTU, &ifr) < 0)
		ife->mtu = 0;
	else
		ife->mtu = ifr.ifr_mtu;

#ifdef HAVE_HWSLIP
	if (ife->type == ARPHRD_SLIP || ife->type == ARPHRD_CSLIP ||
		ife->type == ARPHRD_SLIP6 || ife->type == ARPHRD_CSLIP6 ||
		ife->type == ARPHRD_ADAPT) {
#ifdef SIOCGOUTFILL
		strcpy(ifr.ifr_name, ifname);
		if (ioctl(skfd, SIOCGOUTFILL, &ifr) < 0)
			ife->outfill = 0;
		else
			ife->outfill = (unsigned int) ifr.ifr_data;
#endif
#ifdef SIOCGKEEPALIVE
		strcpy(ifr.ifr_name, ifname);
		if (ioctl(skfd, SIOCGKEEPALIVE, &ifr) < 0)
			ife->keepalive = 0;
		else
			ife->keepalive = (unsigned int) ifr.ifr_data;
#endif
	}
#endif

	strcpy(ifr.ifr_name, ifname);
	if (ioctl(skfd, SIOCGIFMAP, &ifr) < 0)
		memset(&ife->map, 0, sizeof(struct ifmap));
	else
		memcpy(&ife->map, &ifr.ifr_map, sizeof(struct ifmap));

	strcpy(ifr.ifr_name, ifname);
	if (ioctl(skfd, SIOCGIFMAP, &ifr) < 0)
		memset(&ife->map, 0, sizeof(struct ifmap));
	else
		ife->map = ifr.ifr_map;

#ifdef HAVE_TXQUEUELEN
	strcpy(ifr.ifr_name, ifname);
	if (ioctl(skfd, SIOCGIFTXQLEN, &ifr) < 0)
		ife->tx_queue_len = -1;	/* unknown value */
	else
		ife->tx_queue_len = ifr.ifr_qlen;
#else
	ife->tx_queue_len = -1;	/* unknown value */
#endif

#if HAVE_AFINET
	/* IPv4 address? */
	fd = get_socket_for_af(AF_INET);
	if (fd >= 0) {
		strcpy(ifr.ifr_name, ifname);
		ifr.ifr_addr.sa_family = AF_INET;
		if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
			ife->has_ip = 1;
			ife->addr = ifr.ifr_addr;
			strcpy(ifr.ifr_name, ifname);
			if (ioctl(fd, SIOCGIFDSTADDR, &ifr) < 0)
				memset(&ife->dstaddr, 0, sizeof(struct sockaddr));
			else
				ife->dstaddr = ifr.ifr_dstaddr;

			strcpy(ifr.ifr_name, ifname);
			if (ioctl(fd, SIOCGIFBRDADDR, &ifr) < 0)
				memset(&ife->broadaddr, 0, sizeof(struct sockaddr));
			else
				ife->broadaddr = ifr.ifr_broadaddr;

			strcpy(ifr.ifr_name, ifname);
			if (ioctl(fd, SIOCGIFNETMASK, &ifr) < 0)
				memset(&ife->netmask, 0, sizeof(struct sockaddr));
			else
				ife->netmask = ifr.ifr_netmask;
		} else
			memset(&ife->addr, 0, sizeof(struct sockaddr));
	}
#endif

#if HAVE_AFATALK
	/* DDP address maybe ? */
	fd = get_socket_for_af(AF_APPLETALK);
	if (fd >= 0) {
		strcpy(ifr.ifr_name, ifname);
		if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
			ife->ddpaddr = ifr.ifr_addr;
			ife->has_ddp = 1;
		}
	}
#endif

#if HAVE_AFIPX
	/* Look for IPX addresses with all framing types */
	fd = get_socket_for_af(AF_IPX);
	if (fd >= 0) {
		strcpy(ifr.ifr_name, ifname);
		if (!ipx_getaddr(fd, IPX_FRAME_ETHERII, &ifr)) {
			ife->has_ipx_bb = 1;
			ife->ipxaddr_bb = ifr.ifr_addr;
		}
		strcpy(ifr.ifr_name, ifname);
		if (!ipx_getaddr(fd, IPX_FRAME_SNAP, &ifr)) {
			ife->has_ipx_sn = 1;
			ife->ipxaddr_sn = ifr.ifr_addr;
		}
		strcpy(ifr.ifr_name, ifname);
		if (!ipx_getaddr(fd, IPX_FRAME_8023, &ifr)) {
			ife->has_ipx_e3 = 1;
			ife->ipxaddr_e3 = ifr.ifr_addr;
		}
		strcpy(ifr.ifr_name, ifname);
		if (!ipx_getaddr(fd, IPX_FRAME_8022, &ifr)) {
			ife->has_ipx_e2 = 1;
			ife->ipxaddr_e2 = ifr.ifr_addr;
		}
	}
#endif

#if HAVE_AFECONET
	/* Econet address maybe? */
	fd = get_socket_for_af(AF_ECONET);
	if (fd >= 0) {
		strcpy(ifr.ifr_name, ifname);
		if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
			ife->ecaddr = ifr.ifr_addr;
			ife->has_econet = 1;
		}
	}
#endif

	return 0;
}


static int do_if_fetch(struct interface *ife)
{
	if (if_fetch(ife) < 0) {
		char *errmsg;

		if (errno == ENODEV) {
			/* Give better error message for this case. */
			errmsg = _("Device not found");
		} else {
			errmsg = strerror(errno);
		}
		bb_error_msg(_("%s: error fetching interface information: %s\n"),
				ife->name, errmsg);
		return -1;
	}
	return 0;
}

/* This structure defines hardware protocols and their handlers. */
struct hwtype {
	const char *name;
	const char *title;
	int type;
	int alen;
	char *(*print) (unsigned char *);
	int (*input) (char *, struct sockaddr *);
	int (*activate) (int fd);
	int suppress_null_addr;
};

static struct hwtype unspec_hwtype = {
	"unspec", "UNSPEC", -1, 0,
	UNSPEC_print, NULL, NULL
};

static struct hwtype loop_hwtype = {
	"loop", "Local Loopback", ARPHRD_LOOPBACK, 0,
	NULL, NULL, NULL
};

#if HAVE_HWETHER
#include <net/if_arp.h>

#if __GLIBC__ >=2 && __GLIBC_MINOR >= 1
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif

/* Display an Ethernet address in readable format. */
static char *pr_ether(unsigned char *ptr)
{
	static char buff[64];

	snprintf(buff, sizeof(buff), "%02X:%02X:%02X:%02X:%02X:%02X",
			 (ptr[0] & 0377), (ptr[1] & 0377), (ptr[2] & 0377),
			 (ptr[3] & 0377), (ptr[4] & 0377), (ptr[5] & 0377)
		);
	return (buff);
}

#ifdef KEEP_UNUSED
/* Input an Ethernet address and convert to binary. */
static int in_ether(char *bufp, struct sockaddr *sap)
{
	unsigned char *ptr;
	char c, *orig;
	int i;
	unsigned val;

	sap->sa_family = ether_hwtype.type;
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
			bb_error_msg(_("in_ether(%s): invalid ether address!\n"), orig);
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
			bb_error_msg(_("in_ether(%s): invalid ether address!"), orig);
#endif
			errno = EINVAL;
			return (-1);
		}
		if (c != 0)
			bufp++;
		*ptr++ = (unsigned char) (val & 0377);
		i++;

		/* We might get a semicolon here - not required. */
		if (*bufp == ':') {
#ifdef DEBUG
			if (i == ETH_ALEN) {
				bb_error_msg(_("in_ether(%s): trailing : ignored!"), orig);
			}
#endif
			bufp++;
		}
	}

#ifdef DEBUG
	/* That's it.  Any trailing junk? */
	if ((i == ETH_ALEN) && (*bufp != '\0')) {
		bb_error_msg(_("in_ether(%s): trailing junk!"), orig);
		errno = EINVAL;
		return (-1);
	}
	bb_error_msg("in_ether(%s): %s", orig, pr_ether(sap->sa_data));
#endif

	return (0);
}
#endif							/* KEEP_UNUSED */


static struct hwtype ether_hwtype = {
	"ether", "Ethernet", ARPHRD_ETHER, ETH_ALEN,
	pr_ether, NULL /* UNUSED in_ether */ , NULL
};


#endif							/* HAVE_HWETHER */


#if HAVE_HWPPP

#include <net/if_arp.h>

#ifdef KEEP_UNUSED
/* Start the PPP encapsulation on the file descriptor. */
static int do_ppp(int fd)
{
	bb_error_msg(_("You cannot start PPP with this program."));
	return -1;
}
#endif							/* KEEP_UNUSED */

static struct hwtype ppp_hwtype = {
	"ppp", "Point-Point Protocol", ARPHRD_PPP, 0,
	NULL, NULL, NULL /* UNUSED do_ppp */ , 0
};


#endif							/* HAVE_PPP */

static struct hwtype *hwtypes[] = {

	&loop_hwtype,

#if HAVE_HWSLIP
	&slip_hwtype,
	&cslip_hwtype,
	&slip6_hwtype,
	&cslip6_hwtype,
	&adaptive_hwtype,
#endif
#if HAVE_HWSTRIP
	&strip_hwtype,
#endif
#if HAVE_HWASH
	&ash_hwtype,
#endif
#if HAVE_HWETHER
	&ether_hwtype,
#endif
#if HAVE_HWTR
	&tr_hwtype,
#ifdef ARPHRD_IEEE802_TR
	&tr_hwtype1,
#endif
#endif
#if HAVE_HWAX25
	&ax25_hwtype,
#endif
#if HAVE_HWNETROM
	&netrom_hwtype,
#endif
#if HAVE_HWROSE
	&rose_hwtype,
#endif
#if HAVE_HWTUNNEL
	&tunnel_hwtype,
#endif
#if HAVE_HWPPP
	&ppp_hwtype,
#endif
#if HAVE_HWHDLCLAPB
	&hdlc_hwtype,
	&lapb_hwtype,
#endif
#if HAVE_HWARC
	&arcnet_hwtype,
#endif
#if HAVE_HWFR
	&dlci_hwtype,
	&frad_hwtype,
#endif
#if HAVE_HWSIT
	&sit_hwtype,
#endif
#if HAVE_HWFDDI
	&fddi_hwtype,
#endif
#if HAVE_HWHIPPI
	&hippi_hwtype,
#endif
#if HAVE_HWIRDA
	&irda_hwtype,
#endif
#if HAVE_HWEC
	&ec_hwtype,
#endif
#if HAVE_HWX25
	&x25_hwtype,
#endif
	&unspec_hwtype,
	NULL
};

#ifdef KEEP_UNUSED
static short sVhwinit = 0;

static void hwinit()
{
	loop_hwtype.title = _("Local Loopback");
	unspec_hwtype.title = _("UNSPEC");
#if HAVE_HWSLIP
	slip_hwtype.title = _("Serial Line IP");
	cslip_hwtype.title = _("VJ Serial Line IP");
	slip6_hwtype.title = _("6-bit Serial Line IP");
	cslip6_hwtype.title = _("VJ 6-bit Serial Line IP");
	adaptive_hwtype.title = _("Adaptive Serial Line IP");
#endif
#if HAVE_HWETHER
	ether_hwtype.title = _("Ethernet");
#endif
#if HAVE_HWASH
	ash_hwtype.title = _("Ash");
#endif
#if HAVE_HWFDDI
	fddi_hwtype.title = _("Fiber Distributed Data Interface");
#endif
#if HAVE_HWHIPPI
	hippi_hwtype.title = _("HIPPI");
#endif
#if HAVE_HWAX25
	ax25_hwtype.title = _("AMPR AX.25");
#endif
#if HAVE_HWROSE
	rose_hwtype.title = _("AMPR ROSE");
#endif
#if HAVE_HWNETROM
	netrom_hwtype.title = _("AMPR NET/ROM");
#endif
#if HAVE_HWX25
	x25_hwtype.title = _("generic X.25");
#endif
#if HAVE_HWTUNNEL
	tunnel_hwtype.title = _("IPIP Tunnel");
#endif
#if HAVE_HWPPP
	ppp_hwtype.title = _("Point-to-Point Protocol");
#endif
#if HAVE_HWHDLCLAPB
	hdlc_hwtype.title = _("(Cisco)-HDLC");
	lapb_hwtype.title = _("LAPB");
#endif
#if HAVE_HWARC
	arcnet_hwtype.title = _("ARCnet");
#endif
#if HAVE_HWFR
	dlci_hwtype.title = _("Frame Relay DLCI");
	frad_hwtype.title = _("Frame Relay Access Device");
#endif
#if HAVE_HWSIT
	sit_hwtype.title = _("IPv6-in-IPv4");
#endif
#if HAVE_HWIRDA
	irda_hwtype.title = _("IrLAP");
#endif
#if HAVE_HWTR
	tr_hwtype.title = _("16/4 Mbps Token Ring");
#ifdef ARPHRD_IEEE802_TR
	tr_hwtype1.title = _("16/4 Mbps Token Ring (New)");
#endif
#endif
#if HAVE_HWEC
	ec_hwtype.title = _("Econet");
#endif
	sVhwinit = 1;
}
#endif							/* KEEP_UNUSED */

#ifdef IFF_PORTSEL
#if 0
static const char * const if_port_text[][4] = {
	/* Keep in step with <linux/netdevice.h> */
	{"unknown", NULL, NULL, NULL},
	{"10base2", "bnc", "coax", NULL},
	{"10baseT", "utp", "tpe", NULL},
	{"AUI", "thick", "db15", NULL},
	{"100baseT", NULL, NULL, NULL},
	{"100baseTX", NULL, NULL, NULL},
	{"100baseFX", NULL, NULL, NULL},
	{NULL, NULL, NULL, NULL},
};
#else
static const char * const if_port_text[] = {
	/* Keep in step with <linux/netdevice.h> */
	"unknown",
	"10base2",
	"10baseT",
	"AUI",
	"100baseT",
	"100baseTX",
	"100baseFX",
	NULL
};
#endif
#endif

/* Check our hardware type table for this type. */
static struct hwtype *get_hwntype(int type)
{
	struct hwtype **hwp;

#ifdef KEEP_UNUSED
	if (!sVhwinit)
		hwinit();
#endif							/* KEEP_UNUSED */

	hwp = hwtypes;
	while (*hwp != NULL) {
		if ((*hwp)->type == type)
			return (*hwp);
		hwp++;
	}
	return (NULL);
}

/* return 1 if address is all zeros */
static int hw_null_address(struct hwtype *hw, void *ap)
{
	unsigned int i;
	unsigned char *address = (unsigned char *) ap;

	for (i = 0; i < hw->alen; i++)
		if (address[i])
			return 0;
	return 1;
}

#warning devel code
static const char TRext[] = "\0\0\0Ki\0Mi\0Gi\0Ti";

static void print_bytes_scaled(unsigned long long ull, const char *end)
{
	unsigned long long int_part;
	const char *ext;
	unsigned int frac_part;
	int i;

	frac_part = 0;
	ext = TRext;
	int_part = ull;
	i = 4;
	do {
#if 0
		/* This does correct rounding and is a little larger.  But it
		 * uses KiB as the smallest displayed unit. */
		if ((int_part < (1024*1024 - 51)) || !--i) {
			i = 0;
			int_part += 51;		/* 1024*.05 = 51.2 */
			frac_part = ((((unsigned int) int_part) & (1024-1)) * 10) / 1024;
		}
		int_part /= 1024;
		ext += 3;	/* KiB, MiB, GiB, TiB */
#else
		if (int_part >= 1024) {
			frac_part = ((((unsigned int) int_part) & (1024-1)) * 10) / 1024;
			int_part /= 1024;
			ext += 3;	/* KiB, MiB, GiB, TiB */
		}
		--i;
#endif
	} while (i);

	printf("X bytes:%Lu (%Lu.%u %sB)%s", ull, int_part, frac_part, ext, end);
}

static const char * const ife_print_flags_strs[] = {
	"UP ",
	"BROADCAST ",
	"DEBUG ",
	"LOOPBACK ",
	"POINTOPOINT ",
	"NOTRAILERS ",
	"RUNNING ",
	"NOARP ",
	"PROMISC ",
	"ALLMULTI ",
	"SLAVE ",
	"MASTER ",
	"MULTICAST ",
#ifdef HAVE_DYNAMIC
	"DYNAMIC "
#endif
};

static const unsigned short ife_print_flags_mask[] = {
	IFF_UP,
	IFF_BROADCAST,
	IFF_DEBUG,
	IFF_LOOPBACK,
	IFF_POINTOPOINT,
	IFF_NOTRAILERS,
	IFF_RUNNING,
	IFF_NOARP,
	IFF_PROMISC,
	IFF_ALLMULTI,
	IFF_SLAVE,
	IFF_MASTER,
	IFF_MULTICAST,
#ifdef HAVE_DYNAMIC
	IFF_DYNAMIC
#endif
	0
};

static void ife_print(struct interface *ptr)
{
	struct aftype *ap;
	struct hwtype *hw;
	int hf;
	int can_compress = 0;

#if HAVE_AFIPX
	static struct aftype *ipxtype = NULL;
#endif
#if HAVE_AFECONET
	static struct aftype *ectype = NULL;
#endif
#if HAVE_AFATALK
	static struct aftype *ddptype = NULL;
#endif
#if HAVE_AFINET6
	FILE *f;
	char addr6[40], devname[20];
	struct sockaddr_in6 sap;
	int plen, scope, dad_status, if_idx;
	char addr6p[8][5];
#endif

	ap = get_afntype(ptr->addr.sa_family);
	if (ap == NULL)
		ap = get_afntype(0);

	hf = ptr->type;

	if (hf == ARPHRD_CSLIP || hf == ARPHRD_CSLIP6)
		can_compress = 1;

	hw = get_hwntype(hf);
	if (hw == NULL)
		hw = get_hwntype(-1);

	printf(_("%-9.9s Link encap:%s  "), ptr->name, _(hw->title));
	/* For some hardware types (eg Ash, ATM) we don't print the
	   hardware address if it's null.  */
	if (hw->print != NULL && (!(hw_null_address(hw, ptr->hwaddr) &&
								hw->suppress_null_addr)))
		printf(_("HWaddr %s  "), hw->print(ptr->hwaddr));
#ifdef IFF_PORTSEL
	if (ptr->flags & IFF_PORTSEL) {
		printf(_("Media:%s"), if_port_text[ptr->map.port] /* [0] */);
		if (ptr->flags & IFF_AUTOMEDIA)
			printf(_("(auto)"));
	}
#endif
	printf("\n");

#if HAVE_AFINET
	if (ptr->has_ip) {
		printf(_("          %s addr:%s "), ap->name,
			   ap->sprint(&ptr->addr, 1));
		if (ptr->flags & IFF_POINTOPOINT) {
			printf(_(" P-t-P:%s "), ap->sprint(&ptr->dstaddr, 1));
		}
		if (ptr->flags & IFF_BROADCAST) {
			printf(_(" Bcast:%s "), ap->sprint(&ptr->broadaddr, 1));
		}
		printf(_(" Mask:%s\n"), ap->sprint(&ptr->netmask, 1));
	}
#endif

#if HAVE_AFINET6

#define IPV6_ADDR_ANY           0x0000U

#define IPV6_ADDR_UNICAST       0x0001U
#define IPV6_ADDR_MULTICAST     0x0002U
#define IPV6_ADDR_ANYCAST       0x0004U

#define IPV6_ADDR_LOOPBACK      0x0010U
#define IPV6_ADDR_LINKLOCAL     0x0020U
#define IPV6_ADDR_SITELOCAL     0x0040U

#define IPV6_ADDR_COMPATv4      0x0080U

#define IPV6_ADDR_SCOPE_MASK    0x00f0U

#define IPV6_ADDR_MAPPED        0x1000U
#define IPV6_ADDR_RESERVED      0x2000U	/* reserved address space */

	if ((f = fopen(_PATH_PROCNET_IFINET6, "r")) != NULL) {
		while (fscanf
			   (f, "%4s%4s%4s%4s%4s%4s%4s%4s %02x %02x %02x %02x %20s\n",
				addr6p[0], addr6p[1], addr6p[2], addr6p[3], addr6p[4],
				addr6p[5], addr6p[6], addr6p[7], &if_idx, &plen, &scope,
				&dad_status, devname) != EOF) {
			if (!strcmp(devname, ptr->name)) {
				sprintf(addr6, "%s:%s:%s:%s:%s:%s:%s:%s",
						addr6p[0], addr6p[1], addr6p[2], addr6p[3],
						addr6p[4], addr6p[5], addr6p[6], addr6p[7]);
				inet_pton(AF_INET6, addr6,
						  (struct sockaddr *) &sap.sin6_addr);
				sap.sin6_family = AF_INET6;
				printf(_("          inet6 addr: %s/%d"),
					   inet6_aftype.sprint((struct sockaddr *) &sap, 1),
					   plen);
				printf(_(" Scope:"));
				switch (scope & IPV6_ADDR_SCOPE_MASK) {
				case 0:
					printf(_("Global"));
					break;
				case IPV6_ADDR_LINKLOCAL:
					printf(_("Link"));
					break;
				case IPV6_ADDR_SITELOCAL:
					printf(_("Site"));
					break;
				case IPV6_ADDR_COMPATv4:
					printf(_("Compat"));
					break;
				case IPV6_ADDR_LOOPBACK:
					printf(_("Host"));
					break;
				default:
					printf(_("Unknown"));
				}
				printf("\n");
			}
		}
		fclose(f);
	}
#endif

#if HAVE_AFIPX
	if (ipxtype == NULL)
		ipxtype = get_afntype(AF_IPX);

	if (ipxtype != NULL) {
		if (ptr->has_ipx_bb)
			printf(_("          IPX/Ethernet II addr:%s\n"),
				   ipxtype->sprint(&ptr->ipxaddr_bb, 1));
		if (ptr->has_ipx_sn)
			printf(_("          IPX/Ethernet SNAP addr:%s\n"),
				   ipxtype->sprint(&ptr->ipxaddr_sn, 1));
		if (ptr->has_ipx_e2)
			printf(_("          IPX/Ethernet 802.2 addr:%s\n"),
				   ipxtype->sprint(&ptr->ipxaddr_e2, 1));
		if (ptr->has_ipx_e3)
			printf(_("          IPX/Ethernet 802.3 addr:%s\n"),
				   ipxtype->sprint(&ptr->ipxaddr_e3, 1));
	}
#endif

#if HAVE_AFATALK
	if (ddptype == NULL)
		ddptype = get_afntype(AF_APPLETALK);
	if (ddptype != NULL) {
		if (ptr->has_ddp)
			printf(_("          EtherTalk Phase 2 addr:%s\n"),
				   ddptype->sprint(&ptr->ddpaddr, 1));
	}
#endif

#if HAVE_AFECONET
	if (ectype == NULL)
		ectype = get_afntype(AF_ECONET);
	if (ectype != NULL) {
		if (ptr->has_econet)
			printf(_("          econet addr:%s\n"),
				   ectype->sprint(&ptr->ecaddr, 1));
	}
#endif

	printf("          ");
	/* DONT FORGET TO ADD THE FLAGS IN ife_print_short, too */

	if (ptr->flags == 0) {
		printf(_("[NO FLAGS] "));
	} else {
		int i = 0;
		do {
			if (ptr->flags & ife_print_flags_mask[i]) {
				printf(_(ife_print_flags_strs[i]));
			}
		} while (ife_print_flags_mask[++i]);
	}

	/* DONT FORGET TO ADD THE FLAGS IN ife_print_short */
	printf(_(" MTU:%d  Metric:%d"), ptr->mtu, ptr->metric ? ptr->metric : 1);
#ifdef SIOCSKEEPALIVE
	if (ptr->outfill || ptr->keepalive)
		printf(_("  Outfill:%d  Keepalive:%d"), ptr->outfill, ptr->keepalive);
#endif
	printf("\n");

	/* If needed, display the interface statistics. */

	if (ptr->statistics_valid) {
		/* XXX: statistics are currently only printed for the primary address,
		 *      not for the aliases, although strictly speaking they're shared
		 *      by all addresses.
		 */
		printf("          ");

		printf(_("RX packets:%Lu errors:%lu dropped:%lu overruns:%lu frame:%lu\n"),
			   ptr->stats.rx_packets, ptr->stats.rx_errors,
			   ptr->stats.rx_dropped, ptr->stats.rx_fifo_errors,
			   ptr->stats.rx_frame_errors);
		if (can_compress)
			printf(_("             compressed:%lu\n"),
				   ptr->stats.rx_compressed);
		printf("          ");
		printf(_("TX packets:%Lu errors:%lu dropped:%lu overruns:%lu carrier:%lu\n"),
			   ptr->stats.tx_packets, ptr->stats.tx_errors,
			   ptr->stats.tx_dropped, ptr->stats.tx_fifo_errors,
			   ptr->stats.tx_carrier_errors);
		printf(_("          collisions:%lu "), ptr->stats.collisions);
		if (can_compress)
			printf(_("compressed:%lu "), ptr->stats.tx_compressed);
		if (ptr->tx_queue_len != -1)
			printf(_("txqueuelen:%d "), ptr->tx_queue_len);
		printf("\n          R");
		print_bytes_scaled(ptr->stats.rx_bytes, "  T");
		print_bytes_scaled(ptr->stats.tx_bytes, "\n");

	}

	if ((ptr->map.irq || ptr->map.mem_start || ptr->map.dma ||
		 ptr->map.base_addr)) {
		printf("          ");
		if (ptr->map.irq)
			printf(_("Interrupt:%d "), ptr->map.irq);
		if (ptr->map.base_addr >= 0x100)	/* Only print devices using it for
											   I/O maps */
			printf(_("Base address:0x%lx "),
				   (unsigned long) ptr->map.base_addr);
		if (ptr->map.mem_start) {
			printf(_("Memory:%lx-%lx "), ptr->map.mem_start,
				   ptr->map.mem_end);
		}
		if (ptr->map.dma)
			printf(_("DMA chan:%x "), ptr->map.dma);
		printf("\n");
	}
	printf("\n");
}


static int do_if_print(struct interface *ife, void *cookie)
{
	int *opt_a = (int *) cookie;
	int res;

	res = do_if_fetch(ife);
	if (res >= 0) {
		if ((ife->flags & IFF_UP) || *opt_a)
			ife_print(ife);
	}
	return res;
}

static struct interface *lookup_interface(char *name)
{
	struct interface *ife = NULL;

	if (if_readlist_proc(name) < 0)
		return NULL;
	ife = add_interface(name);
	return ife;
}

/* for ipv4 add/del modes */
static int if_print(char *ifname)
{
	int res;

	if (!ifname) {
		res = for_all_interfaces(do_if_print, &interface_opt_a);
	} else {
		struct interface *ife;

		ife = lookup_interface(ifname);
		res = do_if_fetch(ife);
		if (res >= 0)
			ife_print(ife);
	}
	return res;
}

int display_interfaces(char *ifname)
{
	int status;

	/* Create a channel to the NET kernel. */
	if ((skfd = sockets_open(0)) < 0) {
		bb_perror_msg_and_die("socket");
	}

	/* Do we have to show the current setup? */
	status = if_print(ifname);
	close(skfd);
	exit(status < 0);
}
