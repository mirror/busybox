/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Connect to host at port using address resolusion from getaddrinfo
 *
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libbb.h"

int bb_getport(const char *port)
{
	int port_nr;
	char *endptr;
	struct servent *tserv;

	if (!port) {
		return -1;
	}
	port_nr=strtol(port, &endptr, 10);
	if (errno != 0 || *endptr!='\0' || endptr==port || port_nr < 1 || port_nr > 65536) 
	{
		if (port_nr==0 && (tserv = getservbyname(port, "tcp")) != NULL) {
			port_nr = tserv->s_port;
		} else {
			return -1;
		}
	} else {
		port_nr = htons(port_nr);
	}
	return port_nr;
}

void bb_lookup_host(struct sockaddr_in *s_in, const char *host, const char *port)
{
	struct hostent *he;

	memset(s_in, 0, sizeof(struct sockaddr_in));
	s_in->sin_family = AF_INET;
	he = xgethostbyname(host);
	memcpy(&(s_in->sin_addr), he->h_addr_list[0], he->h_length);

	if (port) {
		s_in->sin_port=bb_getport(port);
	}
}

int xconnect(struct sockaddr_in *s_addr)
{
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(s, (struct sockaddr_in *)s_addr, sizeof(struct sockaddr_in)) < 0)
	{
		bb_perror_msg_and_die("Unable to connect to remote host (%s)", 
				inet_ntoa(s_addr->sin_addr));
	}
	return s;
}
