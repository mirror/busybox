/* vi: set sw=4 ts=4: */
/*
 * iplink.c "ip link".
 *
 * Authors: Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_packet.h>
#include <netpacket/packet.h>

#include <net/ethernet.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

/* take from linux/sockios.h */
#define SIOCSIFNAME	0x8923		/* set interface name */

static int on_off(char *msg)
{
	bb_error_msg("error: argument of \"%s\" must be \"on\" or \"off\"", msg);
	return -1;
}

static int get_ctl_fd(void)
{
	int s_errno;
	int fd;

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	s_errno = errno;
	fd = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	fd = socket(PF_INET6, SOCK_DGRAM, 0);
	if (fd >= 0)
		return fd;
	errno = s_errno;
	bb_perror_msg("cannot create control socket");
	return -1;
}

static int do_chflags(char *dev, uint32_t flags, uint32_t mask)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	fd = get_ctl_fd();
	if (fd < 0)
		return -1;
	err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (err) {
		bb_perror_msg("SIOCGIFFLAGS");
		close(fd);
		return -1;
	}
	if ((ifr.ifr_flags^flags)&mask) {
		ifr.ifr_flags &= ~mask;
		ifr.ifr_flags |= mask&flags;
		err = ioctl(fd, SIOCSIFFLAGS, &ifr);
		if (err)
			bb_perror_msg("SIOCSIFFLAGS");
	}
	close(fd);
	return err;
}

static int do_changename(char *dev, char *newdev)
{
	struct ifreq ifr;
	int fd;
	int err;

	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	strncpy(ifr.ifr_newname, newdev, sizeof(ifr.ifr_newname));
	fd = get_ctl_fd();
	if (fd < 0)
		return -1;
	err = ioctl(fd, SIOCSIFNAME, &ifr);
	if (err) {
		bb_perror_msg("SIOCSIFNAME");
		close(fd);
		return -1;
	}
	close(fd);
	return err;
}

static int set_qlen(char *dev, int qlen)
{
	struct ifreq ifr;
	int s;

	s = get_ctl_fd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	ifr.ifr_qlen = qlen;
	if (ioctl(s, SIOCSIFTXQLEN, &ifr) < 0) {
		bb_perror_msg("SIOCSIFXQLEN");
		close(s);
		return -1;
	}
	close(s);

	return 0;
}

