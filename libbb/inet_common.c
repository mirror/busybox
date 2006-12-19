/* vi: set sw=4 ts=4: */
/*
 * stolen from net-tools-1.59 and stripped down for busybox by
 *                      Erik Andersen <andersen@codepoet.org>
 *
 * Heavily modified by Manuel Novoa III       Mar 12, 2001
 *
 * Version:     $Id: inet_common.c,v 1.8 2004/03/10 07:42:38 mjn3 Exp $
 *
 */

#include "libbb.h"
#include "inet_common.h"

int INET_resolve(const char *name, struct sockaddr_in *s_in, int hostfirst)
{
	struct hostent *hp;
	struct netent *np;

	/* Grmpf. -FvK */
	s_in->sin_family = AF_INET;
	s_in->sin_port = 0;

	/* Default is special, meaning 0.0.0.0. */
	if (!strcmp(name, bb_str_default)) {
		s_in->sin_addr.s_addr = INADDR_ANY;
		return 1;
	}
	/* Look to see if it's a dotted quad. */
	if (inet_aton(name, &s_in->sin_addr)) {
		return 0;
	}
	/* If we expect this to be a hostname, try hostname database first */
#ifdef DEBUG
	if (hostfirst) {
		bb_error_msg("gethostbyname (%s)", name);
	}
#endif
	if (hostfirst && (hp = gethostbyname(name)) != (struct hostent *) NULL) {
		memcpy((char *) &s_in->sin_addr, (char *) hp->h_addr_list[0],
			   sizeof(struct in_addr));
		return 0;
	}
	/* Try the NETWORKS database to see if this is a known network. */
#ifdef DEBUG
	bb_error_msg("getnetbyname (%s)", name);
#endif
	if ((np = getnetbyname(name)) != (struct netent *) NULL) {
		s_in->sin_addr.s_addr = htonl(np->n_net);
		return 1;
	}
	if (hostfirst) {
		/* Don't try again */
		return -1;
	}
#ifdef DEBUG
	res_init();
	_res.options |= RES_DEBUG;
#endif

#ifdef DEBUG
	bb_error_msg("gethostbyname (%s)", name);
#endif
	if ((hp = gethostbyname(name)) == (struct hostent *) NULL) {
		return -1;
	}
	memcpy((char *) &s_in->sin_addr, (char *) hp->h_addr_list[0],
		   sizeof(struct in_addr));

	return 0;
}

/* cache */
struct addr {
	struct sockaddr_in addr;
	char *name;
	int host;
	struct addr *next;
};

static struct addr *INET_nn = NULL;	/* addr-to-name cache           */

/* numeric: & 0x8000: default instead of *,
 *          & 0x4000: host instead of net,
 *          & 0x0fff: don't resolve
 */
int INET_rresolve(char *name, size_t len, struct sockaddr_in *s_in,
				  int numeric, unsigned int netmask)
{
	struct hostent *ent;
	struct netent *np;
	struct addr *pn;
	unsigned long ad, host_ad;
	int host = 0;

	/* Grmpf. -FvK */
	if (s_in->sin_family != AF_INET) {
#ifdef DEBUG
		bb_error_msg("rresolve: unsupport address family %d !",
				  s_in->sin_family);
#endif
		errno = EAFNOSUPPORT;
		return -1;
	}
	ad = (unsigned long) s_in->sin_addr.s_addr;
#ifdef DEBUG
	bb_error_msg("rresolve: %08lx, mask %08x, num %08x", ad, netmask, numeric);
#endif
	if (ad == INADDR_ANY) {
		if ((numeric & 0x0FFF) == 0) {
			if (numeric & 0x8000)
				safe_strncpy(name, bb_str_default, len);
			else
				safe_strncpy(name, "*", len);
			return 0;
		}
	}
	if (numeric & 0x0FFF) {
		safe_strncpy(name, inet_ntoa(s_in->sin_addr), len);
		return 0;
	}

	if ((ad & (~netmask)) != 0 || (numeric & 0x4000))
		host = 1;
	pn = INET_nn;
	while (pn != NULL) {
		if (pn->addr.sin_addr.s_addr == ad && pn->host == host) {
			safe_strncpy(name, pn->name, len);
#ifdef DEBUG
			bb_error_msg("rresolve: found %s %08lx in cache",
					  (host ? "host" : "net"), ad);
#endif
			return 0;
		}
		pn = pn->next;
	}

	host_ad = ntohl(ad);
	np = NULL;
	ent = NULL;
	if (host) {
#ifdef DEBUG
		bb_error_msg("gethostbyaddr (%08lx)", ad);
#endif
		ent = gethostbyaddr((char *) &ad, 4, AF_INET);
		if (ent != NULL) {
			safe_strncpy(name, ent->h_name, len);
		}
	} else {
#ifdef DEBUG
		bb_error_msg("getnetbyaddr (%08lx)", host_ad);
#endif
		np = getnetbyaddr(host_ad, AF_INET);
		if (np != NULL) {
			safe_strncpy(name, np->n_name, len);
		}
	}
	if ((ent == NULL) && (np == NULL)) {
		safe_strncpy(name, inet_ntoa(s_in->sin_addr), len);
	}
	pn = xmalloc(sizeof(struct addr));
	pn->addr = *s_in;
	pn->next = INET_nn;
	pn->host = host;
	pn->name = xstrdup(name);
	INET_nn = pn;

	return 0;
}

#ifdef CONFIG_FEATURE_IPV6

int INET6_resolve(const char *name, struct sockaddr_in6 *sin6)
{
	struct addrinfo req, *ai;
	int s;

	memset(&req, '\0', sizeof req);
	req.ai_family = AF_INET6;
	s = getaddrinfo(name, NULL, &req, &ai);
	if (s) {
		bb_error_msg("getaddrinfo: %s: %d", name, s);
		return -1;
	}
	memcpy(sin6, ai->ai_addr, sizeof(struct sockaddr_in6));

	freeaddrinfo(ai);

	return 0;
}

#ifndef IN6_IS_ADDR_UNSPECIFIED
# define IN6_IS_ADDR_UNSPECIFIED(a) \
	(((uint32_t *) (a))[0] == 0 && ((uint32_t *) (a))[1] == 0 && \
	 ((uint32_t *) (a))[2] == 0 && ((uint32_t *) (a))[3] == 0)
#endif


int INET6_rresolve(char *name, size_t len, struct sockaddr_in6 *sin6,
				   int numeric)
{
	int s;

	/* Grmpf. -FvK */
	if (sin6->sin6_family != AF_INET6) {
#ifdef DEBUG
		bb_error_msg("rresolve: unsupport address family %d!",
				  sin6->sin6_family);
#endif
		errno = EAFNOSUPPORT;
		return -1;
	}
	if (numeric & 0x7FFF) {
		inet_ntop(AF_INET6, &sin6->sin6_addr, name, len);
		return 0;
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		if (numeric & 0x8000) {
			strcpy(name, bb_str_default);
		} else {
			name[0] = '*';
			name[1] = '\0';
		}
		return 0;
	}

	s = getnameinfo((struct sockaddr *) sin6, sizeof(struct sockaddr_in6), name, len, NULL, 0, 0);
	if (s) {
		bb_error_msg("getnameinfo failed");
		return -1;
	}
	return 0;
}

#endif							/* CONFIG_FEATURE_IPV6 */
