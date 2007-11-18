/* vi: set sw=4 ts=4: */
/*
 * iplink.c "ip link".
 *
 * Authors: Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

//#include <sys/ioctl.h>
//#include <sys/socket.h>
#include <net/if.h>
#include <net/if_packet.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

#include "ip_common.h"  /* #include "libbb.h" is inside */
#include "rt_names.h"
#include "utils.h"

/* taken from linux/sockios.h */
#define SIOCSIFNAME	0x8923		/* set interface name */

/* Exits on error */
static int get_ctl_fd(void)
{
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	fd = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	return xsocket(PF_INET6, SOCK_DGRAM, 0);
}

/* Exits on error */
static void do_chflags(char *dev, uint32_t flags, uint32_t mask)
{
	struct ifreq ifr;
	int fd;

	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	fd = get_ctl_fd();
	xioctl(fd, SIOCGIFFLAGS, &ifr);
	if ((ifr.ifr_flags ^ flags) & mask) {
		ifr.ifr_flags &= ~mask;
		ifr.ifr_flags |= mask & flags;
		xioctl(fd, SIOCSIFFLAGS, &ifr);
	}
	close(fd);
}

/* Exits on error */
static void do_changename(char *dev, char *newdev)
{
	struct ifreq ifr;
	int fd;

	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	strncpy(ifr.ifr_newname, newdev, sizeof(ifr.ifr_newname));
	fd = get_ctl_fd();
	xioctl(fd, SIOCSIFNAME, &ifr);
	close(fd);
}

/* Exits on error */
static void set_qlen(char *dev, int qlen)
{
	struct ifreq ifr;
	int s;

	s = get_ctl_fd();
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	ifr.ifr_qlen = qlen;
	xioctl(s, SIOCSIFTXQLEN, &ifr);
	close(s);
}

/* Exits on error */
static void set_mtu(char *dev, int mtu)
{
	struct ifreq ifr;
	int s;

	s = get_ctl_fd();
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = mtu;
	xioctl(s, SIOCSIFMTU, &ifr);
	close(s);
}

/* Exits on error */
static int get_address(char *dev, int *htype)
{
	struct ifreq ifr;
	struct sockaddr_ll me;
	socklen_t alen;
	int s;

	s = xsocket(PF_PACKET, SOCK_DGRAM, 0);

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	xioctl(s, SIOCGIFINDEX, &ifr);

	memset(&me, 0, sizeof(me));
	me.sll_family = AF_PACKET;
	me.sll_ifindex = ifr.ifr_ifindex;
	me.sll_protocol = htons(ETH_P_LOOP);
	xbind(s, (struct sockaddr*)&me, sizeof(me));

	alen = sizeof(me);
	if (getsockname(s, (struct sockaddr*)&me, &alen) == -1) {
		bb_perror_msg_and_die("getsockname");
	}
	close(s);
	*htype = me.sll_hatype;
	return me.sll_halen;
}

/* Exits on error */
static void parse_address(char *dev, int hatype, int halen, char *lla, struct ifreq *ifr)
{
	int alen;

	memset(ifr, 0, sizeof(*ifr));
	strncpy(ifr->ifr_name, dev, sizeof(ifr->ifr_name));
	ifr->ifr_hwaddr.sa_family = hatype;
	alen = ll_addr_a2n((unsigned char *)(ifr->ifr_hwaddr.sa_data), 14, lla);
	if (alen < 0)
		exit(1);
	if (alen != halen) {
		bb_error_msg_and_die("wrong address (%s) length: expected %d bytes", lla, halen);
	}
}

/* Exits on error */
static void set_address(struct ifreq *ifr, int brd)
{
	int s;

	s = get_ctl_fd();
	if (brd)
		xioctl(s, SIOCSIFHWBROADCAST, ifr);
	else
		xioctl(s, SIOCSIFHWADDR, ifr);
	close(s);
}


static void die_must_be_on_off(const char *msg) ATTRIBUTE_NORETURN;
static void die_must_be_on_off(const char *msg)
{
	bb_error_msg_and_die("argument of \"%s\" must be \"on\" or \"off\"", msg);
}

