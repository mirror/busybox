/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * create raw socket for icmp (IPv6 version) protocol test permission
 * and drop root privileges if running setuid
 *
 */

#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include "libbb.h"

#ifdef CONFIG_FEATURE_IPV6
int create_icmp6_socket(void)
{
	struct protoent *proto;
	int sock;

	proto = getprotobyname("ipv6-icmp");
	/* if getprotobyname failed, just silently force
	 * proto->p_proto to have the correct value for "ipv6-icmp" */
	if ((sock = socket(AF_INET6, SOCK_RAW,
			(proto ? proto->p_proto : IPPROTO_ICMPV6))) < 0) {
		if (errno == EPERM)
			bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
		else
			bb_perror_msg_and_die(bb_msg_can_not_create_raw_socket);
	}

	/* drop root privs if running setuid */
	xsetuid(getuid());

	return sock;
}
#endif
