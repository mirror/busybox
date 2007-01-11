/* vi: set sw=4 ts=4: */
/*
 * Mini nslookup implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 *
 * Correct default name server display and explicit name server option
 * added by Ben Zeckel <bzeckel@hmc.edu> June 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <resolv.h>
#include "busybox.h"

/*
 *  I'm only implementing non-interactive mode;
 *  I totally forgot nslookup even had an interactive mode.
 */

/* Examples of 'standard' nslookup output
 * $ nslookup yahoo.com
 * Server:         128.193.0.10
 * Address:        128.193.0.10#53
 *
 * Non-authoritative answer:
 * Name:   yahoo.com
 * Address: 216.109.112.135
 * Name:   yahoo.com
 * Address: 66.94.234.13
 *
 * $ nslookup 204.152.191.37
 * Server:         128.193.4.20
 * Address:        128.193.4.20#53
 *
 * Non-authoritative answer:
 * 37.191.152.204.in-addr.arpa     canonical name = 37.32-27.191.152.204.in-addr.arpa.
 * 37.32-27.191.152.204.in-addr.arpa       name = zeus-pub2.kernel.org.
 *
 * Authoritative answers can be found from:
 * 32-27.191.152.204.in-addr.arpa  nameserver = ns1.kernel.org.
 * 32-27.191.152.204.in-addr.arpa  nameserver = ns2.kernel.org.
 * 32-27.191.152.204.in-addr.arpa  nameserver = ns3.kernel.org.
 * ns1.kernel.org  internet address = 140.211.167.34
 * ns2.kernel.org  internet address = 204.152.191.4
 * ns3.kernel.org  internet address = 204.152.191.36
 */

static int sockaddr_to_dotted(struct sockaddr *saddr, char *buf, int buflen)
{
	if (buflen <= 0) return -1;
	buf[0] = '\0';
	if (saddr->sa_family == AF_INET) {
		inet_ntop(AF_INET, &((struct sockaddr_in*)saddr)->sin_addr, buf, buflen);
		return 0;
	}
	if (saddr->sa_family == AF_INET6) {
		inet_ntop(AF_INET6, &((struct sockaddr_in6*)saddr)->sin6_addr, buf, buflen);
		return 0;
	}
	return -1;
}

static int print_host(const char *hostname, const char *header)
{
	char str[128];	/* IPv6 address will fit, hostnames hopefully too */
	struct addrinfo *result = NULL;
	int rc;
	struct addrinfo hint;

	memset(&hint, 0 , sizeof(hint));
	/* hint.ai_family = AF_UNSPEC; - zero anyway */
	/* Needed. Or else we will get each address thrice (or more)
	 * for each possible socket type (tcp,udp,raw...): */
	hint.ai_socktype = SOCK_STREAM;
	// hint.ai_flags = AI_CANONNAME;
	rc = getaddrinfo(hostname, NULL /*service*/, &hint, &result);
	if (!rc) {
		struct addrinfo *cur = result;
		// printf("%s\n", cur->ai_canonname); ?
		while (cur) {
			sockaddr_to_dotted(cur->ai_addr, str, sizeof(str));
			printf("%s  %s\nAddress: %s", header, hostname, str);
			str[0] = ' ';
			if (getnameinfo(cur->ai_addr, cur->ai_addrlen, str+1, sizeof(str)-1, NULL, 0, NI_NAMEREQD))
				str[0] = '\0';
			puts(str);
			cur = cur->ai_next;
		}
	} else {
		bb_error_msg("getaddrinfo('%s') failed: %s", hostname, gai_strerror(rc));
	}
	freeaddrinfo(result);
	return (rc != 0);
}


/* alter the global _res nameserver structure to use
   an explicit dns server instead of what is in /etc/resolv.h */
static void set_default_dns(char *server)
{
	struct in_addr server_in_addr;

	if (inet_pton(AF_INET, server, &server_in_addr) > 0) {
		_res.nscount = 1;
		_res.nsaddr_list[0].sin_addr = server_in_addr;
	}
}


/* lookup the default nameserver and display it */
static void server_print(void)
{
	char str[INET6_ADDRSTRLEN];

	sockaddr_to_dotted((struct sockaddr*)&_res.nsaddr_list[0], str, sizeof(str));
	print_host(str, "Server:");
	puts("");
}


int nslookup_main(int argc, char **argv)
{
	/*
	* initialize DNS structure _res used in printing the default
	* name server and in the explicit name server option feature.
	*/

	res_init();

	/*
	* We allow 1 or 2 arguments.
	* The first is the name to be looked up and the second is an
	* optional DNS server with which to do the lookup.
	* More than 3 arguments is an error to follow the pattern of the
	* standard nslookup
	*/

	if (argc < 2 || *argv[1] == '-' || argc > 3)
		bb_show_usage();
	else if(argc == 3)
		set_default_dns(argv[2]);

	server_print();
	return print_host(argv[1], "Name:  ");
}

/* $Id: nslookup.c,v 1.33 2004/10/13 07:25:01 andersen Exp $ */
