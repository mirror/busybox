/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Connect to host at port using address resolusion from getaddrinfo
 *
 */

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include "libbb.h"

int xconnect(const char *host, const char *port)
{
#if CONFIG_FEATURE_IPV6
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *addr_info;
	int error;
	int s;

	memset(&hints, 0, sizeof(hints));
	/* set-up hints structure */
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, port, &hints, &res);
	if (error||!res)
		perror_msg_and_die(gai_strerror(error));
	addr_info=res;
	while (res) {
		s=socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s<0)
		{
			error=s;
			res=res->ai_next;
			continue;
		}
		/* try to connect() to res->ai_addr */
		error = connect(s, res->ai_addr, res->ai_addrlen);
		if (error >= 0)
			break;
		close(s);
		res=res->ai_next;
	}
	freeaddrinfo(addr_info);
	if (error < 0)
	{
		perror_msg_and_die("Unable to connect to remote host (%s)", host);
	}
	return s;
#else
	struct sockaddr_in s_addr;
	int s = socket(AF_INET, SOCK_STREAM, 0);
	struct servent *tserv;
	int port_nr=atoi(port);
	struct hostent * he;

	if (port_nr==0 && (tserv = getservbyname(port, "tcp")) != NULL)
		port_nr = tserv->s_port;

	memset(&s_addr, 0, sizeof(struct sockaddr_in));
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(port_nr);

	he = xgethostbyname(host);
	memcpy(&s_addr.sin_addr, he->h_addr, sizeof s_addr.sin_addr);

	if (connect(s, (struct sockaddr *)&s_addr, sizeof s_addr) < 0)
	{
		perror_msg_and_die("Unable to connect to remote host (%s)", host);
	}
	return s;
#endif
}
