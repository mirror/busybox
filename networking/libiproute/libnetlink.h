/* vi: set sw=4 ts=4: */
#ifndef __LIBNETLINK_H__
#define __LIBNETLINK_H__ 1

#include <linux/types.h>
/* We need linux/types.h because older kernels use __u32 etc
 * in linux/[rt]netlink.h. 2.6.19 seems to be ok, though */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

struct rtnl_handle
{
	int			fd;
	struct sockaddr_nl	local;
	struct sockaddr_nl	peer;
	uint32_t		seq;
	uint32_t		dump;
};

extern int xrtnl_open(struct rtnl_handle *rth);
extern void rtnl_close(struct rtnl_handle *rth);
extern int xrtnl_wilddump_request(struct rtnl_handle *rth, int fam, int type);
extern int rtnl_dump_request(struct rtnl_handle *rth, int type, void *req, int len);
extern int xrtnl_dump_filter(struct rtnl_handle *rth,
			int (*filter)(struct sockaddr_nl*, struct nlmsghdr *n, void*),
			void *arg1);

/* bbox doesn't use parameters no. 3, 4, 6, 7, stub them out */
#define rtnl_talk(rtnl, n, peer, groups, answer, junk, jarg) \
	rtnl_talk(rtnl, n, answer)
extern int rtnl_talk(struct rtnl_handle *rtnl, struct nlmsghdr *n, pid_t peer,
			unsigned groups, struct nlmsghdr *answer,
			int (*junk)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
			void *jarg);

extern int rtnl_send(struct rtnl_handle *rth, char *buf, int);


extern int addattr32(struct nlmsghdr *n, int maxlen, int type, uint32_t data);
extern int addattr_l(struct nlmsghdr *n, int maxlen, int type, void *data, int alen);
extern int rta_addattr32(struct rtattr *rta, int maxlen, int type, uint32_t data);
extern int rta_addattr_l(struct rtattr *rta, int maxlen, int type, void *data, int alen);

extern int parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len);

#endif /* __LIBNETLINK_H__ */
