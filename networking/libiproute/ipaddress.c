/* vi: set sw=4 ts=4: */
/*
 * ipaddress.c		"ip address".
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Changes:
 *	Laszlo Valko <valko@linux.karinthy.hu> 990223: address label must be zero terminated
 */

//#include <sys/socket.h>
//#include <sys/ioctl.h>
#include <fnmatch.h>
#include <net/if.h>
#include <net/if_arp.h>

#include "ip_common.h"  /* #include "libbb.h" is inside */
#include "rt_names.h"
#include "utils.h"


typedef struct filter_t {
	int ifindex;
	int family;
	int oneline;
	int showqueue;
	inet_prefix pfx;
	int scope, scopemask;
	int flags, flagmask;
	int up;
	char *label;
	int flushed;
	char *flushb;
	int flushp;
	int flushe;
	struct rtnl_handle *rth;
} filter_t;

#define filter (*(filter_t*)&bb_common_bufsiz1)


static void print_link_flags(FILE *fp, unsigned flags, unsigned mdown)
{
	fprintf(fp, "<");
	flags &= ~IFF_RUNNING;
#define _PF(f) if (flags&IFF_##f) { \
		  flags &= ~IFF_##f; \
		  fprintf(fp, #f "%s", flags ? "," : ""); }
	_PF(LOOPBACK);
	_PF(BROADCAST);
	_PF(POINTOPOINT);
	_PF(MULTICAST);
	_PF(NOARP);
#if 0
	_PF(ALLMULTI);
	_PF(PROMISC);
	_PF(MASTER);
	_PF(SLAVE);
	_PF(DEBUG);
	_PF(DYNAMIC);
	_PF(AUTOMEDIA);
	_PF(PORTSEL);
	_PF(NOTRAILERS);
#endif
	_PF(UP);
#undef _PF
	if (flags)
		fprintf(fp, "%x", flags);
	if (mdown)
		fprintf(fp, ",M-DOWN");
	fprintf(fp, "> ");
}

static void print_queuelen(char *name)
{
	struct ifreq ifr;
	int s;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl_or_warn(s, SIOCGIFTXQLEN, &ifr) < 0) {
		close(s);
		return;
	}
	close(s);

	if (ifr.ifr_qlen)
		printf("qlen %d", ifr.ifr_qlen);
}

static int print_linkinfo(struct sockaddr_nl ATTRIBUTE_UNUSED *who,
		const struct nlmsghdr *n, void ATTRIBUTE_UNUSED *arg)
{
	FILE *fp = (FILE*)arg;
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr * tb[IFLA_MAX+1];
	int len = n->nlmsg_len;
	unsigned m_flag = 0;

	if (n->nlmsg_type != RTM_NEWLINK && n->nlmsg_type != RTM_DELLINK)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return -1;

	if (filter.ifindex && ifi->ifi_index != filter.ifindex)
		return 0;
	if (filter.up && !(ifi->ifi_flags&IFF_UP))
		return 0;

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (tb[IFLA_IFNAME] == NULL) {
		bb_error_msg("nil ifname");
		return -1;
	}
	if (filter.label
	 && (!filter.family || filter.family == AF_PACKET)
	 && fnmatch(filter.label, RTA_DATA(tb[IFLA_IFNAME]), 0)
	) {
		return 0;
	}

	if (n->nlmsg_type == RTM_DELLINK)
		fprintf(fp, "Deleted ");

	fprintf(fp, "%d: %s", ifi->ifi_index,
		tb[IFLA_IFNAME] ? (char*)RTA_DATA(tb[IFLA_IFNAME]) : "<nil>");

	if (tb[IFLA_LINK]) {
		SPRINT_BUF(b1);
		int iflink = *(int*)RTA_DATA(tb[IFLA_LINK]);
		if (iflink == 0)
			fprintf(fp, "@NONE: ");
		else {
			fprintf(fp, "@%s: ", ll_idx_n2a(iflink, b1));
			m_flag = ll_index_to_flags(iflink);
			m_flag = !(m_flag & IFF_UP);
		}
	} else {
		fprintf(fp, ": ");
	}
	print_link_flags(fp, ifi->ifi_flags, m_flag);

	if (tb[IFLA_MTU])
		fprintf(fp, "mtu %u ", *(int*)RTA_DATA(tb[IFLA_MTU]));
	if (tb[IFLA_QDISC])
		fprintf(fp, "qdisc %s ", (char*)RTA_DATA(tb[IFLA_QDISC]));
#ifdef IFLA_MASTER
	if (tb[IFLA_MASTER]) {
		SPRINT_BUF(b1);
		fprintf(fp, "master %s ", ll_idx_n2a(*(int*)RTA_DATA(tb[IFLA_MASTER]), b1));
	}
#endif
	if (filter.showqueue)
		print_queuelen((char*)RTA_DATA(tb[IFLA_IFNAME]));

	if (!filter.family || filter.family == AF_PACKET) {
		SPRINT_BUF(b1);
		fprintf(fp, "%c    link/%s ", _SL_, ll_type_n2a(ifi->ifi_type, b1, sizeof(b1)));

		if (tb[IFLA_ADDRESS]) {
			fputs(ll_addr_n2a(RTA_DATA(tb[IFLA_ADDRESS]),
						      RTA_PAYLOAD(tb[IFLA_ADDRESS]),
						      ifi->ifi_type,
						      b1, sizeof(b1)), fp);
		}
		if (tb[IFLA_BROADCAST]) {
			if (ifi->ifi_flags & IFF_POINTOPOINT)
				fprintf(fp, " peer ");
			else
				fprintf(fp, " brd ");
			fputs(ll_addr_n2a(RTA_DATA(tb[IFLA_BROADCAST]),
						      RTA_PAYLOAD(tb[IFLA_BROADCAST]),
						      ifi->ifi_type,
						      b1, sizeof(b1)), fp);
		}
	}
	fputc('\n', fp);
	fflush(fp);
	return 0;
}

