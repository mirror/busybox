/* vi: set sw=4 ts=4: */
/*
 * $Id: hostname.c,v 1.36 2003/07/14 21:21:01 andersen Exp $
 * Mini hostname implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * adjusted by Erik Andersen <andersen@codepoet.org> to remove
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

#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "busybox.h"

extern char *optarg; /* in unistd.h */
extern int  optind, opterr, optopt; /* in unistd.h */

static void do_sethostname(char *s, int isfile)
{
	FILE *f;
	char buf[255];

	if (!s)
		return;
	if (!isfile) {
		if (sethostname(s, strlen(s)) < 0) {
			if (errno == EPERM)
				bb_error_msg_and_die("you must be root to change the hostname");
			else
				bb_perror_msg_and_die("sethostname");
		}
	} else {
		f = bb_xfopen(s, "r");
		while (fgets(buf, 255, f) != NULL) {
			if (buf[0] =='#') {
				continue;
			}
			chomp(buf);
			do_sethostname(buf, 0);
		}
#ifdef CONFIG_FEATURE_CLEAN_UP
		fclose(f);
#endif
	}
}

int hostname_main(int argc, char **argv)
{
	int opt;
	int type = 0;
	struct hostent *hp;
	char *filename = NULL;
	char buf[255];
	char *p = NULL;

	if (argc < 1)
		bb_show_usage();

        while ((opt = getopt(argc, argv, "dfisF:")) > 0) {
                switch (opt) {
		case 'd':
		case 'f':
		case 'i':
		case 's':
			type = opt;
			break;
		case 'F':
			filename = optarg;
			break;
		default:
			bb_show_usage();
		}
	}

	/* Output in desired format */
	if (type != 0) {
		gethostname(buf, 255);
		hp = xgethostbyname(buf);
		p = strchr(hp->h_name, '.');
		if (type == 'f') {
			puts(hp->h_name);
		} else if (type == 's') {
			if (p != NULL) {
				*p = 0;
			}
			puts(hp->h_name);
		} else if (type == 'd') {
			if (p) puts(p + 1);
		} else if (type == 'i') {
			while (hp->h_addr_list[0]) {
				printf("%s ", inet_ntoa(*(struct in_addr *) (*hp->h_addr_list++)));
			}
			printf("\n");
		}
	}
	/* Set the hostname */
	else if (filename != NULL) {
		do_sethostname(filename, 1);
	} else if (optind < argc) {
		do_sethostname(argv[optind], 0);
	}
	/* Or if all else fails,
	 * just print the current hostname */
	 else {
		gethostname(buf, 255);
		puts(buf);
	}
	return(0);
}