static int set_mtu(char *dev, int mtu)
{
	struct ifreq ifr;
	int s;

	s = get_ctl_fd();
	if (s < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = mtu;
	if (ioctl(s, SIOCSIFMTU, &ifr) < 0) {
		bb_perror_msg("SIOCSIFMTU");
		close(s);
		return -1;
	}
	close(s);

	return 0;
}

static int get_address(char *dev, int *htype)
{
	struct ifreq ifr;
	struct sockaddr_ll me;
	socklen_t alen;
	int s;

	s = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (s < 0) {
		bb_perror_msg("socket(PF_PACKET)");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		bb_perror_msg("SIOCGIFINDEX");
		close(s);
		return -1;
	}

	memset(&me, 0, sizeof(me));
	me.sll_family = AF_PACKET;
	me.sll_ifindex = ifr.ifr_ifindex;
	me.sll_protocol = htons(ETH_P_LOOP);
	if (bind(s, (struct sockaddr*)&me, sizeof(me)) == -1) {
		bb_perror_msg("bind");
		close(s);
		return -1;
	}

	alen = sizeof(me);
	if (getsockname(s, (struct sockaddr*)&me, &alen) == -1) {
		bb_perror_msg("getsockname");
		close(s);
		return -1;
	}
	close(s);
	*htype = me.sll_hatype;
	return me.sll_halen;
}

static int parse_address(char *dev, int hatype, int halen, char *lla, struct ifreq *ifr)
{
	int alen;

	memset(ifr, 0, sizeof(*ifr));
	strncpy(ifr->ifr_name, dev, sizeof(ifr->ifr_name));
	ifr->ifr_hwaddr.sa_family = hatype;
	alen = ll_addr_a2n((unsigned char *)(ifr->ifr_hwaddr.sa_data), 14, lla);
	if (alen < 0)
		return -1;
	if (alen != halen) {
		bb_error_msg("wrong address (%s) length: expected %d bytes", lla, halen);
		return -1;
	}
	return 0;
}

static int set_address(struct ifreq *ifr, int brd)
{
	int s;

	s = get_ctl_fd();
	if (s < 0)
		return -1;
	if (ioctl(s, brd?SIOCSIFHWBROADCAST:SIOCSIFHWADDR, ifr) < 0) {
		bb_perror_msg(brd ? "SIOCSIFHWBROADCAST" : "SIOCSIFHWADDR");
		close(s);
		return -1;
	}
	close(s);
	return 0;
}


static int do_set(int argc, char **argv)
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

	while (argc > 0) {
		if (strcmp(*argv, "up") == 0) {
			mask |= IFF_UP;
			flags |= IFF_UP;
		} else if (strcmp(*argv, "down") == 0) {
			mask |= IFF_UP;
			flags &= ~IFF_UP;
		} else if (strcmp(*argv, "name") == 0) {
			NEXT_ARG();
			newname = *argv;
		} else if (strcmp(*argv, "mtu") == 0) {
			NEXT_ARG();
			if (mtu != -1)
				duparg("mtu", *argv);
			if (get_integer(&mtu, *argv, 0))
				invarg(*argv, "mtu");
		} else if (strcmp(*argv, "multicast") == 0) {
			NEXT_ARG();
			mask |= IFF_MULTICAST;
			if (strcmp(*argv, "on") == 0) {
				flags |= IFF_MULTICAST;
			} else if (strcmp(*argv, "off") == 0) {
				flags &= ~IFF_MULTICAST;
			} else
				return on_off("multicast");
		} else if (strcmp(*argv, "arp") == 0) {
			NEXT_ARG();
			mask |= IFF_NOARP;
			if (strcmp(*argv, "on") == 0) {
				flags &= ~IFF_NOARP;
			} else if (strcmp(*argv, "off") == 0) {
				flags |= IFF_NOARP;
			} else
				return on_off("noarp");
		} else if (strcmp(*argv, "addr") == 0) {
			NEXT_ARG();
			newaddr = *argv;
		} else {
			if (strcmp(*argv, "dev") == 0) {
				NEXT_ARG();
			}
			if (dev)
				duparg2("dev", *argv);
			dev = *argv;
		}
		argc--; argv++;
	}

	if (!dev) {
		bb_error_msg(bb_msg_requires_arg, "\"dev\"");
		exit(-1);
	}

	if (newaddr || newbrd) {
		halen = get_address(dev, &htype);
		if (halen < 0)
			return -1;
		if (newaddr) {
			if (parse_address(dev, htype, halen, newaddr, &ifr0) < 0)
				return -1;
		}
		if (newbrd) {
			if (parse_address(dev, htype, halen, newbrd, &ifr1) < 0)
				return -1;
		}
	}

	if (newname && strcmp(dev, newname)) {
		if (do_changename(dev, newname) < 0)
			return -1;
		dev = newname;
	}
	if (qlen != -1) {
		if (set_qlen(dev, qlen) < 0)
			return -1;
	}
	if (mtu != -1) {
		if (set_mtu(dev, mtu) < 0)
			return -1;
	}
	if (newaddr || newbrd) {
		if (newbrd) {
			if (set_address(&ifr1, 1) < 0)
				return -1;
		}
		if (newaddr) {
			if (set_address(&ifr0, 0) < 0)
				return -1;
		}
	}
	if (mask)
		return do_chflags(dev, flags, mask);
	return 0;
}

static int ipaddr_list_link(int argc, char **argv)
{
	preferred_family = AF_PACKET;
	return ipaddr_list_or_flush(argc, argv, 0);
}

int do_iplink(int argc, char **argv)
{
	if (argc > 0) {
		if (matches(*argv, "set") == 0)
			return do_set(argc-1, argv+1);
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return ipaddr_list_link(argc-1, argv+1);
	} else
		return ipaddr_list_link(0, NULL);

	bb_error_msg("command \"%s\" is unknown", *argv);
	exit(-1);
}
