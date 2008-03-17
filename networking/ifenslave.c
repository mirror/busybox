/* Mode: C;
 *
 * Mini ifenslave implementation for busybox
 * Copyright (C) 2005 by Marc Leeman <marc.leeman@barco.com>
 *
 * ifenslave.c: Configure network interfaces for parallel routing.
 *
 *	This program controls the Linux implementation of running multiple
 *	network interfaces in parallel.
 *
 * Author:	Donald Becker <becker@cesdis.gsfc.nasa.gov>
 *		Copyright 1994-1996 Donald Becker
 *
 *		This program is free software; you can redistribute it
 *		and/or modify it under the terms of the GNU General Public
 *		License as published by the Free Software Foundation.
 *
 *	The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
 *	Center of Excellence in Space Data and Information Sciences
 *	   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
 *
 *  Changes :
 *    - 2000/10/02 Willy Tarreau <willy at meta-x.org> :
 *       - few fixes. Master's MAC address is now correctly taken from
 *         the first device when not previously set ;
 *       - detach support : call BOND_RELEASE to detach an enslaved interface.
 *       - give a mini-howto from command-line help : # ifenslave -h
 *
 *    - 2001/02/16 Chad N. Tindel <ctindel at ieee dot org> :
 *       - Master is now brought down before setting the MAC address.  In
 *         the 2.4 kernel you can't change the MAC address while the device is
 *         up because you get EBUSY.
 *
 *    - 2001/09/13 Takao Indoh <indou dot takao at jp dot fujitsu dot com>
 *       - Added the ability to change the active interface on a mode 1 bond
 *         at runtime.
 *
 *    - 2001/10/23 Chad N. Tindel <ctindel at ieee dot org> :
 *       - No longer set the MAC address of the master.  The bond device will
 *         take care of this itself
 *       - Try the SIOC*** versions of the bonding ioctls before using the
 *         old versions
 *    - 2002/02/18 Erik Habbinga <erik_habbinga @ hp dot com> :
 *       - ifr2.ifr_flags was not initialized in the hwaddr_notset case,
 *         SIOCGIFFLAGS now called before hwaddr_notset test
 *
 *    - 2002/10/31 Tony Cureington <tony.cureington * hp_com> :
 *       - If the master does not have a hardware address when the first slave
 *         is enslaved, the master is assigned the hardware address of that
 *         slave - there is a comment in bonding.c stating "ifenslave takes
 *         care of this now." This corrects the problem of slaves having
 *         different hardware addresses in active-backup mode when
 *         multiple interfaces are specified on a single ifenslave command
 *         (ifenslave bond0 eth0 eth1).
 *
 *    - 2003/03/18 - Tsippy Mendelson <tsippy.mendelson at intel dot com> and
 *                   Shmulik Hen <shmulik.hen at intel dot com>
 *       - Moved setting the slave's mac address and openning it, from
 *         the application to the driver. This enables support of modes
 *         that need to use the unique mac address of each slave.
 *         The driver also takes care of closing the slave and restoring its
 *         original mac address upon release.
 *         In addition, block possibility of enslaving before the master is up.
 *         This prevents putting the system in an undefined state.
 *
 *    - 2003/05/01 - Amir Noam <amir.noam at intel dot com>
 *       - Added ABI version control to restore compatibility between
 *         new/old ifenslave and new/old bonding.
 *       - Prevent adding an adapter that is already a slave.
 *         Fixes the problem of stalling the transmission and leaving
 *         the slave in a down state.
 *
 *    - 2003/05/01 - Shmulik Hen <shmulik.hen at intel dot com>
 *       - Prevent enslaving if the bond device is down.
 *         Fixes the problem of leaving the system in unstable state and
 *         halting when trying to remove the module.
 *       - Close socket on all abnormal exists.
 *       - Add versioning scheme that follows that of the bonding driver.
 *         current version is 1.0.0 as a base line.
 *
 *    - 2003/05/22 - Jay Vosburgh <fubar at us dot ibm dot com>
 *	 - ifenslave -c was broken; it's now fixed
 *	 - Fixed problem with routes vanishing from master during enslave
 *	   processing.
 *
 *    - 2003/05/27 - Amir Noam <amir.noam at intel dot com>
 *	 - Fix backward compatibility issues:
 *	   For drivers not using ABI versions, slave was set down while
 *	   it should be left up before enslaving.
 *	   Also, master was not set down and the default set_mac_address()
 *	   would fail and generate an error message in the system log.
 * 	 - For opt_c: slave should not be set to the master's setting
 *	   while it is running. It was already set during enslave. To
 *	   simplify things, it is now handeled separately.
 *
 *    - 2003/12/01 - Shmulik Hen <shmulik.hen at intel dot com>
 *	 - Code cleanup and style changes
 *	   set version to 1.1.0
 */