static int flush_update(void)
{
	if (rtnl_send(filter.rth, filter.flushb, filter.flushp) < 0) {
		bb_perror_msg("failed to send flush request");
		return -1;
	}
	filter.flushp = 0;
	return 0;
}

static int print_addrinfo(struct sockaddr_nl ATTRIBUTE_UNUSED *who,
		struct nlmsghdr *n, void ATTRIBUTE_UNUSED *arg)
{
	FILE *fp = (FILE*)arg;
	struct ifaddrmsg *ifa = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * rta_tb[IFA_MAX+1];
	char abuf[256];
	SPRINT_BUF(b1);

	if (n->nlmsg_type != RTM_NEWADDR && n->nlmsg_type != RTM_DELADDR)
		return 0;
	len -= NLMSG_LENGTH(sizeof(*ifa));
	if (len < 0) {
		bb_error_msg("wrong nlmsg len %d", len);
		return -1;
	}

	if (filter.flushb && n->nlmsg_type != RTM_NEWADDR)
		return 0;

	memset(rta_tb, 0, sizeof(rta_tb));
	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)));

	if (!rta_tb[IFA_LOCAL])
		rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
	if (!rta_tb[IFA_ADDRESS])
		rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];

	if (filter.ifindex && filter.ifindex != ifa->ifa_index)
		return 0;
	if ((filter.scope^ifa->ifa_scope)&filter.scopemask)
		return 0;
	if ((filter.flags^ifa->ifa_flags)&filter.flagmask)
		return 0;
	if (filter.label) {
		const char *label;
		if (rta_tb[IFA_LABEL])
			label = RTA_DATA(rta_tb[IFA_LABEL]);
		else
			label = ll_idx_n2a(ifa->ifa_index, b1);
		if (fnmatch(filter.label, label, 0) != 0)
			return 0;
	}
	if (filter.pfx.family) {
		if (rta_tb[IFA_LOCAL]) {
			inet_prefix dst;
			memset(&dst, 0, sizeof(dst));
			dst.family = ifa->ifa_family;
			memcpy(&dst.data, RTA_DATA(rta_tb[IFA_LOCAL]), RTA_PAYLOAD(rta_tb[IFA_LOCAL]));
			if (inet_addr_match(&dst, &filter.pfx, filter.pfx.bitlen))
				return 0;
		}
	}

	if (filter.flushb) {
		struct nlmsghdr *fn;
		if (NLMSG_ALIGN(filter.flushp) + n->nlmsg_len > filter.flushe) {
			if (flush_update())
				return -1;
		}
		fn = (struct nlmsghdr*)(filter.flushb + NLMSG_ALIGN(filter.flushp));
		memcpy(fn, n, n->nlmsg_len);
		fn->nlmsg_type = RTM_DELADDR;
		fn->nlmsg_flags = NLM_F_REQUEST;
		fn->nlmsg_seq = ++filter.rth->seq;
		filter.flushp = (((char*)fn) + n->nlmsg_len) - filter.flushb;
		filter.flushed++;
		return 0;
	}

	if (n->nlmsg_type == RTM_DELADDR)
		fprintf(fp, "Deleted ");

	if (filter.oneline)
		fprintf(fp, "%u: %s", ifa->ifa_index, ll_index_to_name(ifa->ifa_index));
	if (ifa->ifa_family == AF_INET)
		fprintf(fp, "    inet ");
	else if (ifa->ifa_family == AF_INET6)
		fprintf(fp, "    inet6 ");
	else
		fprintf(fp, "    family %d ", ifa->ifa_family);

	if (rta_tb[IFA_LOCAL]) {
		fputs(rt_addr_n2a(ifa->ifa_family,
					      RTA_PAYLOAD(rta_tb[IFA_LOCAL]),
					      RTA_DATA(rta_tb[IFA_LOCAL]),
					      abuf, sizeof(abuf)), fp);

		if (rta_tb[IFA_ADDRESS] == NULL ||
		    memcmp(RTA_DATA(rta_tb[IFA_ADDRESS]), RTA_DATA(rta_tb[IFA_LOCAL]), 4) == 0) {
			fprintf(fp, "/%d ", ifa->ifa_prefixlen);
		} else {
			fprintf(fp, " peer %s/%d ",
				rt_addr_n2a(ifa->ifa_family,
					    RTA_PAYLOAD(rta_tb[IFA_ADDRESS]),
					    RTA_DATA(rta_tb[IFA_ADDRESS]),
					    abuf, sizeof(abuf)),
				ifa->ifa_prefixlen);
		}
	}

	if (rta_tb[IFA_BROADCAST]) {
		fprintf(fp, "brd %s ",
			rt_addr_n2a(ifa->ifa_family,
				    RTA_PAYLOAD(rta_tb[IFA_BROADCAST]),
				    RTA_DATA(rta_tb[IFA_BROADCAST]),
				    abuf, sizeof(abuf)));
	}
	if (rta_tb[IFA_ANYCAST]) {
		fprintf(fp, "any %s ",
			rt_addr_n2a(ifa->ifa_family,
				    RTA_PAYLOAD(rta_tb[IFA_ANYCAST]),
				    RTA_DATA(rta_tb[IFA_ANYCAST]),
				    abuf, sizeof(abuf)));
	}
	fprintf(fp, "scope %s ", rtnl_rtscope_n2a(ifa->ifa_scope, b1, sizeof(b1)));
	if (ifa->ifa_flags&IFA_F_SECONDARY) {
		ifa->ifa_flags &= ~IFA_F_SECONDARY;
		fprintf(fp, "secondary ");
	}
	if (ifa->ifa_flags&IFA_F_TENTATIVE) {
		ifa->ifa_flags &= ~IFA_F_TENTATIVE;
		fprintf(fp, "tentative ");
	}
	if (ifa->ifa_flags&IFA_F_DEPRECATED) {
		ifa->ifa_flags &= ~IFA_F_DEPRECATED;
		fprintf(fp, "deprecated ");
	}
	if (!(ifa->ifa_flags&IFA_F_PERMANENT)) {
		fprintf(fp, "dynamic ");
	} else
		ifa->ifa_flags &= ~IFA_F_PERMANENT;
	if (ifa->ifa_flags)
		fprintf(fp, "flags %02x ", ifa->ifa_flags);
	if (rta_tb[IFA_LABEL])
		fputs((char*)RTA_DATA(rta_tb[IFA_LABEL]), fp);
	if (rta_tb[IFA_CACHEINFO]) {
		struct ifa_cacheinfo *ci = RTA_DATA(rta_tb[IFA_CACHEINFO]);
		char buf[128];
		fputc(_SL_, fp);
		if (ci->ifa_valid == 0xFFFFFFFFU)
			sprintf(buf, "valid_lft forever");
		else
			sprintf(buf, "valid_lft %dsec", ci->ifa_valid);
		if (ci->ifa_prefered == 0xFFFFFFFFU)
			sprintf(buf+strlen(buf), " preferred_lft forever");
		else
			sprintf(buf+strlen(buf), " preferred_lft %dsec", ci->ifa_prefered);
		fprintf(fp, "       %s", buf);
	}
	fputc('\n', fp);
	fflush(fp);
	return 0;
}


struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr	  h;
};

static int print_selected_addrinfo(int ifindex, struct nlmsg_list *ainfo, FILE *fp)
{
	for (; ainfo; ainfo = ainfo->next) {
		struct nlmsghdr *n = &ainfo->h;
		struct ifaddrmsg *ifa = NLMSG_DATA(n);

		if (n->nlmsg_type != RTM_NEWADDR)
			continue;

		if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifa)))
			return -1;

		if (ifa->ifa_index != ifindex ||
		    (filter.family && filter.family != ifa->ifa_family))
			continue;

		print_addrinfo(NULL, n, fp);
	}
	return 0;
}


static int store_nlmsg(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct nlmsg_list **linfo = (struct nlmsg_list**)arg;
	struct nlmsg_list *h;
	struct nlmsg_list **lp;

	h = malloc(n->nlmsg_len+sizeof(void*));
	if (h == NULL)
		return -1;

	memcpy(&h->h, n, n->nlmsg_len);
	h->next = NULL;

	for (lp = linfo; *lp; lp = &(*lp)->next) /* NOTHING */;
	*lp = h;

	ll_remember_index(who, n, NULL);
	return 0;
}

static void ipaddr_reset_filter(int _oneline)
{
	memset(&filter, 0, sizeof(filter));
	filter.oneline = _oneline;
}

