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

/* take from linux/sockios.h */
#define SIOCSIFNAME	0x8923		/* set interface name */

/* Octets in one ethernet addr, from <linux/if_ether.h>	 */
#define ETH_ALEN	6

#ifndef ifr_newname
#define ifr_newname ifr_ifru.ifru_slave
#endif

typedef struct mactable_s {
	struct mactable_s *next;
	struct mactable_s *prev;
	char *ifname;
	struct ether_addr *mac;
} mactable_t;

static void serror(const char use_syslog, const char *s, ...)
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

/* Check ascii str_macaddr, convert and copy to *mac */
struct ether_addr *cc_macaddr(char *str_macaddr, unsigned char use_syslog)
{
	struct ether_addr *lmac, *mac;

	lmac = ether_aton(str_macaddr);
	if (lmac == NULL)
		serror(use_syslog, "cannot parse MAC %s", str_macaddr);
	mac = xcalloc(1, ETH_ALEN);
	memcpy(mac, lmac, ETH_ALEN);

	return mac;
}

int nameif_main(int argc, char **argv)
{
	mactable_t *clist = NULL;
	FILE *ifh;
	char *fname = "/etc/mactab";
	char *line;
	unsigned char use_syslog = 0;
	int ctl_sk = -1;
	int opt;
	int if_index = 1;
	mactable_t *ch = NULL;

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

	if ((argc - optind) & 1)
		show_usage();

	if (optind < argc) {
		while (optind < argc) {

			if (strlen(argv[optind]) > IF_NAMESIZE)
				serror(use_syslog, "interface name `%s' too long",
					   argv[optind]);
			optind++;

			ch = xcalloc(1, sizeof(mactable_t));
			ch->next = NULL;
			ch->prev = NULL;
			ch->ifname = strdup(argv[optind - 1]);
			ch->mac = cc_macaddr(argv[optind], use_syslog);
			optind++;
			if (clist)
				clist->prev = ch->next;
			ch->next = clist;
			ch->prev = clist;
			clist = ch;
		}
	} else {
		ifh = xfopen(fname, "r");

		while ((line = get_line_from_file(ifh)) != NULL) {
			char *line_ptr;
			unsigned short name_length;

			line_ptr = line + strspn(line, " \t");
			if ((line_ptr[0] == '#') || (line_ptr[0] == '\n'))
				continue;
			name_length = strcspn(line_ptr, " \t");
			if (name_length > IF_NAMESIZE)
				serror(use_syslog, "interface name `%s' too long",
					   argv[optind]);
			ch = xcalloc(1, sizeof(mactable_t));
			ch->next = NULL;
			ch->prev = NULL;
			ch->ifname = strndup(line_ptr, name_length);
			line_ptr += name_length;
			line_ptr += strspn(line_ptr, " \t");
			name_length = strspn(line_ptr, "0123456789ABCDEFabcdef:");
			line_ptr[name_length] = '\0';
			ch->mac = cc_macaddr(line_ptr, use_syslog);
			if (clist)
				clist->prev = ch;
			ch->next = clist;
			clist = ch;
			free(line);
		}
		fclose(ifh);
	}

	if ((ctl_sk = socket(PF_INET, SOCK_DGRAM, 0)) == -1)
		serror(use_syslog, "socket: %s", strerror(errno));

	while (clist) {
		struct ifreq ifr;

		bzero(&ifr, sizeof(struct ifreq));
		if_index++;
		ifr.ifr_ifindex = if_index;

		/* Get ifname by index or die */
		if (ioctl(ctl_sk, SIOCGIFNAME, &ifr))
			break;

		/* Has this device hwaddr? */
		if (ioctl(ctl_sk, SIOCGIFHWADDR, &ifr))
			continue;

		/* Search for mac like in ifr.ifr_hwaddr.sa_data */
		for (ch = clist; ch; ch = ch->next)
			if (!memcmp(ch->mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN))
				break;

		/* Nothing found for current ifr.ifr_hwaddr.sa_data */
		if (ch == NULL)
			continue;

		strcpy(ifr.ifr_newname, ch->ifname);
		if (ioctl(ctl_sk, SIOCSIFNAME, &ifr) < 0)
			serror(use_syslog, "cannot change ifname %s to %s: %s",
				   ifr.ifr_name, ch->ifname, strerror(errno));

		/* Remove list entry of renamed interface */
		if (ch->prev != NULL) {
			(ch->prev)->next = ch->next;
		} else {
			clist = ch->next;
		}
		if (ch->next != NULL)
			(ch->next)->prev = ch->prev;
		free(ch);
	}

	while (clist) {
		ch = clist;
		clist = clist->next;
		free(ch);
	}

	return 0;
}
