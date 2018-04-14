/* vi: set sw=4 ts=4: */

//config:config NSLOOKUP
//config:	bool "nslookup (4.5 kb)"
//config:	default y
//config:	help
//config:	nslookup is a tool to query Internet name servers.
//config:
//config:config NSLOOKUP_BIG
//config:	bool "Use internal resolver code instead of libc"
//config:	depends on NSLOOKUP
//config:	default y
//config:
//config:config FEATURE_NSLOOKUP_LONG_OPTIONS
//config:	bool "Enable long options"
//config:	default y
//config:	depends on NSLOOKUP_BIG && LONG_OPTS

//applet:IF_NSLOOKUP(APPLET(nslookup, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_NSLOOKUP) += nslookup.o

//usage:#define nslookup_trivial_usage
//usage:       "[HOST] [SERVER]"
//usage:#define nslookup_full_usage "\n\n"
//usage:       "Query the nameserver for the IP address of the given HOST\n"
//usage:       "optionally using a specified DNS server"
//usage:
//usage:#define nslookup_example_usage
//usage:       "$ nslookup localhost\n"
//usage:       "Server:     default\n"
//usage:       "Address:    default\n"
//usage:       "\n"
//usage:       "Name:       debian\n"
//usage:       "Address:    127.0.0.1\n"

#include <resolv.h>
#include <net/if.h>	/* for IFNAMSIZ */
//#include <arpa/inet.h>
//#include <netdb.h>
#include "libbb.h"
#include "common_bufsiz.h"


#if !ENABLE_NSLOOKUP_BIG

/*
 * Mini nslookup implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 *
 * Correct default name server display and explicit name server option
 * added by Ben Zeckel <bzeckel@hmc.edu> June 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/*
 * I'm only implementing non-interactive mode;
 * I totally forgot nslookup even had an interactive mode.
 *
 * This applet is the only user of res_init(). Without it,
 * you may avoid pulling in _res global from libc.
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

static int print_host(const char *hostname, const char *header)
{
	/* We can't use xhost2sockaddr() - we want to get ALL addresses,
	 * not just one */
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

	if (rc == 0) {
		struct addrinfo *cur = result;
		unsigned cnt = 0;

		printf("%-10s %s\n", header, hostname);
		// puts(cur->ai_canonname); ?
		while (cur) {
			char *dotted, *revhost;
			dotted = xmalloc_sockaddr2dotted_noport(cur->ai_addr);
			revhost = xmalloc_sockaddr2hostonly_noport(cur->ai_addr);

			printf("Address %u: %s%c", ++cnt, dotted, revhost ? ' ' : '\n');
			if (revhost) {
				puts(revhost);
				if (ENABLE_FEATURE_CLEAN_UP)
					free(revhost);
			}
			if (ENABLE_FEATURE_CLEAN_UP)
				free(dotted);
			cur = cur->ai_next;
		}
	} else {
#if ENABLE_VERBOSE_RESOLUTION_ERRORS
		bb_error_msg("can't resolve '%s': %s", hostname, gai_strerror(rc));
#else
		bb_error_msg("can't resolve '%s'", hostname);
#endif
	}
	if (ENABLE_FEATURE_CLEAN_UP && result)
		freeaddrinfo(result);
	return (rc != 0);
}

/* lookup the default nameserver and display it */
static void server_print(void)
{
	char *server;
	struct sockaddr *sa;

#if ENABLE_FEATURE_IPV6
	sa = (struct sockaddr*)_res._u._ext.nsaddrs[0];
	if (!sa)
#endif
		sa = (struct sockaddr*)&_res.nsaddr_list[0];
	server = xmalloc_sockaddr2dotted_noport(sa);

	print_host(server, "Server:");
	if (ENABLE_FEATURE_CLEAN_UP)
		free(server);
	bb_putchar('\n');
}

/* alter the global _res nameserver structure to use
   an explicit dns server instead of what is in /etc/resolv.conf */
