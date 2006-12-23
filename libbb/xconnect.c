/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Connect to host at port using address resolution from getaddrinfo
 *
 */

#include "libbb.h"

/* Return network byte ordered port number for a service.
 * If "port" is a number use it as the port.
 * If "port" is a name it is looked up in /etc/services, if it isnt found return
 * default_port
 */
unsigned short bb_lookup_port(const char *port, const char *protocol, unsigned short default_port)
{
	unsigned short port_nr = htons(default_port);
	if (port) {
		char *endptr;
		int old_errno;
		long port_long;

		/* Since this is a lib function, we're not allowed to reset errno to 0.
		 * Doing so could break an app that is deferring checking of errno. */
		old_errno = errno;
		errno = 0;
		port_long = strtol(port, &endptr, 10);
		if (errno != 0 || *endptr!='\0' || endptr==port || port_long < 0 || port_long > 65535) {
			struct servent *tserv = getservbyname(port, protocol);
			if (tserv) {
				port_nr = tserv->s_port;
			}
		} else {
			port_nr = htons(port_long);
		}
		errno = old_errno;
	}
	return port_nr;
}

void bb_lookup_host(struct sockaddr_in *s_in, const char *host)
{
	struct hostent *he;

	memset(s_in, 0, sizeof(struct sockaddr_in));
	s_in->sin_family = AF_INET;
	he = xgethostbyname(host);
	memcpy(&(s_in->sin_addr), he->h_addr_list[0], he->h_length);
}

void xconnect(int s, const struct sockaddr *s_addr, socklen_t addrlen)
{
	if (connect(s, s_addr, addrlen) < 0) {
		if (ENABLE_FEATURE_CLEAN_UP) close(s);
		if (s_addr->sa_family == AF_INET)
			bb_perror_msg_and_die("%s (%s)",
				"cannot connect to remote host",
				inet_ntoa(((struct sockaddr_in *)s_addr)->sin_addr));
		bb_perror_msg_and_die("cannot connect to remote host");
	}
}

int xconnect_tcp_v4(struct sockaddr_in *s_addr)
{
	int s = xsocket(AF_INET, SOCK_STREAM, 0);
	xconnect(s, (struct sockaddr*) s_addr, sizeof(*s_addr));
	return s;
}

static const int one = 1;
int setsockopt_reuseaddr(int fd)
{
	return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
}
int setsockopt_broadcast(int fd)
{
	return setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
}

int dotted2sockaddr(const char *dotted, struct sockaddr* sp, int socklen)
{
	union {
		struct in_addr a4;
#if ENABLE_FEATURE_IPV6
		struct in6_addr a6;
#endif
	} a;

	/* TODO maybe: port spec? like n.n.n.n:nn */

#if ENABLE_FEATURE_IPV6
	if (socklen >= sizeof(struct sockaddr_in6)
	 && inet_pton(AF_INET6, dotted, &a.a6) > 0
	) {
		((struct sockaddr_in6*)sp)->sin6_family = AF_INET6;
		((struct sockaddr_in6*)sp)->sin6_addr = a.a6;
		/* ((struct sockaddr_in6*)sp)->sin6_port = */
		return 0; /* success */
	}
#endif
	if (socklen >= sizeof(struct sockaddr_in)
	 && inet_pton(AF_INET, dotted, &a.a4) > 0
	) {
		((struct sockaddr_in*)sp)->sin_family = AF_INET;
		((struct sockaddr_in*)sp)->sin_addr = a.a4;
		/* ((struct sockaddr_in*)sp)->sin_port = */
		return 0; /* success */
	}
	return 1;
}

int xsocket_stream_ip4or6(sa_family_t *fp)
{
	int fd;
#if ENABLE_FEATURE_IPV6
	fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fp) *fp = AF_INET6;
	if (fd < 0)
#endif
	{
		fd = xsocket(AF_INET, SOCK_STREAM, 0);
		if (fp) *fp = AF_INET;
	}
	return fd;
}

int create_and_bind_socket_ip4or6(const char *hostaddr, int port)
{
	int fd;
	sockaddr_inet sa;

	memset(&sa, 0, sizeof(sa));
	if (hostaddr) {
		if (dotted2sockaddr(hostaddr, &sa.sa, sizeof(sa)))
			bb_error_msg_and_die("bad address '%s'", hostaddr);
		/* user specified bind addr dictates family */
		fd = xsocket(sa.sa.sa_family, SOCK_STREAM, 0);
	} else 
		fd = xsocket_stream_ip4or6(&sa.sa.sa_family);
	setsockopt_reuseaddr(fd);

	/* if (port >= 0) { */
#if ENABLE_FEATURE_IPV6
		if (sa.sa.sa_family == AF_INET6 /* && !sa.sin6.sin6_port */)
			sa.sin6.sin6_port = htons(port);
#endif
		if (sa.sa.sa_family == AF_INET /* && !sa.sin.sin_port */)
			sa.sin.sin_port = htons(port);
	/* } */

	xbind(fd, &sa.sa, sizeof(sa));
	return fd;
}
