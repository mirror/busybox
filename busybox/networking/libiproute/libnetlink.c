/*
 * libnetlink.c	RTnetlink service routines.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <sys/socket.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/uio.h>

#include "libnetlink.h"
#include "libbb.h"

void rtnl_close(struct rtnl_handle *rth)
{
	close(rth->fd);
}

int rtnl_open(struct rtnl_handle *rth, unsigned subscriptions)
{
	int addr_len;

	memset(rth, 0, sizeof(rth));

	rth->fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (rth->fd < 0) {
		bb_perror_msg("Cannot open netlink socket");
		return -1;
	}

	memset(&rth->local, 0, sizeof(rth->local));
	rth->local.nl_family = AF_NETLINK;
	rth->local.nl_groups = subscriptions;

	if (bind(rth->fd, (struct sockaddr*)&rth->local, sizeof(rth->local)) < 0) {
		bb_perror_msg("Cannot bind netlink socket");
		return -1;
	}
	addr_len = sizeof(rth->local);
	if (getsockname(rth->fd, (struct sockaddr*)&rth->local, &addr_len) < 0) {
		bb_perror_msg("Cannot getsockname");
		return -1;
	}
	if (addr_len != sizeof(rth->local)) {
		bb_error_msg("Wrong address length %d", addr_len);
		return -1;
	}
	if (rth->local.nl_family != AF_NETLINK) {
		bb_error_msg("Wrong address family %d", rth->local.nl_family);
		return -1;
	}
	rth->seq = time(NULL);
	return 0;
}

int rtnl_wilddump_request(struct rtnl_handle *rth, int family, int type)
{
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;
	struct sockaddr_nl nladdr;

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	req.nlh.nlmsg_len = sizeof(req);
	req.nlh.nlmsg_type = type;
	req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
	req.nlh.nlmsg_pid = 0;
	req.nlh.nlmsg_seq = rth->dump = ++rth->seq;
	req.g.rtgen_family = family;

	return sendto(rth->fd, (void*)&req, sizeof(req), 0, (struct sockaddr*)&nladdr, sizeof(nladdr));
}

int rtnl_send(struct rtnl_handle *rth, char *buf, int len)
{
	struct sockaddr_nl nladdr;

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	return sendto(rth->fd, buf, len, 0, (struct sockaddr*)&nladdr, sizeof(nladdr));
}

int rtnl_dump_request(struct rtnl_handle *rth, int type, void *req, int len)
{
	struct nlmsghdr nlh;
	struct sockaddr_nl nladdr;
	struct iovec iov[2] = { { &nlh, sizeof(nlh) }, { req, len } };
	struct msghdr msg = {
		(void*)&nladdr, sizeof(nladdr),
		iov,	2,
		NULL,	0,
		0
	};

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	nlh.nlmsg_len = NLMSG_LENGTH(len);
	nlh.nlmsg_type = type;
	nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
	nlh.nlmsg_pid = 0;
	nlh.nlmsg_seq = rth->dump = ++rth->seq;

	return sendmsg(rth->fd, &msg, 0);
}

int rtnl_dump_filter(struct rtnl_handle *rth,
		     int (*filter)(struct sockaddr_nl *, struct nlmsghdr *n, void *),
		     void *arg1,
		     int (*junk)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
		     void *arg2)
{
	char	buf[8192];
	struct sockaddr_nl nladdr;
	struct iovec iov = { buf, sizeof(buf) };

	while (1) {
		int status;
		struct nlmsghdr *h;

		struct msghdr msg = {
			(void*)&nladdr, sizeof(nladdr),
			&iov,	1,
			NULL,	0,
			0
		};

		status = recvmsg(rth->fd, &msg, 0);

		if (status < 0) {
			if (errno == EINTR)
				continue;
			bb_perror_msg("OVERRUN");
			continue;
		}
		if (status == 0) {
			bb_error_msg("EOF on netlink");
			return -1;
		}
		if (msg.msg_namelen != sizeof(nladdr)) {
			bb_error_msg_and_die("sender address length == %d", msg.msg_namelen);
		}

		h = (struct nlmsghdr*)buf;
		while (NLMSG_OK(h, status)) {
			int err;

			if (nladdr.nl_pid != 0 ||
			    h->nlmsg_pid != rth->local.nl_pid ||
			    h->nlmsg_seq != rth->dump) {
				if (junk) {
					err = junk(&nladdr, h, arg2);
					if (err < 0) {
						return err;
					}
				}
				goto skip_it;
			}

			if (h->nlmsg_type == NLMSG_DONE) {
				return 0;
			}
			if (h->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *l_err = (struct nlmsgerr*)NLMSG_DATA(h);
				if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
					bb_error_msg("ERROR truncated");
				} else {
					errno = -l_err->error;
					bb_perror_msg("RTNETLINK answers");
				}
				return -1;
			}
			err = filter(&nladdr, h, arg1);
			if (err < 0) {
				return err;
			}

skip_it:
			h = NLMSG_NEXT(h, status);
		}
		if (msg.msg_flags & MSG_TRUNC) {
			bb_error_msg("Message truncated");
			continue;
		}
		if (status) {
			bb_error_msg_and_die("!!!Remnant of size %d", status);
		}
	}
}

int rtnl_talk(struct rtnl_handle *rtnl, struct nlmsghdr *n, pid_t peer,
	      unsigned groups, struct nlmsghdr *answer,
	      int (*junk)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
	      void *jarg)
{
	int status;
	unsigned seq;
	struct nlmsghdr *h;
	struct sockaddr_nl nladdr;
	struct iovec iov = { (void*)n, n->nlmsg_len };
	char   buf[8192];
	struct msghdr msg = {
		(void*)&nladdr, sizeof(nladdr),
		&iov,	1,
		NULL,	0,
		0
	};

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid = peer;
	nladdr.nl_groups = groups;

	n->nlmsg_seq = seq = ++rtnl->seq;
	if (answer == NULL) {
		n->nlmsg_flags |= NLM_F_ACK;
	}
	status = sendmsg(rtnl->fd, &msg, 0);

	if (status < 0) {
		bb_perror_msg("Cannot talk to rtnetlink");
		return -1;
	}

	iov.iov_base = buf;

	while (1) {
		iov.iov_len = sizeof(buf);
		status = recvmsg(rtnl->fd, &msg, 0);

		if (status < 0) {
			if (errno == EINTR) {
				continue;
			}
			bb_perror_msg("OVERRUN");
			continue;
		}
		if (status == 0) {
			bb_error_msg("EOF on netlink");
			return -1;
		}
		if (msg.msg_namelen != sizeof(nladdr)) {
			bb_error_msg_and_die("sender address length == %d", msg.msg_namelen);
		}
		for (h = (struct nlmsghdr*)buf; status >= sizeof(*h); ) {
			int l_err;
			int len = h->nlmsg_len;
			int l = len - sizeof(*h);

			if (l<0 || len>status) {
				if (msg.msg_flags & MSG_TRUNC) {
					bb_error_msg("Truncated message");
					return -1;
				}
				bb_error_msg_and_die("!!!malformed message: len=%d", len);
			}

			if (nladdr.nl_pid != peer ||
			    h->nlmsg_pid != rtnl->local.nl_pid ||
			    h->nlmsg_seq != seq) {
				if (junk) {
					l_err = junk(&nladdr, h, jarg);
					if (l_err < 0) {
						return l_err;
					}
				}
				continue;
			}

			if (h->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(h);
				if (l < sizeof(struct nlmsgerr)) {
					bb_error_msg("ERROR truncated");
				} else {
					errno = -err->error;
					if (errno == 0) {
						if (answer) {
							memcpy(answer, h, h->nlmsg_len);
						}
						return 0;
					}
					bb_perror_msg("RTNETLINK answers");
				}
				return -1;
			}
			if (answer) {
				memcpy(answer, h, h->nlmsg_len);
				return 0;
			}

			bb_error_msg("Unexpected reply!!!");

			status -= NLMSG_ALIGN(len);
			h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len));
		}
		if (msg.msg_flags & MSG_TRUNC) {
			bb_error_msg("Message truncated");
			continue;
		}
		if (status) {
			bb_error_msg_and_die("!!!Remnant of size %d", status);
		}
	}
}

int rtnl_listen(struct rtnl_handle *rtnl,
	      int (*handler)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
	      void *jarg)
{
	int status;
	struct nlmsghdr *h;
	struct sockaddr_nl nladdr;
	struct iovec iov;
	char   buf[8192];
	struct msghdr msg = {
		(void*)&nladdr, sizeof(nladdr),
		&iov,	1,
		NULL,	0,
		0
	};

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid = 0;
	nladdr.nl_groups = 0;


	iov.iov_base = buf;

	while (1) {
		iov.iov_len = sizeof(buf);
		status = recvmsg(rtnl->fd, &msg, 0);

		if (status < 0) {
			if (errno == EINTR)
				continue;
			bb_perror_msg("OVERRUN");
			continue;
		}
		if (status == 0) {
			bb_error_msg("EOF on netlink");
			return -1;
		}
		if (msg.msg_namelen != sizeof(nladdr)) {
			bb_error_msg_and_die("Sender address length == %d", msg.msg_namelen);
		}
		for (h = (struct nlmsghdr*)buf; status >= sizeof(*h); ) {
			int err;
			int len = h->nlmsg_len;
			int l = len - sizeof(*h);

			if (l<0 || len>status) {
				if (msg.msg_flags & MSG_TRUNC) {
					bb_error_msg("Truncated message");
					return -1;
				}
				bb_error_msg_and_die("!!!malformed message: len=%d", len);
			}

			err = handler(&nladdr, h, jarg);
			if (err < 0) {
				return err;
			}

			status -= NLMSG_ALIGN(len);
			h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len));
		}
		if (msg.msg_flags & MSG_TRUNC) {
			bb_error_msg("Message truncated");
			continue;
		}
		if (status) {
			bb_error_msg_and_die("!!!Remnant of size %d", status);
		}
	}
}

int rtnl_from_file(FILE *rtnl,
	      int (*handler)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
	      void *jarg)
{
	int status;
	struct sockaddr_nl nladdr;
	char   buf[8192];
	struct nlmsghdr *h = (void*)buf;

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	nladdr.nl_pid = 0;
	nladdr.nl_groups = 0;

	while (1) {
		int err, len, type;
		int l;

		status = fread(&buf, 1, sizeof(*h), rtnl);

		if (status < 0) {
			if (errno == EINTR)
				continue;
			bb_perror_msg("rtnl_from_file: fread");
			return -1;
		}
		if (status == 0)
			return 0;

		len = h->nlmsg_len;
		type= h->nlmsg_type;
		l = len - sizeof(*h);

		if (l<0 || len>sizeof(buf)) {
			bb_error_msg("!!!malformed message: len=%d @%lu",
				len, ftell(rtnl));
			return -1;
		}

		status = fread(NLMSG_DATA(h), 1, NLMSG_ALIGN(l), rtnl);

		if (status < 0) {
			bb_perror_msg("rtnl_from_file: fread");
			return -1;
		}
		if (status < l) {
			bb_error_msg("rtnl-from_file: truncated message");
			return -1;
		}

		err = handler(&nladdr, h, jarg);
		if (err < 0)
			return err;
	}
}

int addattr32(struct nlmsghdr *n, int maxlen, int type, __u32 data)
{
	int len = RTA_LENGTH(4);
	struct rtattr *rta;
	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen)
		return -1;
	rta = (struct rtattr*)(((char*)n) + NLMSG_ALIGN(n->nlmsg_len));
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), &data, 4);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;
	return 0;
}

int addattr_l(struct nlmsghdr *n, int maxlen, int type, void *data, int alen)
{
	int len = RTA_LENGTH(alen);
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen)
		return -1;
	rta = (struct rtattr*)(((char*)n) + NLMSG_ALIGN(n->nlmsg_len));
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;
	return 0;
}

int rta_addattr32(struct rtattr *rta, int maxlen, int type, __u32 data)
{
	int len = RTA_LENGTH(4);
	struct rtattr *subrta;

	if (RTA_ALIGN(rta->rta_len) + len > maxlen) {
		return -1;
	}
	subrta = (struct rtattr*)(((char*)rta) + RTA_ALIGN(rta->rta_len));
	subrta->rta_type = type;
	subrta->rta_len = len;
	memcpy(RTA_DATA(subrta), &data, 4);
	rta->rta_len = NLMSG_ALIGN(rta->rta_len) + len;
	return 0;
}

int rta_addattr_l(struct rtattr *rta, int maxlen, int type, void *data, int alen)
{
	struct rtattr *subrta;
	int len = RTA_LENGTH(alen);

	if (RTA_ALIGN(rta->rta_len) + len > maxlen) {
		return -1;
	}
	subrta = (struct rtattr*)(((char*)rta) + RTA_ALIGN(rta->rta_len));
	subrta->rta_type = type;
	subrta->rta_len = len;
	memcpy(RTA_DATA(subrta), data, alen);
	rta->rta_len = NLMSG_ALIGN(rta->rta_len) + len;
	return 0;
}


int parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max) {
			tb[rta->rta_type] = rta;
		}
		rta = RTA_NEXT(rta,len);
	}
	if (len) {
		bb_error_msg("!!!Deficit %d, rta_len=%d", len, rta->rta_len);
	}
	return 0;
}
