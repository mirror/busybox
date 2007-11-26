/* vi: set sw=4 ts=4: */
/*
 * iprule.c		"ip rule".
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
 * initially integrated into busybox by Bernhard Fischer
 */

#include <syslog.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "ip_common.h"  /* #include "libbb.h" is inside */
#include "rt_names.h"
#include "utils.h"

/*
static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip rule [ list | add | del ] SELECTOR ACTION\n");
	fprintf(stderr, "SELECTOR := [ from PREFIX ] [ to PREFIX ] [ tos TOS ] [ fwmark FWMARK ]\n");
	fprintf(stderr, "            [ dev STRING ] [ pref NUMBER ]\n");
	fprintf(stderr, "ACTION := [ table TABLE_ID ] [ nat ADDRESS ]\n");
	fprintf(stderr, "          [ prohibit | reject | unreachable ]\n");
	fprintf(stderr, "          [ realms [SRCREALM/]DSTREALM ]\n");
	fprintf(stderr, "TABLE_ID := [ local | main | default | NUMBER ]\n");
	exit(-1);
}
*/

static int print_rule(struct sockaddr_nl *who ATTRIBUTE_UNUSED,
					struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct rtmsg *r = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	int host_len = -1;
	struct rtattr * tb[RTA_MAX+1];
	char abuf[256];
	SPRINT_BUF(b1);

	if (n->nlmsg_type != RTM_NEWRULE)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*r));
	if (len < 0)
		return -1;

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, RTA_MAX, RTM_RTA(r), len);

	if (r->rtm_family == AF_INET)
		host_len = 32;
	else if (r->rtm_family == AF_INET6)
		host_len = 128;
/*	else if (r->rtm_family == AF_DECnet)
		host_len = 16;
	else if (r->rtm_family == AF_IPX)
		host_len = 80;
*/
	if (tb[RTA_PRIORITY])
		fprintf(fp, "%u:\t", *(unsigned*)RTA_DATA(tb[RTA_PRIORITY]));
	else
		fprintf(fp, "0:\t");

	fprintf(fp, "from ");
	if (tb[RTA_SRC]) {
		if (r->rtm_src_len != host_len) {
			fprintf(fp, "%s/%u", rt_addr_n2a(r->rtm_family,
							 RTA_PAYLOAD(tb[RTA_SRC]),
							 RTA_DATA(tb[RTA_SRC]),
							 abuf, sizeof(abuf)),
				r->rtm_src_len
				);
		} else {
			fputs(format_host(r->rtm_family,
						       RTA_PAYLOAD(tb[RTA_SRC]),
						       RTA_DATA(tb[RTA_SRC]),
						       abuf, sizeof(abuf)), fp);
		}
	} else if (r->rtm_src_len) {
		fprintf(fp, "0/%d", r->rtm_src_len);
	} else {
		fprintf(fp, "all");
	}
	fprintf(fp, " ");

	if (tb[RTA_DST]) {
		if (r->rtm_dst_len != host_len) {
			fprintf(fp, "to %s/%u ", rt_addr_n2a(r->rtm_family,
							 RTA_PAYLOAD(tb[RTA_DST]),
							 RTA_DATA(tb[RTA_DST]),
							 abuf, sizeof(abuf)),
				r->rtm_dst_len
				);
		} else {
			fprintf(fp, "to %s ", format_host(r->rtm_family,
						       RTA_PAYLOAD(tb[RTA_DST]),
						       RTA_DATA(tb[RTA_DST]),
						       abuf, sizeof(abuf)));
		}
	} else if (r->rtm_dst_len) {
		fprintf(fp, "to 0/%d ", r->rtm_dst_len);
	}

	if (r->rtm_tos) {
		fprintf(fp, "tos %s ", rtnl_dsfield_n2a(r->rtm_tos, b1, sizeof(b1)));
	}
	if (tb[RTA_PROTOINFO]) {
		fprintf(fp, "fwmark %#x ", *(uint32_t*)RTA_DATA(tb[RTA_PROTOINFO]));
	}

	if (tb[RTA_IIF]) {
		fprintf(fp, "iif %s ", (char*)RTA_DATA(tb[RTA_IIF]));
	}

	if (r->rtm_table)
		fprintf(fp, "lookup %s ", rtnl_rttable_n2a(r->rtm_table, b1, sizeof(b1)));

	if (tb[RTA_FLOW]) {
		uint32_t to = *(uint32_t*)RTA_DATA(tb[RTA_FLOW]);
		uint32_t from = to>>16;
		to &= 0xFFFF;
		if (from) {
			fprintf(fp, "realms %s/",
				rtnl_rtrealm_n2a(from, b1, sizeof(b1)));
		}
		fprintf(fp, "%s ",
			rtnl_rtrealm_n2a(to, b1, sizeof(b1)));
	}

	if (r->rtm_type == RTN_NAT) {
		if (tb[RTA_GATEWAY]) {
			fprintf(fp, "map-to %s ",
				format_host(r->rtm_family,
					    RTA_PAYLOAD(tb[RTA_GATEWAY]),
					    RTA_DATA(tb[RTA_GATEWAY]),
					    abuf, sizeof(abuf)));
		} else
			fprintf(fp, "masquerade");
	} else if (r->rtm_type != RTN_UNICAST)
		fputs(rtnl_rtntype_n2a(r->rtm_type, b1, sizeof(b1)), fp);

	fputc('\n', fp);
	fflush(fp);
	return 0;
}

