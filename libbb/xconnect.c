/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Connect to host at port using address resolution from getaddrinfo
 *
 */

#include <netinet/in.h>
#include "libbb.h"

static const int one = 1;
int setsockopt_reuseaddr(int fd)
{
	return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
}
int setsockopt_broadcast(int fd)
{
	return setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
}

void xconnect(int s, const struct sockaddr *s_addr, socklen_t addrlen)
{
	if (connect(s, s_addr, addrlen) < 0) {
		if (ENABLE_FEATURE_CLEAN_UP)
			close(s);
		if (s_addr->sa_family == AF_INET)
			bb_perror_msg_and_die("%s (%s)",
				"cannot connect to remote host",
				inet_ntoa(((struct sockaddr_in *)s_addr)->sin_addr));
		bb_perror_msg_and_die("cannot connect to remote host");
	}
}

/* Return port number for a service.
 * If "port" is a number use it as the port.
 * If "port" is a name it is looked up in /etc/services, if it isnt found return
 * default_port */
unsigned bb_lookup_port(const char *port, const char *protocol, unsigned default_port)
{
	unsigned port_nr = default_port;
	if (port) {
		int old_errno;

		/* Since this is a lib function, we're not allowed to reset errno to 0.
		 * Doing so could break an app that is deferring checking of errno. */
		old_errno = errno;
		port_nr = bb_strtou(port, NULL, 10);
		if (errno || port_nr > 65535) {
			struct servent *tserv = getservbyname(port, protocol);
			port_nr = default_port;
			if (tserv)
				port_nr = ntohs(tserv->s_port);
		}
		errno = old_errno;
	}
	return (uint16_t)port_nr;
}


/* "Old" networking API - only IPv4 */


void bb_lookup_host(struct sockaddr_in *s_in, const char *host)
{
	struct hostent *he;

	memset(s_in, 0, sizeof(struct sockaddr_in));
	s_in->sin_family = AF_INET;
	he = xgethostbyname(host);
	memcpy(&(s_in->sin_addr), he->h_addr_list[0], he->h_length);
}

int xconnect_tcp_v4(struct sockaddr_in *s_addr)
{
	int s = xsocket(AF_INET, SOCK_STREAM, 0);
	xconnect(s, (struct sockaddr*) s_addr, sizeof(*s_addr));
	return s;
}


/* "New" networking API */


int get_nport(len_and_sockaddr *lsa)
{
#if ENABLE_FEATURE_IPV6
	if (lsa->sa.sa_family == AF_INET6) {
		return lsa->sin6.sin6_port;
	}
#endif
	if (lsa->sa.sa_family == AF_INET) {
		return lsa->sin.sin_port;
	}
	return -1;
	/* What? UNIX socket? IPX?? :) */
}

void set_nport(len_and_sockaddr *lsa, unsigned port)
{
#if ENABLE_FEATURE_IPV6
	if (lsa->sa.sa_family == AF_INET6) {
		lsa->sin6.sin6_port = port;
		return;
	}
#endif
	if (lsa->sa.sa_family == AF_INET) {
		lsa->sin.sin_port = port;
		return;
	}
	/* What? UNIX socket? IPX?? :) */
}

/* peer: "1.2.3.4[:port]", "www.google.com[:port]"
 * port: if neither of above specifies port #
 */