/* Return value becomes exitcode. It's okay to not return at all */
static int do_set(char **argv)
{
	char *dev = NULL;
	uint32_t mask = 0;
	uint32_t flags = 0;
	int qlen = -1;
	int mtu = -1;
	char *newaddr = NULL;
	char *newbrd = NULL;
	struct ifreq ifr0, ifr1;
	char *newname = NULL;
	int htype, halen;
	static const char keywords[] ALIGN1 =
		"up\0""down\0""name\0""mtu\0""multicast\0""arp\0""addr\0""dev\0";
	enum { ARG_up = 0, ARG_down, ARG_name, ARG_mtu, ARG_multicast, ARG_arp,
		ARG_addr, ARG_dev };
	static const char str_on_off[] ALIGN1 = "on\0""off\0";
	enum { PARM_on = 0, PARM_off };
	smalluint key;

	while (*argv) {
		key = index_in_strings(keywords, *argv);
		if (key == ARG_up) {
			mask |= IFF_UP;
			flags |= IFF_UP;
		}
		if (key == ARG_down) {
			mask |= IFF_UP;
			flags &= ~IFF_UP;
		}
		if (key == ARG_name) {
			NEXT_ARG();
			newname = *argv;
		}
		if (key == ARG_mtu) {
			NEXT_ARG();
			if (mtu != -1)
				duparg("mtu", *argv);
			if (get_integer(&mtu, *argv, 0))
				invarg(*argv, "mtu");
		}
		if (key == ARG_multicast) {
			int param;
			NEXT_ARG();
			mask |= IFF_MULTICAST;
			param = index_in_strings(str_on_off, *argv);
			if (param < 0)
				die_must_be_on_off("multicast");
			if (param == PARM_on)
				flags |= IFF_MULTICAST;
			else
				flags &= ~IFF_MULTICAST;
		}
		if (key == ARG_arp) {
			int param;
			NEXT_ARG();
			mask |= IFF_NOARP;
			param = index_in_strings(str_on_off, *argv);
			if (param < 0)
				die_must_be_on_off("arp");
			if (param == PARM_on)
				flags &= ~IFF_NOARP;
			else
				flags |= IFF_NOARP;
		}
		if (key == ARG_addr) {
			NEXT_ARG();
			newaddr = *argv;
		}
		if (key >= ARG_dev) {
			if (key == ARG_dev) {
				NEXT_ARG();
			}
			if (dev)
				duparg2("dev", *argv);
			dev = *argv;
		}
		argv++;
	}

	if (!dev) {
		bb_error_msg_and_die(bb_msg_requires_arg, "\"dev\"");
	}

	if (newaddr || newbrd) {
		halen = get_address(dev, &htype);
		if (newaddr) {
			parse_address(dev, htype, halen, newaddr, &ifr0);
		}
		if (newbrd) {
			parse_address(dev, htype, halen, newbrd, &ifr1);
		}
	}

	if (newname && strcmp(dev, newname)) {
		do_changename(dev, newname);
		dev = newname;
	}
	if (qlen != -1) {
		set_qlen(dev, qlen);
	}
	if (mtu != -1) {
		set_mtu(dev, mtu);
	}
	if (newaddr || newbrd) {
		if (newbrd) {
			set_address(&ifr1, 1);
		}
		if (newaddr) {
			set_address(&ifr0, 0);
		}
	}
	if (mask)
		do_chflags(dev, flags, mask);
	return 0;
}

static int ipaddr_list_link(char **argv)
{
	preferred_family = AF_PACKET;
	return ipaddr_list_or_flush(argv, 0);
}

/* Return value becomes exitcode. It's okay to not return at all */
int do_iplink(char **argv)
{
	static const char keywords[] ALIGN1 =
		"set\0""show\0""lst\0""list\0";
	int key;
	if (!*argv)
		return ipaddr_list_link(argv);
	key = index_in_substrings(keywords, *argv);
	if (key < 0)
		bb_error_msg_and_die(bb_msg_invalid_arg, *argv, applet_name);
	argv++;
	if (key == 0) /* set */
		return do_set(argv);
	/* show, lst, list */
	return ipaddr_list_link(argv);
}