#include "libbb.h"

#include <net/if_arp.h>
#include <linux/if_bonding.h>
#include <linux/sockios.h>

typedef unsigned long long u64; /* hack, so we may include kernel's ethtool.h */
typedef uint32_t u32;           /* ditto */
typedef uint16_t u16;           /* ditto */
typedef uint8_t u8;             /* ditto */
#include <linux/ethtool.h>


struct dev_data {
	struct ifreq mtu, flags, hwaddr;
};


enum { skfd = 3 };      /* AF_INET socket for ioctl() calls.*/
struct globals {
	unsigned abi_ver;       /* userland - kernel ABI version */
	smallint hwaddr_set;    /* Master's hwaddr is set */
	struct dev_data master;
	struct dev_data slave;
};
#define G (*ptr_to_globals)
#define abi_ver    (G.abi_ver   )
#define hwaddr_set (G.hwaddr_set)
#define master     (G.master    )
#define slave      (G.slave     )
#define INIT_G() do { \
        SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)


static void get_drv_info(char *master_ifname);
static int get_if_settings(char *ifname, struct dev_data *dd);
static int get_slave_flags(char *slave_ifname);
static int set_master_hwaddr(char *master_ifname, struct sockaddr *hwaddr);
static int set_slave_hwaddr(char *slave_ifname, struct sockaddr *hwaddr);
static int set_slave_mtu(char *slave_ifname, int mtu);
static int set_if_flags(char *ifname, short flags);
static int set_if_up(char *ifname, short flags);
static int set_if_down(char *ifname, short flags);
static int clear_if_addr(char *ifname);
static int set_if_addr(char *master_ifname, char *slave_ifname);
static void change_active(char *master_ifname, char *slave_ifname);
static int enslave(char *master_ifname, char *slave_ifname);
static int release(char *master_ifname, char *slave_ifname);


int ifenslave_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ifenslave_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	char *master_ifname, *slave_ifname;
	int rv;
	int res;
	unsigned opt;
	enum {
		OPT_c = (1 << 0),
		OPT_d = (1 << 1),
		OPT_f = (1 << 2),
	};
#if ENABLE_GETOPT_LONG
	static const char ifenslave_longopts[] ALIGN1 =
		"change-active" No_argument "c"
		"detach"        No_argument "d"
		"force"         No_argument "f"
	;

	applet_long_options = ifenslave_longopts;
