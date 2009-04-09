/* vi: set sw=4 ts=4: */
/* common.h
 *
 * Russ Dill <Russ.Dill@asu.edu> September 2001
 * Rewritten by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#ifndef UDHCP_COMMON_H
#define UDHCP_COMMON_H 1

#include "libbb.h"
#include <netinet/udp.h>
#include <netinet/ip.h>

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

#define DEFAULT_SCRIPT   CONFIG_UDHCPC_DEFAULT_SCRIPT

extern const uint8_t MAC_BCAST_ADDR[6]; /* six all-ones */

/*** packet.h ***/

#define DHCP_OPTIONS_BUFSIZE  308

struct dhcpMessage {
	uint8_t op;      /* 1 = BOOTREQUEST, 2 = BOOTREPLY */
	uint8_t htype;   /* hardware address type. 1 = 10mb ethernet */
	uint8_t hlen;    /* hardware address length */
	uint8_t hops;    /* used by relay agents only */
	uint32_t xid;    /* unique id */
	uint16_t secs;   /* elapsed since client began acquisition/renewal */
	uint16_t flags;  /* only one flag so far: */
#define BROADCAST_FLAG 0x8000 /* "I need broadcast replies" */
	uint32_t ciaddr; /* client IP (if client is in BOUND, RENEW or REBINDING state) */
	uint32_t yiaddr; /* 'your' (client) IP address */
	uint32_t siaddr; /* IP address of next server to use in bootstrap,
	                  * returned in DHCPOFFER, DHCPACK by server */
	uint32_t giaddr; /* relay agent IP address */
	uint8_t chaddr[16];/* link-layer client hardware address (MAC) */
	uint8_t sname[64]; /* server host name (ASCIZ) */
	uint8_t file[128]; /* boot file name (ASCIZ) */
	uint32_t cookie;   /* fixed first four option bytes (99,130,83,99 dec) */
	uint8_t options[DHCP_OPTIONS_BUFSIZE + CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS];
} PACKED;

struct udp_dhcp_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct dhcpMessage data;
} PACKED;

/* Let's see whether compiler understood us right */
struct BUG_bad_sizeof_struct_udp_dhcp_packet {
	char BUG_bad_sizeof_struct_udp_dhcp_packet
		[(sizeof(struct udp_dhcp_packet) != 576 + CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS) ? -1 : 1];
};

uint16_t udhcp_checksum(void *addr, int count) FAST_FUNC;

void udhcp_init_header(struct dhcpMessage *packet, char type) FAST_FUNC;

/*int udhcp_recv_raw_packet(struct dhcpMessage *payload, int fd); - in dhcpc.h */
int udhcp_recv_kernel_packet(struct dhcpMessage *packet, int fd) FAST_FUNC;

int udhcp_send_raw_packet(struct dhcpMessage *payload,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port, const uint8_t *dest_arp,
		int ifindex) FAST_FUNC;

int udhcp_send_kernel_packet(struct dhcpMessage *payload,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port) FAST_FUNC;


/**/

void udhcp_run_script(struct dhcpMessage *packet, const char *name) FAST_FUNC;

// Still need to clean these up...

/* from options.h */
#define get_option		udhcp_get_option
#define end_option		udhcp_end_option
#define add_option_string	udhcp_add_option_string
#define add_simple_option	udhcp_add_simple_option

void udhcp_sp_setup(void) FAST_FUNC;
int udhcp_sp_fd_set(fd_set *rfds, int extra_fd) FAST_FUNC;
int udhcp_sp_read(const fd_set *rfds) FAST_FUNC;
int udhcp_read_interface(const char *interface, int *ifindex, uint32_t *addr, uint8_t *arp) FAST_FUNC;
int udhcp_raw_socket(int ifindex) FAST_FUNC;
int udhcp_listen_socket(/*uint32_t ip,*/ int port, const char *inf) FAST_FUNC;
/* Returns 1 if no reply received */
int arpping(uint32_t test_ip, uint32_t from_ip, uint8_t *from_mac, const char *interface) FAST_FUNC;

#if ENABLE_UDHCP_DEBUG
# define DEBUG(str, args...) bb_info_msg("### " str, ## args)
#else
# define DEBUG(str, args...) do {;} while (0)
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif
