/*
 * Mini nslookup implementation for busybox
 *
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


static const char nslookup_usage[] =
"only implementing non-interactive mode\n"
"I totally forgot nslookup even had an interactive mode\n"
;


/* */
static void
server_fprint(FILE *dst)
{
    fprintf(dst, "Server:  %s\n", "something");
    fprintf(dst, "Address:  %s\n\n", "something");
}

/* only works for IPv4 */
static int
addr_fprint(char *addr, FILE *dst)
{
    uint8_t	split[4];
    uint32_t	ip;
    uint32_t	*x = (uint32_t *) addr;

    ip = ntohl(*x);
    split[0] = (ip & 0xff000000) >> 24;
    split[1] = (ip & 0x00ff0000) >> 16;
    split[2] = (ip & 0x0000ff00) >>  8;
    split[3] = (ip & 0x000000ff);
    fprintf (
	dst, "%d.%d.%d.%d", 
	split[0], split[1], split[2], split[3]
    );
    return 0;
}

/* */
static uint32_t
str_to_addr(const char *addr)
{
    uint32_t	split[4];
    uint32_t	ip;

    sscanf(addr, "%d.%d.%d.%d", 
	    &split[0], &split[1], &split[2], &split[3]);

    /* assuming sscanf worked */
    ip = (split[0] << 24) |
	 (split[1] << 16) |
	 (split[2] << 8)  |
	 (split[3]);

    return htonl(ip);
}

/* */
static int
addr_list_fprint(char **h_addr_list, FILE *dst)
{
    int	    i;
    char    *addr_string = (h_addr_list[1]) 
	? "Addresses" 
	: "Address";

    fprintf(dst, "%s:  ", addr_string);
    for (i = 0; h_addr_list[i]; i++) {
	addr_fprint(h_addr_list[i], dst);
	if (h_addr_list[i+1]) {
	    fprintf(dst, ", ");
	}
    }
    fprintf(dst,"\n");
    return 0;
}

/* */
static struct hostent *
lookup_by_name(const char *hostname)
{
    struct hostent  *host;

    host = gethostbyname(hostname);
    if (host) {
	fprintf(stdout, "Name:    %s\n", host->h_name);
	addr_list_fprint(host->h_addr_list, stdout);
    } else {
	herror("crap");
    }
    return host;
}

/* */
static struct hostent *
lookup_by_addr(const char *addr)
{
    struct hostent  *host;

    host = gethostbyaddr(addr, 4, AF_INET); /* IPv4 only for now */
    if (host) {
	fprintf(stdout, "Name:    %s\n", host->h_name);
	addr_list_fprint(host->h_addr_list, stdout);
    } else {
	herror("crap");
    }
    return host;
}

/* */
static int
is_ip_address(const char *s)
{
    while (*s) {
	if ((isdigit(*s)) || (*s == '.')) { s++; continue; }
	return 0;
    }
    return 1;
}

/* ________________________________________________________________________ */
int
nslookup_main(int argc, char **argv)
{
    struct in_addr addr;

    server_fprint(stdout);
    if (is_ip_address(argv[1])) {
	addr.s_addr = str_to_addr(argv[1]);
	lookup_by_addr((char *) &addr); 
    } else {
	lookup_by_name(argv[1]);
    }
    return 0;
}

/* $Id: nslookup.c,v 1.1 2000/01/29 12:59:01 beppu Exp $ */