static void set_default_dns(const char *server)
{
	len_and_sockaddr *lsa;

	if (!server)
		return;

	/* NB: this works even with, say, "[::1]:5353"! :) */
	lsa = xhost2sockaddr(server, 53);

	if (lsa->u.sa.sa_family == AF_INET) {
		_res.nscount = 1;
		/* struct copy */
		_res.nsaddr_list[0] = lsa->u.sin;
	}
#if ENABLE_FEATURE_IPV6
	/* Hoped libc can cope with IPv4 address there too.
	 * No such luck, glibc 2.4 segfaults even with IPv6,
	 * maybe I misunderstand how to make glibc use IPv6 addr?
	 * (uclibc 0.9.31+ should work) */
	if (lsa->u.sa.sa_family == AF_INET6) {
		// glibc neither SEGVs nor sends any dgrams with this
		// (strace shows no socket ops):
		//_res.nscount = 0;
		_res._u._ext.nscount = 1;
		/* store a pointer to part of malloc'ed lsa */
		_res._u._ext.nsaddrs[0] = &lsa->u.sin6;
		/* must not free(lsa)! */
	}
#endif
}

int nslookup_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nslookup_main(int argc, char **argv)
{
	/* We allow 1 or 2 arguments.
	 * The first is the name to be looked up and the second is an
	 * optional DNS server with which to do the lookup.
	 * More than 3 arguments is an error to follow the pattern of the
	 * standard nslookup */
	if (!argv[1] || argv[1][0] == '-' || argc > 3)
		bb_show_usage();

	/* initialize DNS structure _res used in printing the default
	 * name server and in the explicit name server option feature. */
	res_init();
	/* rfc2133 says this enables IPv6 lookups */
	/* (but it also says "may be enabled in /etc/resolv.conf") */
	/*_res.options |= RES_USE_INET6;*/

	set_default_dns(argv[2]);

	server_print();

	/* getaddrinfo and friends are free to request a resolver
	 * reinitialization. Just in case, set_default_dns() again
	 * after getaddrinfo (in server_print). This reportedly helps
	 * with bug 675 "nslookup does not properly use second argument"
	 * at least on Debian Wheezy and Openwrt AA (eglibc based).
	 */
	set_default_dns(argv[2]);

	return print_host(argv[1], "Name:");
}


#else /****** A version from LEDE / OpenWRT ******/

