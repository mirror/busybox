/* vi: set sw=4 ts=4: */
/*
 * stolen from net-tools-1.59 and stripped down for busybox by
 *                      Erik Andersen <andersen@codepoet.org>
 *
 * Heavily modified by Manuel Novoa III       Mar 12, 2001
 *
 */

#include "platform.h"

/* hostfirst!=0 If we expect this to be a hostname,
   try hostname database first
 */
int INET_resolve(const char *name, struct sockaddr_in *s_in, int hostfirst);

/* numeric: & 0x8000: "default" instead of "*",
 *          & 0x4000: host instead of net,
 *          & 0x0fff: don't resolve
 */

int INET6_resolve(const char *name, struct sockaddr_in6 *sin6);

/* These return malloced string */
char *INET_rresolve(struct sockaddr_in *s_in, int numeric, uint32_t netmask);
char *INET6_rresolve(struct sockaddr_in6 *sin6, int numeric);