#endif
	opt = getopt32(argv, "cdf");
	argv += optind;
	if (opt & (opt-1)) /* options check */
		bb_show_usage();

	/* Copy the interface name */
	master_ifname = *argv++;

	/* No remaining args means show all interfaces. */
	if (!master_ifname) {
		display_interfaces(NULL);
		return EXIT_SUCCESS;
	}

	/* Open a basic socket */
	xmove_fd(xsocket(AF_INET, SOCK_DGRAM, 0), skfd);

	/* exchange abi version with bonding module */
	get_drv_info(master_ifname);

	slave_ifname = *argv++;
	if (!slave_ifname) {
		if (opt & (OPT_d|OPT_c)) {
			display_interfaces(slave_ifname);
			return 2; /* why? */
		}
		/* A single arg means show the
		 * configuration for this interface
		 */
		display_interfaces(master_ifname);
		return EXIT_SUCCESS;
	}

	res = get_if_settings(master_ifname, &master);
	if (res) {
		/* Probably a good reason not to go on */
		bb_perror_msg_and_die("%s: can't get settings", master_ifname);
	}

	/* check if master is indeed a master;
	 * if not then fail any operation
	 */
	if (!(master.flags.ifr_flags & IFF_MASTER))
		bb_error_msg_and_die("%s is not a master", master_ifname);

	/* check if master is up; if not then fail any operation */
	if (!(master.flags.ifr_flags & IFF_UP))
		bb_error_msg_and_die("%s is not up", master_ifname);

	/* Only for enslaving */
	if (!(opt & (OPT_c|OPT_d))) {
		sa_family_t master_family = master.hwaddr.ifr_hwaddr.sa_family;

		/* The family '1' is ARPHRD_ETHER for ethernet. */
		if (master_family != 1 && !(opt & OPT_f)) {
			bb_error_msg_and_die(
				"%s is not ethernet-like (-f overrides)",
				master_ifname);
		}
	}

	/* Accepts only one slave */
	if (opt & OPT_c) {
		/* change active slave */
		res = get_slave_flags(slave_ifname);
		if (res) {
			bb_perror_msg_and_die(
				"%s: can't get flags", slave_ifname);
		}
		change_active(master_ifname, slave_ifname);
		return EXIT_SUCCESS;
	}

	/* Accept multiple slaves */
	res = 0;
	do {
		if (opt & OPT_d) {
			/* detach a slave interface from the master */
			rv = get_slave_flags(slave_ifname);
			if (rv) {
				/* Can't work with this slave. */
				/* remember the error and skip it*/
				bb_perror_msg(
					"skipping %s: can't get flags",
					slave_ifname);
				res = rv;
				continue;
			}
			rv = release(master_ifname, slave_ifname);
			if (rv) {
				bb_perror_msg(
					"master %s, slave %s: "
					"can't release",
					master_ifname, slave_ifname);
				res = rv;
			}
		} else {
			/* attach a slave interface to the master */
			rv = get_if_settings(slave_ifname, &slave);
			if (rv) {
				/* Can't work with this slave. */
				/* remember the error and skip it*/
				bb_perror_msg(
					"skipping %s: can't get settings",
					slave_ifname);
				res = rv;
				continue;
			}
			rv = enslave(master_ifname, slave_ifname);
			if (rv) {
				bb_perror_msg(
					"master %s, slave %s: "
					"can't enslave",
					master_ifname, slave_ifname);
				res = rv;
			}
		}
	} while ((slave_ifname = *argv++) != NULL);

	if (ENABLE_FEATURE_CLEAN_UP) {
		close(skfd);
	}

	return res;
}

static void get_drv_info(char *master_ifname)
{
	struct ifreq ifr;
	struct ethtool_drvinfo info;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, master_ifname, IFNAMSIZ);
	ifr.ifr_data = (caddr_t)&info;

	info.cmd = ETHTOOL_GDRVINFO;
	strncpy(info.driver, "ifenslave", 32);
	snprintf(info.fw_version, 32, "%d", BOND_ABI_VERSION);

	if (ioctl(skfd, SIOCETHTOOL, &ifr) < 0) {
		if (errno == EOPNOTSUPP)
			return;
		bb_perror_msg_and_die("%s: SIOCETHTOOL error", master_ifname);
	}

	abi_ver = bb_strtou(info.fw_version, NULL, 0);
	if (errno)
		bb_error_msg_and_die("%s: SIOCETHTOOL error", master_ifname);

	return;
}

static void change_active(char *master_ifname, char *slave_ifname)
{
	struct ifreq ifr;

	if (!(slave.flags.ifr_flags & IFF_SLAVE)) {
		bb_error_msg_and_die(
			"%s is not a slave",
			slave_ifname);
	}

	strncpy(ifr.ifr_name, master_ifname, IFNAMSIZ);
	strncpy(ifr.ifr_slave, slave_ifname, IFNAMSIZ);
	if (ioctl(skfd, SIOCBONDCHANGEACTIVE, &ifr) < 0
	 && ioctl(skfd, BOND_CHANGE_ACTIVE_OLD, &ifr) < 0
	) {
		bb_perror_msg_and_die(
			"master %s, slave %s: can't "
			"change active",
			master_ifname, slave_ifname);
	}
}