/*
 * musl compatible nslookup
 *
 * Copyright (C) 2017 Jo-Philipp Wich <jo@mein.io>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if 0
# define dbg(...) fprintf(stderr, __VA_ARGS__)
#else
# define dbg(...) ((void)0)
#endif

struct ns {
	const char *name;
	len_and_sockaddr *lsa;
	int failures;
	int replies;
};

struct query {
	const char *name;
	size_t qlen, rlen;
	unsigned char query[512], reply[512];
	unsigned long latency;
	int rcode;
};

static const struct {
	int type;
	const char *name;
} qtypes[] = {
	{ ns_t_soa,   "SOA"   },
	{ ns_t_ns,    "NS"    },
	{ ns_t_a,     "A"     },
#if ENABLE_FEATURE_IPV6
	{ ns_t_aaaa,  "AAAA"  },
#endif
	{ ns_t_cname, "CNAME" },
	{ ns_t_mx,    "MX"    },
	{ ns_t_txt,   "TXT"   },
	{ ns_t_ptr,   "PTR"   },
	{ ns_t_any,   "ANY"   },
	{ }
};

static const char *const rcodes[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"YXDOMAIN",
	"YXRRSET",
	"NXRRSET",
	"NOTAUTH",
	"NOTZONE",
	"RESERVED11",
	"RESERVED12",
	"RESERVED13",
	"RESERVED14",
	"RESERVED15",
	"BADVERS"
};

#if ENABLE_FEATURE_IPV6
static const char v4_mapped[] = { 0,0,0,0,0,0,0,0,0,0,0xff,0xff };
#endif

struct globals {
	unsigned default_port;
	unsigned default_retry;
	unsigned default_timeout;
	unsigned serv_count;
	struct ns *server;
} FIX_ALIASING;
#define G (*(struct globals*)bb_common_bufsiz1)
#define INIT_G() do { \
	setup_common_bufsiz(); \
	G.default_port = 53; \
	G.default_retry = 2; \
	G.default_timeout = 5; \
} while (0)

static int
parse_reply(const unsigned char *msg, size_t len, int *bb_style_counter)
{
	ns_msg handle;
	ns_rr rr;
	int i, n, rdlen;
	const char *format = NULL;
	char astr[INET6_ADDRSTRLEN], dname[MAXDNAME];
	const unsigned char *cp;

	if (ns_initparse(msg, len, &handle) != 0) {
		//fprintf(stderr, "Unable to parse reply: %s\n", strerror(errno));
		return -1;
	}

	for (i = 0; i < ns_msg_count(handle, ns_s_an); i++) {
		if (ns_parserr(&handle, ns_s_an, i, &rr) != 0) {
			//fprintf(stderr, "Unable to parse resource record: %s\n", strerror(errno));
			return -1;
		}

		if (bb_style_counter && *bb_style_counter == 1)
			printf("Name:      %s\n", ns_rr_name(rr));

		rdlen = ns_rr_rdlen(rr);

		switch (ns_rr_type(rr))
		{
		case ns_t_a:
			if (rdlen != 4) {
				//fprintf(stderr, "Unexpected A record length\n");
				return -1;
			}
			inet_ntop(AF_INET, ns_rr_rdata(rr), astr, sizeof(astr));
			if (bb_style_counter)
				printf("Address %d: %s\n", (*bb_style_counter)++, astr);
			else
				printf("Name:\t%s\nAddress: %s\n", ns_rr_name(rr), astr);
			break;

#if ENABLE_FEATURE_IPV6
		case ns_t_aaaa:
			if (rdlen != 16) {
				//fprintf(stderr, "Unexpected AAAA record length\n");
				return -1;
			}
			inet_ntop(AF_INET6, ns_rr_rdata(rr), astr, sizeof(astr));
			if (bb_style_counter)
				printf("Address %d: %s\n", (*bb_style_counter)++, astr);
			else
				printf("%s\thas AAAA address %s\n", ns_rr_name(rr), astr);
			break;
#endif

		case ns_t_ns:
			if (!format)
				format = "%s\tnameserver = %s\n";
			/* fall through */

		case ns_t_cname:
			if (!format)
				format = "%s\tcanonical name = %s\n";
			/* fall through */

		case ns_t_ptr:
			if (!format)
				format = "%s\tname = %s\n";
			if (ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle),
				ns_rr_rdata(rr), dname, sizeof(dname)) < 0) {
				//fprintf(stderr, "Unable to uncompress domain: %s\n", strerror(errno));
				return -1;
			}
			printf(format, ns_rr_name(rr), dname);
			break;

		case ns_t_mx:
			if (rdlen < 2) {
				fprintf(stderr, "MX record too short\n");
				return -1;
			}
			n = ns_get16(ns_rr_rdata(rr));
			if (ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle),
				ns_rr_rdata(rr) + 2, dname, sizeof(dname)) < 0) {
				//fprintf(stderr, "Cannot uncompress MX domain: %s\n", strerror(errno));
				return -1;
			}
			printf("%s\tmail exchanger = %d %s\n", ns_rr_name(rr), n, dname);
			break;

		case ns_t_txt:
			if (rdlen < 1) {
				//fprintf(stderr, "TXT record too short\n");
				return -1;
			}
			n = *(unsigned char *)ns_rr_rdata(rr);
			if (n > 0) {
				memset(dname, 0, sizeof(dname));
				memcpy(dname, ns_rr_rdata(rr) + 1, n);
				printf("%s\ttext = \"%s\"\n", ns_rr_name(rr), dname);
			}
			break;

		case ns_t_soa:
			if (rdlen < 20) {
				//fprintf(stderr, "SOA record too short\n");
				return -1;
			}

			printf("%s\n", ns_rr_name(rr));

			cp = ns_rr_rdata(rr);
			n = ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle),
			                       cp, dname, sizeof(dname));

			if (n < 0) {
				//fprintf(stderr, "Unable to uncompress domain: %s\n", strerror(errno));
				return -1;
			}

			printf("\torigin = %s\n", dname);
			cp += n;

			n = ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle),
			                       cp, dname, sizeof(dname));

			if (n < 0) {
				//fprintf(stderr, "Unable to uncompress domain: %s\n", strerror(errno));
				return -1;
			}

			printf("\tmail addr = %s\n", dname);
			cp += n;

			printf("\tserial = %lu\n", ns_get32(cp));
			cp += 4;

			printf("\trefresh = %lu\n", ns_get32(cp));
			cp += 4;

			printf("\tretry = %lu\n", ns_get32(cp));
			cp += 4;

			printf("\texpire = %lu\n", ns_get32(cp));
			cp += 4;

			printf("\tminimum = %lu\n", ns_get32(cp));
			break;

		default:
			break;
		}
	}

	return i;
}

