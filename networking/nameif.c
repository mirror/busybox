/* 
 * nameif.c - Naming Interfaces based on MAC address for busybox.
 *
 * Writen 2000 by Andi Kleen.
 * Busybox port 2002 by Nick Fedchik <nick@fedchik.org.ua>
 *			Glenn McGrath <bug1@optushome.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307 USA
 *
 */

#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <netinet/ether.h>

#include "busybox.h"

/* set interface name, from <linux/sockios.h> */
#define SIOCSIFNAME	0x8923
/* Octets in one ethernet addr, from <linux/if_ether.h>	 */
#define ETH_ALEN	6

#ifndef ifr_newname
#define ifr_newname ifr_ifru.ifru_slave
#endif

typedef struct mactable_s {
	struct mactable_s *next;
	struct mactable_s **pprev;
	char *ifname;
	struct ether_addr *mac;
} mactable_t;

static void serror_msg_and_die(const char use_syslog, const char *s, ...)
{
	va_list ap;

	va_start(ap, s);

	if (use_syslog) {
		openlog("nameif", 0, LOG_LOCAL0);
		syslog(LOG_ERR, s, ap);
		closelog();
	} else {
		vfprintf(stderr, s, ap);
		putc('\n', stderr);
	}

	va_end(ap);

	exit(EXIT_FAILURE);
}

int nameif_main(int argc, char **argv)
{
	mactable_t *clist = NULL;
	FILE *ifh;
	char *fname = "/etc/mactab";
	char *line;
	unsigned short linenum = 0;
	unsigned char use_syslog = 0;
	int ctl_sk = -1;
	int opt;

	static struct option opts[] = {
		{"syslog", 0, NULL, 's'},
		{"configfile", 1, NULL, 'c'},
		{NULL},
	};

	while ((opt = getopt_long(argc, argv, "c:s", opts, NULL)) != -1) {
		switch (opt) {
		case 'c':
			fname = optarg;
			break;
		case 's':
			use_syslog = 1;
			break;
		default:
			show_usage();
		}
	}

	if ((argc - optind) & 1) {
		show_usage();
	}

	if (optind < argc) {
		while (optind < argc) {
			struct ether_addr *mac;
			mactable_t *ch;

			if (strlen(argv[optind]) > IF_NAMESIZE) {
				serror_msg_and_die(use_syslog, "interface name `%s' too long", argv[optind]);
			}
			optind++;
			mac = ether_aton(argv[optind]);
			if (mac == NULL) {
				serror_msg_and_die(use_syslog, "cannot parse MAC %s", argv[optind]);
			}
			ch = xcalloc(1, sizeof(mactable_t));
			ch->ifname = strdup(argv[optind - 1]);
			ch->mac = xcalloc(1, ETH_ALEN);
			memcpy(ch->mac, &mac, ETH_ALEN);
			optind++;
			if (clist)
				clist->pprev = &ch->next;
			ch->next = clist;
			ch->pprev = &clist;
			clist = ch;
		}
	} else {
		ifh = xfopen(fname, "r");

		while ((line = get_line_from_file(ifh)) != NULL) {
			struct ether_addr *mac;
			mactable_t *ch;
			char *line_ptr;
			unsigned short name_length;

			line_ptr = line + strspn(line, " \t");
			if ((line_ptr[0] == '#') || (line_ptr[0] == '\n'))
				continue;
			name_length = strcspn(line_ptr, " \t");
			if (name_length > IF_NAMESIZE) {
				serror_msg_and_die(use_syslog, "interface name `%s' too long", argv[optind]); 
			}
			ch = xcalloc(1, sizeof(mactable_t));
			ch->ifname = strndup(line_ptr, name_length);
			line_ptr += name_length;
			line_ptr += strspn(line_ptr, " \t");
			name_length = strspn(line_ptr, "0123456789ABCDEFabcdef:");
			line_ptr[name_length] = '\0';
			mac = ether_aton(line_ptr);
			if (mac == NULL) {
				serror_msg_and_die(use_syslog,  "cannot parse MAC %s", argv[optind]);
			}
			ch->mac = xcalloc(1, ETH_ALEN);
			memcpy(ch->mac, mac, ETH_ALEN);
			if (clist)
				clist->pprev = &ch->next;
			ch->next = clist;
			ch->pprev = &clist;
			clist = ch;
			free(line);
		}
		fclose(ifh);
	}

	ifh = xfopen("/proc/net/dev", "r");
	while ((line = get_line_from_file(ifh)) != NULL) {
		char *line_ptr;
		unsigned short iface_name_length;
		struct ifreq ifr;
		mactable_t *ch = NULL;

		linenum++;
		if (linenum < 3)
			continue;
		line_ptr = line + strspn(line, " \t");
		if (line_ptr[0] == '\n')
			continue;
		iface_name_length = strcspn(line_ptr, ":");
		if (ctl_sk < 0)
			ctl_sk = socket(PF_INET, SOCK_DGRAM, 0);
		memset(&ifr, 0, sizeof(struct ifreq));
		strncpy(ifr.ifr_name, line_ptr, iface_name_length);
		if (ioctl(ctl_sk, SIOCGIFHWADDR, &ifr) < 0) {
			serror_msg_and_die(use_syslog, "cannot change name of %s to %s: %s", ifr.ifr_name, ch->ifname, strerror(errno));
		}
		for (ch = clist; ch; ch = ch->next)
			if (!memcmp(ch->mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN))
				break;
		if (ch == NULL) {
			continue;
		}
		strcpy(ifr.ifr_newname, ch->ifname);

		if (ioctl(ctl_sk, SIOCSIFNAME, &ifr) < 0) {;
			serror_msg_and_die(use_syslog, "cannot change name of %s to %s: %s", ifr.ifr_name, ch->ifname, strerror(errno));
		}
		*ch->pprev = ch->next;
		free(ch);
		free(line);
	}
	fclose(ifh);

	while (clist) {
		mactable_t *ch;

		ch = clist;
		clist = clist->next;
		free(ch);
	}

	return 0;
}