static int enslave(char *master_ifname, char *slave_ifname)
{
	struct ifreq ifr;
	int res;

	if (slave.flags.ifr_flags & IFF_SLAVE) {
		bb_error_msg(
			"%s is already a slave",
			slave_ifname);
		return 1;
	}

	res = set_if_down(slave_ifname, slave.flags.ifr_flags);
	if (res)
		return res;

	if (abi_ver < 2) {
		/* Older bonding versions would panic if the slave has no IP
		 * address, so get the IP setting from the master.
		 */
		res = set_if_addr(master_ifname, slave_ifname);
		if (res) {
			bb_perror_msg("%s: can't set address", slave_ifname);
			return res;
		}
	} else {
		res = clear_if_addr(slave_ifname);
		if (res) {
			bb_perror_msg("%s: can't clear address", slave_ifname);
			return res;
		}
	}

	if (master.mtu.ifr_mtu != slave.mtu.ifr_mtu) {
		res = set_slave_mtu(slave_ifname, master.mtu.ifr_mtu);
		if (res) {
			bb_perror_msg("%s: can't set MTU", slave_ifname);
			return res;
		}
	}

	if (hwaddr_set) {
		/* Master already has an hwaddr
		 * so set it's hwaddr to the slave
		 */
		if (abi_ver < 1) {
			/* The driver is using an old ABI, so
			 * the application sets the slave's
			 * hwaddr
			 */
			res = set_slave_hwaddr(slave_ifname,
					       &(master.hwaddr.ifr_hwaddr));
			if (res) {
				bb_perror_msg("%s: can't set hw address",
						slave_ifname);
				goto undo_mtu;
			}

			/* For old ABI the application needs to bring the
			 * slave back up
			 */
			res = set_if_up(slave_ifname, slave.flags.ifr_flags);
			if (res)
				goto undo_slave_mac;
		}
		/* The driver is using a new ABI,
		 * so the driver takes care of setting
		 * the slave's hwaddr and bringing
		 * it up again
		 */
	} else {
		/* No hwaddr for master yet, so
		 * set the slave's hwaddr to it
		 */
		if (abi_ver < 1) {
			/* For old ABI, the master needs to be
			 * down before setting it's hwaddr
			 */
			res = set_if_down(master_ifname, master.flags.ifr_flags);
			if (res)
				goto undo_mtu;
		}

		res = set_master_hwaddr(master_ifname,
					&(slave.hwaddr.ifr_hwaddr));
		if (res) {
			bb_error_msg("%s: can't set hw address",
				master_ifname);
			goto undo_mtu;
		}

		if (abi_ver < 1) {
			/* For old ABI, bring the master
			 * back up
			 */
			res = set_if_up(master_ifname, master.flags.ifr_flags);
			if (res)
				goto undo_master_mac;
		}

		hwaddr_set = 1;
	}

	/* Do the real thing */
	strncpy(ifr.ifr_name, master_ifname, IFNAMSIZ);
	strncpy(ifr.ifr_slave, slave_ifname, IFNAMSIZ);
	if (ioctl(skfd, SIOCBONDENSLAVE, &ifr) < 0
	 && ioctl(skfd, BOND_ENSLAVE_OLD, &ifr) < 0
	) {
		res = 1;
	}

	if (res)
		goto undo_master_mac;

	return 0;

/* rollback (best effort) */
 undo_master_mac:
	set_master_hwaddr(master_ifname, &(master.hwaddr.ifr_hwaddr));
	hwaddr_set = 0;
	goto undo_mtu;
 undo_slave_mac:
	set_slave_hwaddr(slave_ifname, &(slave.hwaddr.ifr_hwaddr));
 undo_mtu:
	set_slave_mtu(slave_ifname, slave.mtu.ifr_mtu);
	return res;
}

static int release(char *master_ifname, char *slave_ifname)
{
	struct ifreq ifr;
	int res = 0;

	if (!(slave.flags.ifr_flags & IFF_SLAVE)) {
		bb_error_msg("%s is not a slave",
			slave_ifname);
		return 1;
	}

	strncpy(ifr.ifr_name, master_ifname, IFNAMSIZ);
	strncpy(ifr.ifr_slave, slave_ifname, IFNAMSIZ);
	if (ioctl(skfd, SIOCBONDRELEASE, &ifr) < 0
	 && ioctl(skfd, BOND_RELEASE_OLD, &ifr) < 0
	) {
		return 1;
	}
	if (abi_ver < 1) {
		/* The driver is using an old ABI, so we'll set the interface
		 * down to avoid any conflicts due to same MAC/IP
		 */
		res = set_if_down(slave_ifname, slave.flags.ifr_flags);
	}

	/* set to default mtu */
	set_slave_mtu(slave_ifname, 1500);

	return res;
}