static char *make_ptr(char resbuf[80], const char *addrstr)
{
	unsigned char addr[16];
	int i;

#if ENABLE_FEATURE_IPV6
	if (inet_pton(AF_INET6, addrstr, addr)) {
		if (memcmp(addr, v4_mapped, 12) != 0) {
			char *ptr = resbuf;
			for (i = 0; i < 16; i++) {
				*ptr++ = 0x20 | bb_hexdigits_upcase[(unsigned char)addr[15 - i] & 0xf];
				*ptr++ = '.';
				*ptr++ = 0x20 | bb_hexdigits_upcase[(unsigned char)addr[15 - i] >> 4];
				*ptr++ = '.';
			}
			strcpy(ptr, "ip6.arpa");
		}
		else {
			sprintf(resbuf, "%u.%u.%u.%u.in-addr.arpa",
				addr[15], addr[14], addr[13], addr[12]);
		}
		return resbuf;
	}
#endif

	if (inet_pton(AF_INET, addrstr, addr)) {
		sprintf(resbuf, "%u.%u.%u.%u.in-addr.arpa",
		        addr[3], addr[2], addr[1], addr[0]);
		return resbuf;
	}

	return NULL;
}

/*
 * Function logic borrowed & modified from musl libc, res_msend.c
 */
static int send_queries(struct ns *ns, struct query *queries, int n_queries)
{
	struct pollfd pfd;
	int servfail_retry = 0;
	int n_replies = 0;
	int next_query = 0;
	unsigned retry_interval;
	unsigned timeout = G.default_timeout * 1000;
	unsigned t0, t1, t2;

	pfd.fd = -1;
	pfd.events = POLLIN;

	retry_interval = timeout / G.default_retry;
	t0 = t2 = monotonic_ms();
	t1 = t2 - retry_interval;

	for (; t2 - t0 < timeout; t2 = monotonic_ms()) {
		if (t2 - t1 >= retry_interval) {
			int qn;
			for (qn = 0; qn < n_queries; qn++) {
				if (queries[qn].rlen)
					continue;
				if (pfd.fd < 0) {
					len_and_sockaddr *local_lsa;
					pfd.fd = xsocket_type(&local_lsa, ns->lsa->u.sa.sa_family, SOCK_DGRAM);
					/*
					 * local_lsa has "null" address and port 0 now.
					 * bind() ensures we have a *particular port* selected by kernel
					 * and remembered in fd, thus later recv(fd)
					 * receives only packets sent to this port.
					 */
					xbind(pfd.fd, &local_lsa->u.sa, local_lsa->len);
					free(local_lsa);
					/* Make read/writes know the destination */
					xconnect(pfd.fd, &ns->lsa->u.sa, ns->lsa->len);
					ndelay_on(pfd.fd);
				}
				write(pfd.fd, queries[qn].query, queries[qn].qlen);
			}

			t1 = t2;
			servfail_retry = 2 * n_queries;
		}
 poll_more:
		/* Wait for a response, or until time to retry */
		if (poll(&pfd, 1, t1 + retry_interval - t2) <= 0)
			continue;

		while (1) {
			int qn;
			int recvlen;

			recvlen = read(pfd.fd,
					queries[next_query].reply,
					sizeof(queries[next_query].reply)
			);

			/* read error */
			if (recvlen < 0)
				break;

			/* Ignore non-identifiable packets */
			if (recvlen < 4)
				goto poll_more;

			/* Find which query this answer goes with, if any */
			for (qn = next_query; qn < n_queries; qn++)
				if (memcmp(queries[next_query].reply, queries[qn].query, 2) == 0)
					break;

			if (qn >= n_queries || queries[qn].rlen)
				goto poll_more;

			queries[qn].rcode = queries[next_query].reply[3] & 15;
			queries[qn].latency = monotonic_ms() - t0;

			ns->replies++;

			/* Only accept positive or negative responses;
			 * retry immediately on server failure, and ignore
			 * all other codes such as refusal. */
			switch (queries[qn].rcode) {
			case 0:
			case 3:
				break;

			case 2:
				if (servfail_retry && servfail_retry--) {
					ns->failures++;
					write(pfd.fd, queries[qn].query, queries[qn].qlen);
				}
				/* fall through */

			default:
				continue;
			}

			/* Store answer */
			n_replies++;

			queries[qn].rlen = recvlen;

			if (qn == next_query) {
				while (next_query < n_queries) {
					if (!queries[next_query].rlen)
						break;
					next_query++;
				}
			} else {
				memcpy(queries[qn].reply, queries[next_query].reply, recvlen);
			}

			if (next_query >= n_queries)
				goto ret;
		}
	}
 ret:
	if (pfd.fd >= 0)
		close(pfd.fd);

	return n_replies;
}

