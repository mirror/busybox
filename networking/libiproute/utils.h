#ifndef __UTILS_H__
#define __UTILS_H__ 1

#include <asm/types.h>
#include <resolv.h>

#include "libnetlink.h"
#include "ll_map.h"
#include "rtm_map.h"

extern int preferred_family;
extern int show_stats;
extern int show_details;
extern int show_raw;
extern int resolve_hosts;
extern int oneline;
extern char * _SL_;

#ifndef IPPROTO_ESP
#define IPPROTO_ESP	50
#endif
#ifndef IPPROTO_AH
#define IPPROTO_AH	51
#endif

#define SPRINT_BSIZE 64
#define SPRINT_BUF(x)	char x[SPRINT_BSIZE]

extern void incomplete_command(void) __attribute__((noreturn));

#define NEXT_ARG() do { argv++; if (--argc <= 0) incomplete_command(); } while(0)

typedef struct
{
	__u8 family;
	__u8 bytelen;
	__s16 bitlen;
	__u32 data[4];
} inet_prefix;

#define DN_MAXADDL 20
#ifndef AF_DECnet
#define AF_DECnet 12
#endif

struct dn_naddr
{
        unsigned short          a_len;
        unsigned char a_addr[DN_MAXADDL];
};

#define IPX_NODE_LEN 6

struct ipx_addr {
	uint32_t ipx_net;
	uint8_t  ipx_node[IPX_NODE_LEN];
};

extern __u32 get_addr32(char *name);
extern int get_addr_1(inet_prefix *dst, char *arg, int family);
extern int get_prefix_1(inet_prefix *dst, char *arg, int family);
extern int get_addr(inet_prefix *dst, char *arg, int family);
extern int get_prefix(inet_prefix *dst, char *arg, int family);

extern int get_integer(int *val, char *arg, int base);
extern int get_unsigned(unsigned *val, char *arg, int base);
#define get_byte get_u8
#define get_ushort get_u16
#define get_short get_s16
extern int get_u32(__u32 *val, char *arg, int base);
extern int get_u16(__u16 *val, char *arg, int base);
extern int get_s16(__s16 *val, char *arg, int base);
extern int get_u8(__u8 *val, char *arg, int base);
extern int get_s8(__s8 *val, char *arg, int base);

extern const char *format_host(int af, int len, void *addr, char *buf, int buflen);
extern const char *rt_addr_n2a(int af, int len, void *addr, char *buf, int buflen);

void invarg(char *, char *) __attribute__((noreturn));
void duparg(char *, char *) __attribute__((noreturn));
void duparg2(char *, char *) __attribute__((noreturn));
int matches(char *arg, char *pattern);
extern int inet_addr_match(inet_prefix *a, inet_prefix *b, int bits);

const char *dnet_ntop(int af, const void *addr, char *str, size_t len);
int dnet_pton(int af, const char *src, void *addr);

const char *ipx_ntop(int af, const void *addr, char *str, size_t len);
int ipx_pton(int af, const char *src, void *addr);

extern int __iproute2_hz_internal;
extern int __get_hz(void);

static __inline__ int get_hz(void)
{
	if (__iproute2_hz_internal == 0)
		__iproute2_hz_internal = __get_hz();
	return __iproute2_hz_internal;
}

#endif /* __UTILS_H__ */