static int get_if_settings(char *ifname, struct dev_data *dd)
{
	int res;

	strncpy(dd->mtu.ifr_name, ifname, IFNAMSIZ);
	res = ioctl(skfd, SIOCGIFMTU, &dd->mtu);
	strncpy(dd->flags.ifr_name, ifname, IFNAMSIZ);
	res |= ioctl(skfd, SIOCGIFFLAGS, &dd->flags);
	strncpy(dd->hwaddr.ifr_name, ifname, IFNAMSIZ);
	res |= ioctl(skfd, SIOCGIFHWADDR, &dd->hwaddr);

	return res;
}

static int get_slave_flags(char *slave_ifname)
{
	strncpy(slave.flags.ifr_name, slave_ifname, IFNAMSIZ);
	return ioctl(skfd, SIOCGIFFLAGS, &slave.flags);
}

static int set_master_hwaddr(char *master_ifname, struct sockaddr *hwaddr)
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, master_ifname, IFNAMSIZ);
	memcpy(&(ifr.ifr_hwaddr), hwaddr, sizeof(struct sockaddr));
	return ioctl(skfd, SIOCSIFHWADDR, &ifr);
}

static int set_slave_hwaddr(char *slave_ifname, struct sockaddr *hwaddr)
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, slave_ifname, IFNAMSIZ);
	memcpy(&(ifr.ifr_hwaddr), hwaddr, sizeof(struct sockaddr));
	return ioctl(skfd, SIOCSIFHWADDR, &ifr);
}

static int set_slave_mtu(char *slave_ifname, int mtu)
{
	struct ifreq ifr;

	ifr.ifr_mtu = mtu;
	strncpy(ifr.ifr_name, slave_ifname, IFNAMSIZ);
	return ioctl(skfd, SIOCSIFMTU, &ifr);
}

static int set_if_flags(char *ifname, short flags)
{
	struct ifreq ifr;

	ifr.ifr_flags = flags;
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	return ioctl(skfd, SIOCSIFFLAGS, &ifr);
}

static int set_if_up(char *ifname, short flags)
{
	int res = set_if_flags(ifname, flags | IFF_UP);
	if (res)
		bb_perror_msg("%s: can't up", ifname);
	return res;
}

static int set_if_down(char *ifname, short flags)
{
	int res = set_if_flags(ifname, flags & ~IFF_UP);
	if (res)
		bb_perror_msg("%s: can't down", ifname);
	return res;
}

static int clear_if_addr(char *ifname)
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
	ifr.ifr_addr.sa_family = AF_INET;
	memset(ifr.ifr_addr.sa_data, 0, sizeof(ifr.ifr_addr.sa_data));
	return ioctl(skfd, SIOCSIFADDR, &ifr);
}

static int set_if_addr(char *master_ifname, char *slave_ifname)
{
	static const struct {
		int g_ioctl;
		int s_ioctl;
	} ifra[] = {
		{ SIOCGIFADDR,    SIOCSIFADDR    },
		{ SIOCGIFDSTADDR, SIOCSIFDSTADDR },
		{ SIOCGIFBRDADDR, SIOCSIFBRDADDR },
		{ SIOCGIFNETMASK, SIOCSIFNETMASK },
	};

	struct ifreq ifr;
	int res;
	int i;

	for (i = 0; i < ARRAY_SIZE(ifra); i++) {
		strncpy(ifr.ifr_name, master_ifname, IFNAMSIZ);
		res = ioctl(skfd, ifra[i].g_ioctl, &ifr);
		if (res < 0) {
			ifr.ifr_addr.sa_family = AF_INET;
			memset(ifr.ifr_addr.sa_data, 0,
			       sizeof(ifr.ifr_addr.sa_data));
		}

		strncpy(ifr.ifr_name, slave_ifname, IFNAMSIZ);
		res = ioctl(skfd, ifra[i].s_ioctl, &ifr);
		if (res < 0)
			return res;
	}

	return 0;
}