static void add_ns(const char *addr)
{
	struct ns *ns;
	unsigned count;

	dbg("%s: addr:'%s'\n", __func__, addr);

	count = G.serv_count++;

	G.server = xrealloc_vector(G.server, /*8=2^3:*/ 3, count);
	ns = &G.server[count];
	ns->name = addr;
	ns->lsa = xhost2sockaddr(addr, 53);
	/*ns->replies = 0; - already is */
	/*ns->failures = 0; - already is */
}

static void parse_resolvconf(void)
{
	FILE *resolv;

	resolv = fopen("/etc/resolv.conf", "r");
	if (resolv) {
		char line[128], *p;

		while (fgets(line, sizeof(line), resolv)) {
			p = strtok(line, " \t\n");

			if (!p || strcmp(p, "nameserver") != 0)
				continue;

			p = strtok(NULL, " \t\n");

			if (!p)
				continue;

			add_ns(xstrdup(p));
		}

		fclose(resolv);
	}
}

static struct query *add_query(struct query **queries, int *n_queries,
		int type, const char *dname)
{
	struct query *new_q;
	int pos;
	ssize_t qlen;

	pos = *n_queries;
	*n_queries = pos + 1;
	*queries = new_q = xrealloc(*queries, sizeof(**queries) * (pos + 1));

	dbg("new query#%u type %u for '%s'\n", pos, type, dname);
	new_q += pos;
	memset(new_q, 0, sizeof(*new_q));

	qlen = res_mkquery(QUERY, dname, C_IN, type, NULL, 0, NULL,
			new_q->query, sizeof(new_q->query));

	new_q->qlen = qlen;
	new_q->name = dname;

	return new_q;
}

//FIXME: use xmalloc_sockaddr2dotted[_noport]() instead of sal2str()

#define SIZEOF_SAL2STR_BUF (INET6_ADDRSTRLEN + 1 + IFNAMSIZ + 1 + 5 + 1)
static char *sal2str(char buf[SIZEOF_SAL2STR_BUF], len_and_sockaddr *a)
{
	char *p = buf;

#if ENABLE_FEATURE_IPV6
	if (a->u.sa.sa_family == AF_INET6) {
		inet_ntop(AF_INET6, &a->u.sin6.sin6_addr, buf, SIZEOF_SAL2STR_BUF);
		p += strlen(p);

		if (a->u.sin6.sin6_scope_id) {
			if (if_indextoname(a->u.sin6.sin6_scope_id, p + 1)) {
				*p++ = '%';
				p += strlen(p);
			}
		}
	} else
#endif
	{
		inet_ntop(AF_INET, &a->u.sin.sin_addr, buf, SIZEOF_SAL2STR_BUF);
		p += strlen(p);
	}

	sprintf(p, "#%hu", ntohs(a->u.sin.sin_port));

	return buf;
}