static len_and_sockaddr* str2sockaddr(const char *host, int port, int ai_flags)
{
	int rc;
	len_and_sockaddr *r; // = NULL;
	struct addrinfo *result = NULL;
	const char *org_host = host; /* only for error msg */
	const char *cp;
	struct addrinfo hint;

	/* Ugly parsing of host:addr */
	if (ENABLE_FEATURE_IPV6 && host[0] == '[') {
		host++;
		cp = strchr(host, ']');
		if (!cp || cp[1] != ':') /* Malformed: must have [xx]:nn */
			bb_error_msg_and_die("bad address '%s'", org_host);
			//return r; /* return NULL */
	} else {
		cp = strrchr(host, ':');
		if (ENABLE_FEATURE_IPV6 && cp && strchr(host, ':') != cp) {
			/* There is more than one ':' (e.g. "::1") */
			cp = NULL; /* it's not a port spec */
		}
	}
	if (cp) {
		int sz = cp - host + 1;
		host = safe_strncpy(alloca(sz), host, sz);
		if (ENABLE_FEATURE_IPV6 && *cp != ':')
			cp++; /* skip ']' */
		cp++; /* skip ':' */
		port = xatou16(cp);
	}

	memset(&hint, 0 , sizeof(hint));
	/* hint.ai_family = AF_UNSPEC; - zero anyway */
#if !ENABLE_FEATURE_IPV6
	hint.ai_family = AF_INET; /* do not try to find IPv6 */
#endif
	/* Needed. Or else we will get each address thrice (or more)
	 * for each possible socket type (tcp,udp,raw...): */
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = ai_flags;
	rc = getaddrinfo(host, NULL, &hint, &result);
	if (rc || !result)
		bb_error_msg_and_die("bad address '%s'", org_host);
	r = xmalloc(offsetof(len_and_sockaddr, sa) + result->ai_addrlen);
	r->len = result->ai_addrlen;
	memcpy(&r->sa, result->ai_addr, result->ai_addrlen);
	set_nport(r, htons(port));
	freeaddrinfo(result);
	return r;
}

len_and_sockaddr* host2sockaddr(const char *host, int port)
{
	return str2sockaddr(host, port, 0);
}

static len_and_sockaddr* dotted2sockaddr(const char *host, int port)
{
	return str2sockaddr(host, port, NI_NUMERICHOST);
}

int xsocket_stream(len_and_sockaddr **lsap)
{
	len_and_sockaddr *lsa;
	int fd;
	int len = sizeof(struct sockaddr_in);
	int family = AF_INET;

#if ENABLE_FEATURE_IPV6
	fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (fd >= 0) {
		len = sizeof(struct sockaddr_in6);
		family = AF_INET6;
	} else
#endif
	{
		fd = xsocket(AF_INET, SOCK_STREAM, 0);
	}
	lsa = xzalloc(offsetof(len_and_sockaddr, sa) + len);
	lsa->len = len;
	lsa->sa.sa_family = family;
	*lsap = lsa;
	return fd;
}

int create_and_bind_stream_or_die(const char *bindaddr, int port)
{
	int fd;
	len_and_sockaddr *lsa;

	if (bindaddr && bindaddr[0]) {
		lsa = dotted2sockaddr(bindaddr, port);
		/* currently NULL check is in str2sockaddr */
		//if (!lsa)
		//	bb_error_msg_and_die("bad address '%s'", bindaddr);
		/* user specified bind addr dictates family */
		fd = xsocket(lsa->sa.sa_family, SOCK_STREAM, 0);
	} else {
		fd = xsocket_stream(&lsa);
		set_nport(lsa, htons(port));
	}
	setsockopt_reuseaddr(fd);
	xbind(fd, &lsa->sa, lsa->len);
	free(lsa);
	return fd;
}

int create_and_connect_stream_or_die(const char *peer, int port)
{
	int fd;
	len_and_sockaddr *lsa;

	lsa = host2sockaddr(peer, port);
	/* currently NULL check is in str2sockaddr */
	//if (!lsa)
	//	bb_error_msg_and_die("bad address '%s'", peer);
	fd = xsocket(lsa->sa.sa_family, SOCK_STREAM, 0);
	setsockopt_reuseaddr(fd);
	xconnect(fd, &lsa->sa, lsa->len);
	free(lsa);
	return fd;
}

int xconnect_stream(const len_and_sockaddr *lsa)
{
	int fd = xsocket(lsa->sa.sa_family, SOCK_STREAM, 0);
	xconnect(fd, &lsa->sa, lsa->len);
	return fd;
}

static char* sockaddr2str(const struct sockaddr *sa, socklen_t salen, int flags)
{
	char host[128];
	char serv[16];
	int rc = getnameinfo(sa, salen,
			host, sizeof(host),
			serv, sizeof(serv),
			flags | NI_NUMERICSERV /* do not resolve port# */
	);
	if (rc) return NULL;
// We probably need to use [%s]:%s for IPv6...
	return xasprintf("%s:%s", host, serv);
}

char* xmalloc_sockaddr2host(const struct sockaddr *sa, socklen_t salen)
{
	return sockaddr2str(sa, salen, 0);
}

char* xmalloc_sockaddr2dotted(const struct sockaddr *sa, socklen_t salen)
{
	return sockaddr2str(sa, salen, NI_NUMERICHOST);
}
