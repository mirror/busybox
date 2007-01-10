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

/* Return network byte ordered port number for a service.
 * If "port" is a number use it as the port.
 * If "port" is a name it is looked up in /etc/services, if it isnt found return
 * default_port */
unsigned bb_lookup_port(const char *port, const char *protocol, unsigned default_port)
{
	unsigned port_nr = htons(default_port);
	if (port) {
		int old_errno;

		/* Since this is a lib function, we're not allowed to reset errno to 0.
		 * Doing so could break an app that is deferring checking of errno. */
		old_errno = errno;
		port_nr = bb_strtou(port, NULL, 10);
		if (errno || port_nr > 65535) {
			struct servent *tserv = getservbyname(port, protocol);
			if (tserv)
				port_nr = tserv->s_port;
		} else {
			port_nr = htons(port_nr);
		}
		errno = old_errno;
	}
	return port_nr;
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


/* So far we do not expose struct and helpers to libbb */
typedef struct len_and_sockaddr {
	int len;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#if ENABLE_FEATURE_IPV6
		struct sockaddr_in6 sin6;
#endif
	};
} len_and_sockaddr;
//extern int xsocket_stream_ip4or6(sa_family_t *fp);
//extern len_and_sockaddr* host2sockaddr(const char *host, int def_port);
//extern len_and_sockaddr* dotted2sockaddr(const char *dotted, int def_port);

/* peer: "1.2.3.4[:port]", "www.google.com[:port]"
 * def_port: if neither of above specifies port #
 */
static len_and_sockaddr* str2sockaddr(const char *host, int def_port, int ai_flags)
{
	int rc;
	len_and_sockaddr *r; // = NULL;
	struct addrinfo *result = NULL;
	const char *org_host = host; /* only for error msg */
	const char *cp;
	char service[sizeof(int)*3 + 1];
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
		host = safe_strncpy(alloca(cp - host + 1), host, cp - host);
		if (ENABLE_FEATURE_IPV6 && *cp != ':')
			cp++; /* skip ']' */
		cp++; /* skip ':' */
	} else {
		utoa_to_buf(def_port, service, sizeof(service));
		cp = service;
	}

	memset(&hint, 0 , sizeof(hint));
	/* hint.ai_family = AF_UNSPEC; - zero anyway */
#if !ENABLE_FEATURE_IPV6
	hint.ai_family = AF_INET; /* do not try to find IPv6 */
#endif
	/* Needed. Or else we will get each address thrice (or more)
	 * for each possible socket type (tcp,udp,raw...): */
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = ai_flags | AI_NUMERICSERV;
	rc = getaddrinfo(host, cp, &hint, &result);
	if (rc || !result)
		bb_error_msg_and_die("bad address '%s'", org_host);
	r = xmalloc(offsetof(len_and_sockaddr, sa) + result->ai_addrlen);
	r->len = result->ai_addrlen;
	memcpy(&r->sa, result->ai_addr, result->ai_addrlen);
	freeaddrinfo(result);
	return r;
}

static len_and_sockaddr* host2sockaddr(const char *host, int def_port)
{
	return str2sockaddr(host, def_port, 0);
}

static len_and_sockaddr* dotted2sockaddr(const char *host, int def_port)
{
	return str2sockaddr(host, def_port, NI_NUMERICHOST);
}

static int xsocket_stream_ip4or6(len_and_sockaddr *lsa)
{
	int fd;
#if ENABLE_FEATURE_IPV6
	fd = socket(AF_INET6, SOCK_STREAM, 0);
	lsa->sa.sa_family = AF_INET6;
	lsa->len = sizeof(struct sockaddr_in6);
	if (fd >= 0)
		return fd;
#endif
	fd = xsocket(AF_INET, SOCK_STREAM, 0);
	lsa->sa.sa_family = AF_INET;
	lsa->len = sizeof(struct sockaddr_in);
	return fd;
}

int create_and_bind_stream_or_die(const char *bindaddr, int port)
{
	int fd;
	len_and_sockaddr *lsa;

	if (bindaddr) {
		lsa = dotted2sockaddr(bindaddr, port);
		/* currently NULL check is in str2sockaddr */
		//if (!lsa)
		//	bb_error_msg_and_die("bad address '%s'", bindaddr);
		/* user specified bind addr dictates family */
		fd = xsocket(lsa->sa.sa_family, SOCK_STREAM, 0);
	} else {
		lsa = xzalloc(offsetof(len_and_sockaddr, sa) +
			USE_FEATURE_IPV6(sizeof(struct sockaddr_in6))
			SKIP_FEATURE_IPV6(sizeof(struct sockaddr_in))
		);
		fd = xsocket_stream_ip4or6(lsa);
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
