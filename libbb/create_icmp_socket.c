/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * create raw socket for icmp protocol test permision
 * and drop root privilegies if running setuid
 *
 */

#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include "libbb.h"

int create_icmp_socket(void)
{
	struct protoent *proto;
	int sock;

	proto = getprotobyname("icmp");
	/* if getprotobyname failed, just silently force
	 * proto->p_proto to have the correct value for "icmp" */
	if ((sock = socket(AF_INET, SOCK_RAW,
			(proto ? proto->p_proto : 1))) < 0) {        /* 1 == ICMP */
		if (errno == EPERM)
			error_msg_and_die("permission denied. (are you root?)");
		else
			perror_msg_and_die(can_not_create_raw_socket);
	}

	/* drop root privs if running setuid */
	setuid(getuid());

	return sock;
}
