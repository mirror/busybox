/* vi: set sw=4 ts=4: */
#ifndef _IP_COMMON_H
#define _IP_COMMON_H 1

#include "busybox.h"
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#if !defined IFA_RTA
#include <linux/if_addr.h>
#endif
#if !defined IFLA_RTA
#include <linux/if_link.h>
#endif

extern int preferred_family;
extern char * _SL_;

extern void ip_parse_common_args(int *argcp, char ***argvp);
extern int print_neigh(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);
extern int ipaddr_list_or_flush(int argc, char **argv, int flush);
extern int iproute_monitor(int argc, char **argv);
extern void iplink_usage(void) ATTRIBUTE_NORETURN;
extern void ipneigh_reset_filter(void);
extern int do_ipaddr(int argc, char **argv);
extern int do_iproute(int argc, char **argv);
extern int do_iprule(int argc, char **argv);
extern int do_ipneigh(int argc, char **argv);
extern int do_iptunnel(int argc, char **argv);
extern int do_iplink(int argc, char **argv);
extern int do_ipmonitor(int argc, char **argv);
extern int do_multiaddr(int argc, char **argv);
extern int do_multiroute(int argc, char **argv);
#endif /* ip_common.h */