/* Return value becomes exitcode. It's okay to not return at all */
static int iprule_list(char **argv)
{
	struct rtnl_handle rth;
	int af = preferred_family;

	if (af == AF_UNSPEC)
		af = AF_INET;

	if (*argv) {
		//bb_error_msg("\"rule show\" needs no arguments");
		bb_warn_ignoring_args(1);
		return -1;
	}

	xrtnl_open(&rth);

	xrtnl_wilddump_request(&rth, af, RTM_GETRULE);
	xrtnl_dump_filter(&rth, print_rule, stdout);

	return 0;
}

/* Return value becomes exitcode. It's okay to not return at all */
static int iprule_modify(int cmd, char **argv)
{
	static const char keywords[] ALIGN1 =
		"from\0""to\0""preference\0""order\0""priority\0"
		"tos\0""fwmark\0""realms\0""table\0""lookup\0""dev\0"
		"iif\0""nat\0""map-to\0""type\0""help\0";
	enum {
		ARG_from = 1, ARG_to, ARG_preference, ARG_order, ARG_priority,
		ARG_tos, ARG_fwmark, ARG_realms, ARG_table, ARG_lookup, ARG_dev,
		ARG_iif, ARG_nat, ARG_map_to, ARG_type, ARG_help
	};
	bool table_ok = 0;
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr	n;
		struct rtmsg	r;
		char		buf[1024];
	} req;
	smalluint key;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_type = cmd;
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.r.rtm_family = preferred_family;
	req.r.rtm_protocol = RTPROT_BOOT;
	req.r.rtm_scope = RT_SCOPE_UNIVERSE;
	req.r.rtm_table = 0;
	req.r.rtm_type = RTN_UNSPEC;

	if (cmd == RTM_NEWRULE) {
		req.n.nlmsg_flags |= NLM_F_CREATE|NLM_F_EXCL;
		req.r.rtm_type = RTN_UNICAST;
	}

	while (*argv) {
		key = index_in_substrings(keywords, *argv) + 1;
		if (key == 0) /* no match found in keywords array, bail out. */
			bb_error_msg_and_die(bb_msg_invalid_arg, *argv, applet_name);
		if (key == ARG_from) {
			inet_prefix dst;
			NEXT_ARG();
			get_prefix(&dst, *argv, req.r.rtm_family);
			req.r.rtm_src_len = dst.bitlen;
			addattr_l(&req.n, sizeof(req), RTA_SRC, &dst.data, dst.bytelen);
		} else if (key == ARG_to) {
			inet_prefix dst;
			NEXT_ARG();
			get_prefix(&dst, *argv, req.r.rtm_family);
			req.r.rtm_dst_len = dst.bitlen;
			addattr_l(&req.n, sizeof(req), RTA_DST, &dst.data, dst.bytelen);
		} else if (key == ARG_preference ||
			   key == ARG_order ||
			   key == ARG_priority) {
			uint32_t pref;
			NEXT_ARG();
			if (get_u32(&pref, *argv, 0))
				invarg(*argv, "preference");
			addattr32(&req.n, sizeof(req), RTA_PRIORITY, pref);
		} else if (key == ARG_tos) {
			uint32_t tos;
			NEXT_ARG();
			if (rtnl_dsfield_a2n(&tos, *argv))
				invarg(*argv, "TOS");
			req.r.rtm_tos = tos;
		} else if (key == ARG_fwmark) {
			uint32_t fwmark;
			NEXT_ARG();
			if (get_u32(&fwmark, *argv, 0))
				invarg(*argv, "fwmark");
			addattr32(&req.n, sizeof(req), RTA_PROTOINFO, fwmark);
		} else if (key == ARG_realms) {
			uint32_t realm;
			NEXT_ARG();
			if (get_rt_realms(&realm, *argv))
				invarg(*argv, "realms");
			addattr32(&req.n, sizeof(req), RTA_FLOW, realm);
		} else if (key == ARG_table ||
			   key == ARG_lookup) {
			uint32_t tid;
			NEXT_ARG();
			if (rtnl_rttable_a2n(&tid, *argv))
				invarg(*argv, "table ID");
			req.r.rtm_table = tid;
			table_ok = 1;
		} else if (key == ARG_dev ||
			   key == ARG_iif) {
			NEXT_ARG();
			addattr_l(&req.n, sizeof(req), RTA_IIF, *argv, strlen(*argv)+1);
		} else if (key == ARG_nat ||
			   key == ARG_map_to) {
			NEXT_ARG();
			addattr32(&req.n, sizeof(req), RTA_GATEWAY, get_addr32(*argv));
			req.r.rtm_type = RTN_NAT;
		} else {
			int type;

			if (key == ARG_type) {
				NEXT_ARG();
			}
			if (key == ARG_help)
				bb_show_usage();
			if (rtnl_rtntype_a2n(&type, *argv))
				invarg(*argv, "type");
			req.r.rtm_type = type;
		}
		argv++;
	}

	if (req.r.rtm_family == AF_UNSPEC)
		req.r.rtm_family = AF_INET;

	if (!table_ok && cmd == RTM_NEWRULE)
		req.r.rtm_table = RT_TABLE_MAIN;

	xrtnl_open(&rth);

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		return 2;

	return 0;
}

/* Return value becomes exitcode. It's okay to not return at all */
int do_iprule(char **argv)
{
	static const char ip_rule_commands[] ALIGN1 =
		"add\0""delete\0""list\0""show\0";
	int cmd = 2; /* list */

	if (!*argv)
		return iprule_list(argv);

	cmd = index_in_substrings(ip_rule_commands, *argv);
	switch (cmd) {
		case 0: /* add */
			cmd = RTM_NEWRULE;
			break;
		case 1: /* delete */
			cmd = RTM_DELRULE;
			break;
		case 2: /* list */
		case 3: /* show */
			return iprule_list(argv+1);
			break;
		default:
			bb_error_msg_and_die("unknown command %s", *argv);
	}
	return iprule_modify(cmd, argv+1);
}
