/*
 * ip.c		"ip" utility frontend.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 */

#include <string.h>

#include "utils.h"
#include "ip_common.h"

#include "busybox.h"

int preferred_family = AF_UNSPEC;
int oneline = 0;
char * _SL_ = NULL;

void ip_parse_common_args(int *argcp, char ***argvp)
{
	int argc = *argcp;
	char **argv = *argvp;

	while (argc > 1) {
		char *opt = argv[1];

		if (strcmp(opt,"--") == 0) {
			argc--; argv++;
			break;
		}

		if (opt[0] != '-')
			break;

		if (opt[1] == '-')
			opt++;

		if (matches(opt, "-family") == 0) {
			argc--;
			argv++;
			if (! argv[1]) 
			    bb_show_usage();
			if (strcmp(argv[1], "inet") == 0)
				preferred_family = AF_INET;
			else if (strcmp(argv[1], "inet6") == 0)
				preferred_family = AF_INET6;
			else if (strcmp(argv[1], "link") == 0)
				preferred_family = AF_PACKET;
			else
				invarg(argv[1], "invalid protocol family");
		} else if (strcmp(opt, "-4") == 0) {
			preferred_family = AF_INET;
		} else if (strcmp(opt, "-6") == 0) {
			preferred_family = AF_INET6;
		} else if (strcmp(opt, "-0") == 0) {
			preferred_family = AF_PACKET;
		} else if (matches(opt, "-oneline") == 0) {
			++oneline;
		} else {
			bb_show_usage();
		}
		argc--;	argv++;
	}
	_SL_ = oneline ? "\\" : "\n" ;
	*argcp = argc;
	*argvp = argv;
}