int nslookup_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nslookup_main(int argc UNUSED_PARAM, char **argv)
{
	struct ns *ns;
	struct query *queries;
	llist_t *type_strings;
	int n_queries;
	int bb_style_counter = 0;
	unsigned types;
	int rc;
	int opts;
	enum {
		OPT_stats = (1 << 4),
	};

	INIT_G();

	type_strings = NULL;
#if ENABLE_FEATURE_NSLOOKUP_LONG_OPTIONS
	opts = getopt32long(argv, "^"
			"+" /* '+': stop at first non-option (why?) */
			"q:*p:+r:+t:+s"
			"\0"
			"-1:q::", /* minimum 1 arg, -q is a list */
				"type\0"      Required_argument "q"
				"querytype\0" Required_argument "q"
				"port\0"      Required_argument "p"
				"retry\0"     Required_argument "r"
				"timeout\0"   Required_argument "t"
				"stats\0"     No_argument       "s",
	                &type_strings, &G.default_port,
	                &G.default_retry, &G.default_timeout
	);
#else
	opts = getopt32(argv, "^"
			"+" /* '+': stop at first non-option (why?) */
			"q:*p:+r:+t:+s"
			"\0"
			"-1:q::", /* minimum 1 arg, -q is a list */
	                &type_strings, &G.default_port,
	                &G.default_retry, &G.default_timeout
	);
#endif
	if (G.default_port > 65535)
		bb_error_msg_and_die("invalid server port");
	if (G.default_retry == 0)
		bb_error_msg_and_die("invalid retry value");
	if (G.default_timeout == 0)
		bb_error_msg_and_die("invalid timeout value");

	types = 0;
	while (type_strings) {
		int c;
		char *ptr, *chr;

		ptr = llist_pop(&type_strings);

		/* skip leading text, e.g. when invoked with -querytype=AAAA */
		chr = strchr(ptr, '=');
		if (chr)
			ptr = chr + 1;

		for (c = 0;; c++) {
			if (!qtypes[c].name)
				bb_error_msg_and_die("invalid query type \"%s\"", ptr);
			if (strcmp(qtypes[c].name, ptr) == 0)
				break;
		}

		types |= (1 << c);
	}

	argv += optind;

	n_queries = 0;
	queries = NULL;
	do {
		/* No explicit type given, guess query type.
		 * If we can convert the domain argument into a ptr (means that
		 * inet_pton() could read it) we assume a PTR request, else
		 * we issue A+AAAA queries and switch to an output format
		 * mimicking the one of the traditional nslookup applet. */
		if (types == 0) {
			char *ptr;
			char buf80[80];

			ptr = make_ptr(buf80, *argv);

			if (ptr) {
				add_query(&queries, &n_queries, T_PTR, ptr);
			}
			else {
				bb_style_counter = 1;
				add_query(&queries, &n_queries, T_A, *argv);
#if ENABLE_FEATURE_IPV6
				add_query(&queries, &n_queries, T_AAAA, *argv);
#endif
			}
		}
		else {
			int c;
			for (c = 0; qtypes[c].name; c++) {
				if (types & (1 << c))
					add_query(&queries, &n_queries, qtypes[c].type,
						*argv);
			}
		}
		argv++;
	} while (argv[0] && argv[1]);

	/* Use given DNS server if present */
	if (argv[0]) {
		add_ns(argv[0]);
	} else {
		parse_resolvconf();
		/* Fall back to localhost if we could not find NS in resolv.conf */
		if (G.serv_count == 0)
			add_ns("127.0.0.1");
	}

	for (rc = 0; rc < G.serv_count; rc++) {
		int c = send_queries(&G.server[rc], queries, n_queries);
		if (c < 0)
			bb_perror_msg_and_die("can't send queries");
		if (c > 0)
			break;
		rc++;
		if (rc >= G.serv_count) {
			fprintf(stderr,
				";; connection timed out; no servers could be reached\n\n");
			return EXIT_FAILURE;
		}
	}

	printf("Server:\t\t%s\n", G.server[rc].name);
	{
		char buf[SIZEOF_SAL2STR_BUF];
		printf("Address:\t%s\n", sal2str(buf, G.server[rc].lsa));
	}
	if (opts & OPT_stats) {
		printf("Replies:\t%d\n", G.server[rc].replies);
		printf("Failures:\t%d\n", G.server[rc].failures);
	}
	printf("\n");

	for (rc = 0; rc < n_queries; rc++) {
		int c;

		if (opts & OPT_stats) {
			printf("Query #%d completed in %lums:\n", rc, queries[rc].latency);
		}

		if (queries[rc].rcode != 0) {
			printf("** server can't find %s: %s\n", queries[rc].name,
			       rcodes[queries[rc].rcode]);
			continue;
		}

		c = 0;

		if (queries[rc].rlen) {
			HEADER *header;

			if (!bb_style_counter) {
				header = (HEADER *)queries[rc].reply;
				if (!header->aa)
					printf("Non-authoritative answer:\n");
				c = parse_reply(queries[rc].reply, queries[rc].rlen, NULL);
			}
			else {
				c = parse_reply(queries[rc].reply, queries[rc].rlen,
					&bb_style_counter);
			}
		}

		if (c == 0)
			printf("*** Can't find %s: No answer\n", queries[rc].name);
		else if (c < 0)
			printf("*** Can't find %s: Parse error\n", queries[rc].name);

		if (!bb_style_counter)
			printf("\n");
	}

	if (ENABLE_FEATURE_CLEAN_UP) {
		free(ns);
		if (n_queries)
			free(queries);
	}

	return EXIT_SUCCESS;
}

#endif