/* Return value becomes exitcode. It's okay to not return at all */
int ipaddr_list_or_flush(char **argv, int flush)
{
	static const char option[] ALIGN1 = "to\0""scope\0""up\0""label\0""dev\0";

	struct nlmsg_list *linfo = NULL;
	struct nlmsg_list *ainfo = NULL;
	struct nlmsg_list *l;
	struct rtnl_handle rth;
	char *filter_dev = NULL;
	int no_link = 0;

	ipaddr_reset_filter(oneline);
	filter.showqueue = 1;

	if (filter.family == AF_UNSPEC)
		filter.family = preferred_family;

	if (flush) {
		if (!*argv) {
			bb_error_msg_and_die(bb_msg_requires_arg, "flush");
		}
		if (filter.family == AF_PACKET) {
			bb_error_msg_and_die("cannot flush link addresses");
		}
	}

	while (*argv) {
		const int option_num = index_in_strings(option, *argv);
		switch (option_num) {
			case 0: /* to */
				NEXT_ARG();
				get_prefix(&filter.pfx, *argv, filter.family);
				if (filter.family == AF_UNSPEC) {
					filter.family = filter.pfx.family;
				}
				break;
			case 1: /* scope */
			{
				uint32_t scope = 0;
				NEXT_ARG();
				filter.scopemask = -1;
				if (rtnl_rtscope_a2n(&scope, *argv)) {
					if (strcmp(*argv, "all") != 0) {
						invarg(*argv, "scope");
					}
					scope = RT_SCOPE_NOWHERE;
					filter.scopemask = 0;
				}
				filter.scope = scope;
				break;
			}
			case 2: /* up */
				filter.up = 1;
				break;
			case 3: /* label */
				NEXT_ARG();
				filter.label = *argv;
				break;
			case 4: /* dev */
				NEXT_ARG();
			default:
				if (filter_dev) {
					duparg2("dev", *argv);
				}
				filter_dev = *argv;
		}
		argv++;
	}

	xrtnl_open(&rth);

	xrtnl_wilddump_request(&rth, preferred_family, RTM_GETLINK);
	xrtnl_dump_filter(&rth, store_nlmsg, &linfo);

	if (filter_dev) {
		filter.ifindex = xll_name_to_index(filter_dev);
	}

	if (flush) {
		char flushb[4096-512];

		filter.flushb = flushb;
		filter.flushp = 0;
		filter.flushe = sizeof(flushb);
		filter.rth = &rth;

		for (;;) {
			xrtnl_wilddump_request(&rth, filter.family, RTM_GETADDR);
			filter.flushed = 0;
			xrtnl_dump_filter(&rth, print_addrinfo, stdout);
			if (filter.flushed == 0) {
				return 0;
			}
			if (flush_update() < 0)
				return 1;
		}
	}

	if (filter.family != AF_PACKET) {
		xrtnl_wilddump_request(&rth, filter.family, RTM_GETADDR);
		xrtnl_dump_filter(&rth, store_nlmsg, &ainfo);
	}


	if (filter.family && filter.family != AF_PACKET) {
		struct nlmsg_list **lp;
		lp = &linfo;

		if (filter.oneline)
			no_link = 1;

		while ((l = *lp) != NULL) {
			int ok = 0;
			struct ifinfomsg *ifi = NLMSG_DATA(&l->h);
			struct nlmsg_list *a;

			for (a = ainfo; a; a = a->next) {
				struct nlmsghdr *n = &a->h;
				struct ifaddrmsg *ifa = NLMSG_DATA(n);

				if (ifa->ifa_index != ifi->ifi_index ||
				    (filter.family && filter.family != ifa->ifa_family))
					continue;
				if ((filter.scope ^ ifa->ifa_scope) & filter.scopemask)
					continue;
				if ((filter.flags ^ ifa->ifa_flags) & filter.flagmask)
					continue;
				if (filter.pfx.family || filter.label) {
					struct rtattr *tb[IFA_MAX+1];
					memset(tb, 0, sizeof(tb));
					parse_rtattr(tb, IFA_MAX, IFA_RTA(ifa), IFA_PAYLOAD(n));
					if (!tb[IFA_LOCAL])
						tb[IFA_LOCAL] = tb[IFA_ADDRESS];

					if (filter.pfx.family && tb[IFA_LOCAL]) {
						inet_prefix dst;
						memset(&dst, 0, sizeof(dst));
						dst.family = ifa->ifa_family;
						memcpy(&dst.data, RTA_DATA(tb[IFA_LOCAL]), RTA_PAYLOAD(tb[IFA_LOCAL]));
						if (inet_addr_match(&dst, &filter.pfx, filter.pfx.bitlen))
							continue;
					}
					if (filter.label) {
						SPRINT_BUF(b1);
						const char *label;
						if (tb[IFA_LABEL])
							label = RTA_DATA(tb[IFA_LABEL]);
						else
							label = ll_idx_n2a(ifa->ifa_index, b1);
						if (fnmatch(filter.label, label, 0) != 0)
							continue;
					}
				}

				ok = 1;
				break;
			}
			if (!ok)
				*lp = l->next;
			else
				lp = &l->next;
		}
	}

	for (l = linfo; l; l = l->next) {
		if (no_link || print_linkinfo(NULL, &l->h, stdout) == 0) {
			struct ifinfomsg *ifi = NLMSG_DATA(&l->h);
			if (filter.family != AF_PACKET)
				print_selected_addrinfo(ifi->ifi_index, ainfo, stdout);
		}
	}

	return 0;
}

