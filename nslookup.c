/* vi: set sw=4 ts=4: */
/*
 * Mini nslookup implementation for busybox
 *
 * Copyright (C) 2000 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "internal.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

/*
 |  I'm only implementing non-interactive mode;
 |  I totally forgot nslookup even had an interactive mode.
 |
 |  [ TODO ]
 |  + find out how to use non-default name servers
 |  + find out how the real nslookup gets the default name server
 */

static const char nslookup_usage[] = "nslookup [HOST]\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nQueries the nameserver for the IP address of the given HOST\n"
#endif
;


/* I have to see how the real nslookup does this.
 * I could dig through /etc/resolv.conf, but is there a
 * better (programatic) way?
 */
static void server_fprint(FILE * dst)
{
	fprintf(dst, "Server:     %s\n", "default");
	fprintf(dst, "Address:    %s\n\n", "default");
}

/* only works for IPv4 */
static int addr_fprint(char *addr, FILE * dst)
{
	u_int8_t split[4];
	u_int32_t ip;
	u_int32_t *x = (u_int32_t *) addr;

	ip = ntohl(*x);
	split[0] = (ip & 0xff000000) >> 24;
	split[1] = (ip & 0x00ff0000) >> 16;
	split[2] = (ip & 0x0000ff00) >> 8;
	split[3] = (ip & 0x000000ff);
	fprintf(dst, "%d.%d.%d.%d", split[0], split[1], split[2], split[3]
		);
	return 0;
}

/* changes a c-string matching the perl regex \d+\.\d+\.\d+\.\d+
 * into a u_int32_t
 */
static u_int32_t str_to_addr(const char *addr)
{
	u_int32_t split[4];
	u_int32_t ip;

	sscanf(addr, "%d.%d.%d.%d",
		   &split[0], &split[1], &split[2], &split[3]);

	/* assuming sscanf worked */
	ip = (split[0] << 24) |
		(split[1] << 16) | (split[2] << 8) | (split[3]);

	return htonl(ip);
}

/* takes the NULL-terminated array h_addr_list, and
 * prints its contents appropriately
 */
static int addr_list_fprint(char **h_addr_list, FILE * dst)
{
	int i, j;
	char *addr_string = (h_addr_list[1])
		? "Addresses: " : "Address:   ";

	fprintf(dst, "%s ", addr_string);
	for (i = 0, j = 0; h_addr_list[i]; i++, j++) {
		addr_fprint(h_addr_list[i], dst);

		/* real nslookup does this */
		if (j == 4) {
			if (h_addr_list[i + 1]) {
				fprintf(dst, "\n          ");
			}
			j = 0;
		} else {
			if (h_addr_list[i + 1]) {
				fprintf(dst, ", ");
			}
		}

	}
	fprintf(dst, "\n");
	return 0;
}

/* gethostbyaddr wrapper */
static struct hostent *gethostbyaddr_wrapper(const char *address)
{
	struct in_addr addr;

	addr.s_addr = str_to_addr(address);
	return gethostbyaddr((char *) &addr, 4, AF_INET);	/* IPv4 only for now */
}

/* print the results as nslookup would */
static struct hostent *hostent_fprint(struct hostent *host, FILE * dst)
{
	if (host) {
		fprintf(dst, "Name:       %s\n", host->h_name);
		addr_list_fprint(host->h_addr_list, dst);
	} else {
		fprintf(dst, "*** Unknown host\n");
	}
	return host;
}


/* naive function to check whether char *s is an ip address */
static int is_ip_address(const char *s)
{
	while (*s) {
		if ((isdigit(*s)) || (*s == '.')) {
			s++;
			continue;
		}
		return 0;
	}
	return 1;
}

/* ________________________________________________________________________ */
int nslookup_main(int argc, char **argv)
{
	struct hostent *host;

	if (argc < 2 || *argv[1]=='-') {
		usage(nslookup_usage);
	}

	server_fprint(stdout);
	if (is_ip_address(argv[1])) {
		host = gethostbyaddr_wrapper(argv[1]);
	} else {
		host = gethostbyname(argv[1]);
	}
	hostent_fprint(host, stdout);
	return( TRUE);
}

/* $Id: nslookup.c,v 1.10 2000/06/19 17:25:40 andersen Exp $ */
