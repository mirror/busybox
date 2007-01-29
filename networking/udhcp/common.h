/* vi: set sw=4 ts=4: */
/* common.h
 *
 * Russ Dill <Russ.Dill@asu.edu> September 2001
 * Rewritten by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#ifndef _COMMON_H
#define _COMMON_H

#include "busybox.h"

#define DEFAULT_SCRIPT  "/usr/share/udhcpc/default.script"

#define COMBINED_BINARY


/*** packet.h ***/

#include <netinet/udp.h>
#include <netinet/ip.h>

struct dhcpMessage {
	uint8_t op;
	uint8_t htype;
	uint8_t hlen;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	uint32_t ciaddr;
	uint32_t yiaddr;
	uint32_t siaddr;
	uint32_t giaddr;
	uint8_t chaddr[16];
	uint8_t sname[64];
	uint8_t file[128];
	uint32_t cookie;
	uint8_t options[308]; /* 312 - cookie */
};

struct udp_dhcp_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct dhcpMessage data;
};

void udhcp_init_header(struct dhcpMessage *packet, char type);
int udhcp_get_packet(struct dhcpMessage *packet, int fd);
uint16_t udhcp_checksum(void *addr, int count);
int udhcp_raw_packet(struct dhcpMessage *payload,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port, uint8_t *dest_arp, int ifindex);
int udhcp_kernel_packet(struct dhcpMessage *payload,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port);


/**/

void udhcp_background(const char *pidfile);
void udhcp_start_log_and_pid(const char *pidfile);

void udhcp_run_script(struct dhcpMessage *packet, const char *name);

// Still need to clean these up...

/* from pidfile.h */
#define pidfile_acquire		udhcp_pidfile_acquire
#define pidfile_write_release	udhcp_pidfile_write_release
/* from options.h */
#define get_option		udhcp_get_option
#define end_option		udhcp_end_option
#define add_option_string	udhcp_add_option_string
#define add_simple_option	udhcp_add_simple_option
#define option_lengths		udhcp_option_lengths
/* from socket.h */
#define listen_socket		udhcp_listen_socket
#define read_interface		udhcp_read_interface
/* from dhcpc.h */
#define client_config		udhcp_client_config
/* from dhcpd.h */
#define server_config		udhcp_server_config

long uptime(void);
void udhcp_sp_setup(void);
int udhcp_sp_fd_set(fd_set *rfds, int extra_fd);
int udhcp_sp_read(fd_set *rfds);
int raw_socket(int ifindex);
int read_interface(const char *interface, int *ifindex, uint32_t *addr, uint8_t *arp);
int listen_socket(uint32_t ip, int port, const char *inf);
int pidfile_acquire(const char *pidfile);
void pidfile_write_release(int pid_fd);
int arpping(uint32_t yiaddr, uint32_t ip, uint8_t *arp, char *interface);

#if ENABLE_FEATURE_UDHCP_DEBUG
# define DEBUG(str, args...) bb_info_msg(str, ## args)
#else
# define DEBUG(str, args...) do {;} while (0)
#endif

#endif
