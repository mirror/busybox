/* vi: set sw=4 ts=4: */
/*
 * utils.c
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 */

#include "libbb.h"

#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "inet_common.h"

int get_integer(int *val, char *arg, int base)
{
	long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtol(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > INT_MAX || res < INT_MIN)
		return -1;
	*val = res;
	return 0;
}

int get_unsigned(unsigned *val, char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtoul(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > UINT_MAX)
		return -1;
	*val = res;
	return 0;
}

int get_u32(uint32_t * val, char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtoul(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > 0xFFFFFFFFUL)
		return -1;
	*val = res;
	return 0;
}

int get_u16(uint16_t * val, char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtoul(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > 0xFFFF)
		return -1;
	*val = res;
	return 0;
}

int get_u8(uint8_t * val, char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtoul(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > 0xFF)
		return -1;
	*val = res;
	return 0;
}

int get_s16(int16_t * val, char *arg, int base)
{
	long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtol(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > 0x7FFF || res < -0x8000)
		return -1;
	*val = res;
	return 0;
}

int get_s8(int8_t * val, char *arg, int base)
{
	long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtol(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > 0x7F || res < -0x80)
		return -1;
	*val = res;
	return 0;
}

int get_addr_1(inet_prefix * addr, char *name, int family)
{
	char *cp;
	unsigned char *ap = (unsigned char *) addr->data;
	int i;

	memset(addr, 0, sizeof(*addr));

	if (strcmp(name, bb_str_default) == 0 ||
		strcmp(name, "all") == 0 || strcmp(name, "any") == 0) {
		addr->family = family;
		addr->bytelen = (family == AF_INET6 ? 16 : 4);
		addr->bitlen = -1;
		return 0;
	}

	if (strchr(name, ':')) {
		addr->family = AF_INET6;
		if (family != AF_UNSPEC && family != AF_INET6)
			return -1;
		if (inet_pton(AF_INET6, name, addr->data) <= 0)
			return -1;
		addr->bytelen = 16;
		addr->bitlen = -1;
		return 0;
	}

	addr->family = AF_INET;
	if (family != AF_UNSPEC && family != AF_INET)
		return -1;
	addr->bytelen = 4;
	addr->bitlen = -1;
	for (cp = name, i = 0; *cp; cp++) {
		if (*cp <= '9' && *cp >= '0') {
			ap[i] = 10 * ap[i] + (*cp - '0');
			continue;
		}
		if (*cp == '.' && ++i <= 3)
			continue;
		return -1;
	}
	return 0;
}

int get_prefix_1(inet_prefix * dst, char *arg, int family)
{
	int err;
	int plen;
	char *slash;

	memset(dst, 0, sizeof(*dst));

	if (strcmp(arg, bb_str_default) == 0 || strcmp(arg, "any") == 0) {
		dst->family = family;
		dst->bytelen = 0;
		dst->bitlen = 0;
		return 0;
	}

	slash = strchr(arg, '/');
	if (slash)
		*slash = 0;
	err = get_addr_1(dst, arg, family);
	if (err == 0) {
		switch (dst->family) {
		case AF_INET6:
			dst->bitlen = 128;
			break;
		default:
		case AF_INET:
			dst->bitlen = 32;
		}
		if (slash) {
			if (get_integer(&plen, slash + 1, 0) || plen > dst->bitlen) {
				err = -1;
				goto done;
			}
			dst->bitlen = plen;
		}
	}
  done:
	if (slash)
		*slash = '/';
	return err;
}

int get_addr(inet_prefix * dst, char *arg, int family)
{
	if (family == AF_PACKET) {
		bb_error_msg_and_die("\"%s\" may be inet address, but it is not allowed in this context", arg);
	}
	if (get_addr_1(dst, arg, family)) {
		bb_error_msg_and_die("an inet address is expected rather than \"%s\"", arg);
	}
	return 0;
}

int get_prefix(inet_prefix * dst, char *arg, int family)
{
	if (family == AF_PACKET) {
		bb_error_msg_and_die("\"%s\" may be inet address, but it is not allowed in this context", arg);
	}
	if (get_prefix_1(dst, arg, family)) {
		bb_error_msg_and_die("an inet address is expected rather than \"%s\"", arg);
	}
	return 0;
}

uint32_t get_addr32(char *name)
{
	inet_prefix addr;

	if (get_addr_1(&addr, name, AF_INET)) {
		bb_error_msg_and_die("an IP address is expected rather than \"%s\"", name);
	}
	return addr.data[0];
}

void incomplete_command(void)
{
	bb_error_msg("command line is not complete, try option \"help\"");
	exit(-1);
}

void invarg(const char * const arg, const char * const opt)
{
	bb_error_msg(bb_msg_invalid_arg, arg, opt);
	exit(-1);
}

void duparg(char *key, char *arg)
{
	bb_error_msg("duplicate \"%s\": \"%s\" is the second value", key, arg);
	exit(-1);
}

void duparg2(char *key, char *arg)
{
	bb_error_msg("either \"%s\" is duplicate, or \"%s\" is garbage", key, arg);
	exit(-1);
}

int matches(char *cmd, char *pattern)
{
	int len = strlen(cmd);

	return strncmp(pattern, cmd, len);
}

int inet_addr_match(inet_prefix * a, inet_prefix * b, int bits)
{
	uint32_t *a1 = a->data;
	uint32_t *a2 = b->data;
	int words = bits >> 0x05;

	bits &= 0x1f;

	if (words)
		if (memcmp(a1, a2, words << 2))
			return -1;

	if (bits) {
		uint32_t w1, w2;
		uint32_t mask;

		w1 = a1[words];
		w2 = a2[words];

		mask = htonl((0xffffffff) << (0x20 - bits));

		if ((w1 ^ w2) & mask)
			return 1;
	}

	return 0;
}

const char *rt_addr_n2a(int af, int ATTRIBUTE_UNUSED len,
		void *addr, char *buf, int buflen)
{
	switch (af) {
	case AF_INET:
	case AF_INET6:
		return inet_ntop(af, addr, buf, buflen);
	default:
		return "???";
	}
}


const char *format_host(int af, int len, void *addr, char *buf, int buflen)
{
#ifdef RESOLVE_HOSTNAMES
	if (resolve_hosts) {
		struct hostent *h_ent;

		if (len <= 0) {
			switch (af) {
			case AF_INET:
				len = 4;
				break;
			case AF_INET6:
				len = 16;
				break;
			default:;
			}
		}
		if (len > 0 && (h_ent = gethostbyaddr(addr, len, af)) != NULL) {
			snprintf(buf, buflen - 1, "%s", h_ent->h_name);
			return buf;
		}
	}
#endif
	return rt_addr_n2a(af, len, addr, buf, buflen);
}
