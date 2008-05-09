/* vi: set sw=4 ts=4: */
#ifndef _IP_COMMON_H
#define _IP_COMMON_H 1

#include "libbb.h"
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#if !defined IFA_RTA
#include <linux/if_addr.h>
#endif
#if !defined IFLA_RTA
#include <linux/if_link.h>
#endif

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif

extern char **ip_parse_common_args(char **argv);
extern int print_neigh(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
extern int ipaddr_list_or_flush(char **argv, int flush);
extern int iproute_monitor(char **argv);
extern void iplink_usage(void) ATTRIBUTE_NORETURN;
extern void ipneigh_reset_filter(void);

extern int do_ipaddr(char **argv);
extern int do_iproute(char **argv);
extern int do_iprule(char **argv);
extern int do_ipneigh(char **argv);
extern int do_iptunnel(char **argv);
extern int do_iplink(char **argv);
extern int do_ipmonitor(char **argv);
extern int do_multiaddr(char **argv);
extern int do_multiroute(char **argv);

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif

#endif /* ip_common.h */