static int default_scope(inet_prefix *lcl)
{
	if (lcl->family == AF_INET) {
		if (lcl->bytelen >= 1 && *(uint8_t*)&lcl->data == 127)
			return RT_SCOPE_HOST;
	}
	return 0;
}

/* Return value becomes exitcode. It's okay to not return at all */
static int ipaddr_modify(int cmd, char **argv)
{
	static const char option[] ALIGN1 =
		"peer\0""remote\0""broadcast\0""brd\0"
		"anycast\0""scope\0""dev\0""label\0""local\0";
	struct rtnl_handle rth;
	struct {
		struct nlmsghdr  n;
		struct ifaddrmsg ifa;
		char             buf[256];
	} req;
	char *d = NULL;
	char *l = NULL;
	inet_prefix lcl;
	inet_prefix peer;
	int local_len = 0;
	int peer_len = 0;
	int brd_len = 0;
	int any_len = 0;
	bool scoped = 0;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST;
	req.n.nlmsg_type = cmd;
	req.ifa.ifa_family = preferred_family;

	while (*argv) {
		const int option_num = index_in_strings(option, *argv);
		switch (option_num) {
			case 0: /* peer */
			case 1: /* remote */
				NEXT_ARG();

				if (peer_len) {
					duparg("peer", *argv);
				}
				get_prefix(&peer, *argv, req.ifa.ifa_family);
				peer_len = peer.bytelen;
				if (req.ifa.ifa_family == AF_UNSPEC) {
					req.ifa.ifa_family = peer.family;
				}
				addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &peer.data, peer.bytelen);
				req.ifa.ifa_prefixlen = peer.bitlen;
				break;
			case 2: /* broadcast */
			case 3: /* brd */
			{
				inet_prefix addr;
				NEXT_ARG();
				if (brd_len) {
					duparg("broadcast", *argv);
				}
				if (LONE_CHAR(*argv, '+')) {
					brd_len = -1;
				} else if (LONE_DASH(*argv)) {
					brd_len = -2;
				} else {
					get_addr(&addr, *argv, req.ifa.ifa_family);
					if (req.ifa.ifa_family == AF_UNSPEC)
						req.ifa.ifa_family = addr.family;
					addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &addr.data, addr.bytelen);
					brd_len = addr.bytelen;
				}
				break;
			}
			case 4: /* anycast */
			{
				inet_prefix addr;
				NEXT_ARG();
				if (any_len) {
					duparg("anycast", *argv);
				}
				get_addr(&addr, *argv, req.ifa.ifa_family);
				if (req.ifa.ifa_family == AF_UNSPEC) {
					req.ifa.ifa_family = addr.family;
				}
				addattr_l(&req.n, sizeof(req), IFA_ANYCAST, &addr.data, addr.bytelen);
				any_len = addr.bytelen;
				break;
			}
			case 5: /* scope */
			{
				uint32_t scope = 0;
				NEXT_ARG();
				if (rtnl_rtscope_a2n(&scope, *argv)) {
					invarg(*argv, "scope");
				}
				req.ifa.ifa_scope = scope;
				scoped = 1;
				break;
			}
			case 6: /* dev */
				NEXT_ARG();
				d = *argv;
				break;
			case 7: /* label */
				NEXT_ARG();
				l = *argv;
				addattr_l(&req.n, sizeof(req), IFA_LABEL, l, strlen(l)+1);
				break;
			case 8:	/* local */
				NEXT_ARG();
			default:
				if (local_len) {
					duparg2("local", *argv);
				}
				get_prefix(&lcl, *argv, req.ifa.ifa_family);
				if (req.ifa.ifa_family == AF_UNSPEC) {
					req.ifa.ifa_family = lcl.family;
				}
				addattr_l(&req.n, sizeof(req), IFA_LOCAL, &lcl.data, lcl.bytelen);
				local_len = lcl.bytelen;
		}
		argv++;
	}

	if (d == NULL) {
		bb_error_msg(bb_msg_requires_arg, "\"dev\"");
		return -1;
	}
	if (l && strncmp(d, l, strlen(d)) != 0) {
		bb_error_msg_and_die("\"dev\" (%s) must match \"label\" (%s)", d, l);
	}

	if (peer_len == 0 && local_len && cmd != RTM_DELADDR) {
		peer = lcl;
		addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &lcl.data, lcl.bytelen);
	}
	if (req.ifa.ifa_prefixlen == 0)
		req.ifa.ifa_prefixlen = lcl.bitlen;

	if (brd_len < 0 && cmd != RTM_DELADDR) {
		inet_prefix brd;
		int i;
		if (req.ifa.ifa_family != AF_INET) {
			bb_error_msg_and_die("broadcast can be set only for IPv4 addresses");
		}
		brd = peer;
		if (brd.bitlen <= 30) {
			for (i=31; i>=brd.bitlen; i--) {
				if (brd_len == -1)
					brd.data[0] |= htonl(1<<(31-i));
				else
					brd.data[0] &= ~htonl(1<<(31-i));
			}
			addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &brd.data, brd.bytelen);
			brd_len = brd.bytelen;
		}
	}
	if (!scoped && cmd != RTM_DELADDR)
		req.ifa.ifa_scope = default_scope(&lcl);

	xrtnl_open(&rth);

	ll_init_map(&rth);

	req.ifa.ifa_index = xll_name_to_index(d);

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
		return 2;

	return 0;
}

/* Return value becomes exitcode. It's okay to not return at all */
int do_ipaddr(char **argv)
{
	static const char commands[] ALIGN1 =
		"add\0""delete\0""list\0""show\0""lst\0""flush\0";

	int command_num = 2; /* default command is list */

	if (*argv) {
		command_num = index_in_substrings(commands, *argv);
		if (command_num < 0 || command_num > 5)
			bb_error_msg_and_die("unknown command %s", *argv);
		argv++;
	}
	if (command_num == 0) /* add */
		return ipaddr_modify(RTM_NEWADDR, argv);
	if (command_num == 1) /* delete */
		return ipaddr_modify(RTM_DELADDR, argv);
	if (command_num == 5) /* flush */
		return ipaddr_list_or_flush(argv, 1);
	/* 2 == list, 3 == show, 4 == lst */
	return ipaddr_list_or_flush(argv, 0);
}
