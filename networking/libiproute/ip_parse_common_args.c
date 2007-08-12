/* vi: set sw=4 ts=4: */
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

#include "ip_common.h"  /* #include "libbb.h" is inside */
#include "utils.h"

int preferred_family = AF_UNSPEC;
smallint oneline;
char _SL_;

void ip_parse_common_args(int *argcp, char ***argvp)
{
	int argc = *argcp;
	char **argv = *argvp;
	static const char ip_common_commands[] ALIGN1 =
		"-family\0""inet\0""inet6\0""link\0"
		"-4\0""-6\0""-0\0""-oneline\0";
	enum {
		ARG_family = 1,
		ARG_inet,
		ARG_inet6,
		ARG_link,
		ARG_IPv4,
		ARG_IPv6,
		ARG_packet,
		ARG_oneline
	};
	smalluint arg;

	while (argc > 1) {
		char *opt = argv[1];

		if (strcmp(opt,"--") == 0) {
			argc--;
			argv++;
			break;
		}
		if (opt[0] != '-')
			break;
		if (opt[1] == '-')
			opt++;
		arg = index_in_strings(ip_common_commands, opt) + 1;
		if (arg == ARG_family) {
			argc--;
			argv++;
			if (!argv[1])
				bb_show_usage();
			arg = index_in_strings(ip_common_commands, argv[1]) + 1;
			if (arg == ARG_inet)
				preferred_family = AF_INET;
			else if (arg == ARG_inet6)
				preferred_family = AF_INET6;
			else if (arg == ARG_link)
				preferred_family = AF_PACKET;
			else
				invarg(argv[1], "protocol family");
		} else if (arg == ARG_IPv4) {
			preferred_family = AF_INET;
		} else if (arg == ARG_IPv6) {
			preferred_family = AF_INET6;
		} else if (arg == ARG_packet) {
			preferred_family = AF_PACKET;
		} else if (arg == ARG_oneline) {
			++oneline;
		} else {
			bb_show_usage();
		}
		argc--;
		argv++;
	}
	_SL_ = oneline ? '\\' : '\n';
	*argcp = argc;
	*argvp = argv;
}
