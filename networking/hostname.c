/* vi: set sw=4 ts=4: */
/*
 * $Id: hostname.c,v 1.10 2000/06/19 17:25:39 andersen Exp $
 * Mini hostname implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * adjusted by Erik Andersen <andersee@debian.org> to remove
 * use of long options and GNU getopt.  Improved the usage info.
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
 */

#include "internal.h"
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>

static const char *hostname_usage =
	"hostname [OPTION] {hostname | -F file}\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nGet or set the hostname or DNS domain name. If a hostname is given\n"
	"(or a file with the -F parameter), the host name will be set.\n\n"
	"Options:\n"
	"\t-s\t\tShort\n"

	"\t-i\t\tAddresses for the hostname\n"
	"\t-d\t\tDNS domain name\n"
	"\t-F FILE\t\tUse the contents of FILE to specify the hostname\n"
#endif
	;


void do_sethostname(char *s, int isfile)
{
	FILE *f;
	char buf[255];

	if (!s)
		return;
	if (!isfile) {
		if (sethostname(s, strlen(s)) < 0) {
			if (errno == EPERM)
				fprintf(stderr,
						"hostname: you must be root to change the hostname\n");
			else
				perror("sethostname");
			exit(1);
		}
	} else {
		if ((f = fopen(s, "r")) == NULL) {
			perror(s);
			exit(1);
		} else {
			fgets(buf, 255, f);
			fclose(f);
			if (buf[strlen(buf) - 1] == '\n')
				buf[strlen(buf) - 1] = 0;
			if (sethostname(buf, strlen(buf)) < 0) {
				perror("sethostname");
				exit(1);
			}
		}
	}
}

int hostname_main(int argc, char **argv)
{
	int opt_short = 0;
	int opt_domain = 0;
	int opt_ip = 0;
	struct hostent *h;
	char *filename = NULL;
	char buf[255];
	char *s = NULL;

	if (argc < 1)
		usage(hostname_usage);

	while (--argc > 0 && **(++argv) == '-') {
		while (*(++(*argv))) {
			switch (**argv) {
			case 's':
				opt_short = 1;
				break;
			case 'i':
				opt_ip = 1;
				break;
			case 'd':
				opt_domain = 1;
				break;
			case 'F':
				if (--argc == 0) {
					usage(hostname_usage);
				}
				filename = *(++argv);
				break;
			default:
				usage(hostname_usage);
			}
			if (filename != NULL)
				break;
		}
	}

	if (argc >= 1) {
		do_sethostname(*argv, 0);
	} else if (filename != NULL) {
		do_sethostname(filename, 1);
	} else {
		gethostname(buf, 255);
		if (opt_short) {
			s = strchr(buf, '.');
			if (!s)
				s = buf;
			*s = 0;
			printf("%s\n", buf);
		} else if (opt_domain) {
			s = strchr(buf, '.');
			printf("%s\n", (s ? s + 1 : ""));
		} else if (opt_ip) {
			h = gethostbyname(buf);
			if (!h) {
				printf("Host not found\n");
				exit(1);
			}
			printf("%s\n", inet_ntoa(*(struct in_addr *) (h->h_addr)));
		} else {
			printf("%s\n", buf);
		}
	}
	return(0);
}
