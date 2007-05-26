/* vi: set sw=4 ts=4: */
/*
 * ip.c		"ip" utility frontend.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 * Bernhard Fischer rewrote to use index_in_substr_array
 */

#include "libbb.h"

#include "libiproute/utils.h"
#include "libiproute/ip_common.h"

#if ENABLE_FEATURE_IP_ADDRESS \
 || ENABLE_FEATURE_IP_ROUTE \
 || ENABLE_FEATURE_IP_LINK \
 || ENABLE_FEATURE_IP_TUNNEL \
 || ENABLE_FEATURE_IP_RULE

static int ATTRIBUTE_NORETURN ip_print_help(int ATTRIBUTE_UNUSED ac, char ATTRIBUTE_UNUSED **av)
{
	bb_show_usage();
}

static int (*ip_func)(int argc, char **argv) = ip_print_help;

static int ip_do(int argc, char **argv)
{
	ip_parse_common_args(&argc, &argv);
	return ip_func(argc-1, argv+1);
}

#if ENABLE_FEATURE_IP_ADDRESS
int ipaddr_main(int argc, char **argv);
int ipaddr_main(int argc, char **argv)
{
    ip_func = do_ipaddr;
    return ip_do(argc, argv);
}
#endif
#if ENABLE_FEATURE_IP_LINK
int iplink_main(int argc, char **argv);
int iplink_main(int argc, char **argv)
{
    ip_func = do_iplink;
    return ip_do(argc, argv);
}
#endif
#if ENABLE_FEATURE_IP_ROUTE
int iproute_main(int argc, char **argv);
int iproute_main(int argc, char **argv)
{
    ip_func = do_iproute;
    return ip_do(argc, argv);
}
#endif
#if ENABLE_FEATURE_IP_RULE
int iprule_main(int argc, char **argv);
int iprule_main(int argc, char **argv)
{
    ip_func = do_iprule;
    return ip_do(argc, argv);
}
#endif
#if ENABLE_FEATURE_IP_TUNNEL
int iptunnel_main(int argc, char **argv);
int iptunnel_main(int argc, char **argv)
{
    ip_func = do_iptunnel;
    return ip_do(argc, argv);
}
#endif


int ip_main(int argc, char **argv);
int ip_main(int argc, char **argv)
{
	const char * const keywords[] = {
		USE_FEATURE_IP_ADDRESS("address",)
		USE_FEATURE_IP_ROUTE("route",)
		USE_FEATURE_IP_LINK("link",)
		USE_FEATURE_IP_TUNNEL("tunnel", "tunl",)
		USE_FEATURE_IP_RULE("rule",)
		NULL
	};
	enum {
		USE_FEATURE_IP_ADDRESS(IP_addr,)
		USE_FEATURE_IP_ROUTE(IP_route,)
		USE_FEATURE_IP_LINK(IP_link,)
		USE_FEATURE_IP_TUNNEL(IP_tunnel, IP_tunl,)
		USE_FEATURE_IP_RULE(IP_rule,)
		IP_none
	};

	ip_parse_common_args(&argc, &argv);
	if (argc > 1) {
		int key = index_in_substr_array(keywords, argv[1]);
		argc -= 2;
		argv += 2;
#if ENABLE_FEATURE_IP_ADDRESS
		if (key == IP_addr)
			ip_func = do_ipaddr;
#endif
#if ENABLE_FEATURE_IP_ROUTE
		if (key == IP_route)
			ip_func = do_iproute;
#endif
#if ENABLE_FEATURE_IP_LINK
		if (key == IP_link)
			ip_func = do_iplink;
#endif
#if ENABLE_FEATURE_IP_TUNNEL
		if (key == IP_tunnel || key == IP_tunl)
			ip_func = do_iptunnel;
#endif
#if ENABLE_FEATURE_IP_RULE
		if (key == IP_rule)
			ip_func = do_iprule;
#endif
	}
	return (ip_func(argc, argv));
}

#endif /* any of ENABLE_FEATURE_IP_xxx is 1 */
