/* dhcpd.c
 *
 * udhcp DHCP client
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 * 
 * Converted to busybox by Glenn McGrath August 2002
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/wait.h>

#if __GLIBC__ >=2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif
#include "libbb.h"

static int state;
static unsigned long requested_ip;	/* = 0 */
static unsigned long server_addr;
static unsigned long timeout;
static int packet_num;	/* = 0 */
static int fd_main;

/* #define DEBUG */

#define VERSION "0.9.7"

#define LISTEN_NONE 0
#define LISTEN_KERNEL 1
#define LISTEN_RAW 2
static int listen_mode;

#define DEFAULT_SCRIPT	"/usr/share/udhcpc/default.script"

#define DHCP_END		0xFF
#define TYPE_MASK	0x0F
#define BROADCAST_FLAG		0x8000

#define SERVER_PORT		67

#define DHCP_MAGIC		0x63825363

#define BOOTREQUEST		1
#define BOOTREPLY		2

#define ETH_10MB		1
#define ETH_10MB_LEN		6

#define OPTION_FIELD		0
#define FILE_FIELD		1
#define SNAME_FIELD		2

#define INIT_SELECTING	0
#define REQUESTING	1
#define BOUND		2
#define RENEWING	3
#define REBINDING	4
#define INIT_REBOOT	5
#define RENEW_REQUESTED 6
#define RELEASED	7

#define CLIENT_PORT		68

#define DHCPDISCOVER		1
#define DHCPOFFER		2
#define DHCPREQUEST		3
#define DHCPDECLINE		4
#define DHCPACK			5
#define DHCPNAK			6
#define DHCPRELEASE		7
#define DHCPINFORM		8

/* DHCP option codes (partial list) */
#define DHCP_PADDING		0x00
#define DHCP_SUBNET		0x01
#define DHCP_TIME_OFFSET	0x02
#define DHCP_ROUTER		0x03
#define DHCP_TIME_SERVER	0x04
#define DHCP_NAME_SERVER	0x05
#define DHCP_DNS_SERVER		0x06
#define DHCP_LOG_SERVER		0x07
#define DHCP_COOKIE_SERVER	0x08
#define DHCP_LPR_SERVER		0x09
#define DHCP_HOST_NAME		0x0c
#define DHCP_BOOT_SIZE		0x0d
#define DHCP_DOMAIN_NAME	0x0f
#define DHCP_SWAP_SERVER	0x10
#define DHCP_ROOT_PATH		0x11
#define DHCP_IP_TTL		0x17
#define DHCP_MTU		0x1a
#define DHCP_BROADCAST		0x1c
#define DHCP_NTP_SERVER		0x2a
#define DHCP_WINS_SERVER	0x2c
#define DHCP_REQUESTED_IP	0x32
#define DHCP_LEASE_TIME		0x33
#define DHCP_OPTION_OVER	0x34
#define DHCP_MESSAGE_TYPE	0x35
#define DHCP_SERVER_ID		0x36
#define DHCP_PARAM_REQ		0x37
#define DHCP_MESSAGE		0x38
#define DHCP_MAX_SIZE		0x39
#define DHCP_T1			0x3a
#define DHCP_T2			0x3b
#define DHCP_VENDOR		0x3c
#define DHCP_CLIENT_ID		0x3d

/* miscellaneous defines */
#define MAC_BCAST_ADDR		(unsigned char *) "\xff\xff\xff\xff\xff\xff"
#define OPT_CODE 0
#define OPT_LEN 1
#define OPT_DATA 2

enum {
	OPTION_IP = 1,
	OPTION_IP_PAIR,
	OPTION_STRING,
	OPTION_BOOLEAN,
	OPTION_U8,
	OPTION_U16,
	OPTION_S16,
	OPTION_U32,
	OPTION_S32
};

#define OPTION_REQ	0x10	/* have the client request this option */
#define OPTION_LIST	0x20	/* There can be a list of 1 or more of these */

#ifdef SYSLOG
# define LOG(level, str, args...) do { printf(str, ## args); \
				printf("\n"); \
				syslog(level, str, ## args); } while(0)
# define OPEN_LOG(name) openlog(name, 0, 0)
# define CLOSE_LOG() closelog()
#else
# define LOG_EMERG	"EMERGENCY!"
# define LOG_ALERT	"ALERT!"
# define LOG_CRIT	"critical!"
# define LOG_WARNING	"warning"
# define LOG_ERR	"error"
# define LOG_INFO	"info"
# define LOG_DEBUG	"debug"
# define LOG(level, str, args...) do { printf("%s, " str "\n", level, ## args); } while(0)
# define OPEN_LOG(name)
# define CLOSE_LOG()
#endif

#ifdef DEBUG
# undef DEBUG
# define DEBUG(level, str, args...) LOG(level, str, ## args)
# define DEBUGGING
#else
# define DEBUG(level, str, args...)
#endif

struct dhcpMessage {
	u_int8_t op;
	u_int8_t htype;
	u_int8_t hlen;
	u_int8_t hops;
	u_int32_t xid;
	u_int16_t secs;
	u_int16_t flags;
	u_int32_t ciaddr;
	u_int32_t yiaddr;
	u_int32_t siaddr;
	u_int32_t giaddr;
	u_int8_t chaddr[16];
	u_int8_t sname[64];
	u_int8_t file[128];
	u_int32_t cookie;
	u_int8_t options[308];	/* 312 - cookie */
};

struct client_config_t {
	char foreground;	/* Do not fork */
	char quit_after_lease;	/* Quit after obtaining lease */
	char abort_if_no_lease;	/* Abort if no lease */
	char *interface;	/* The name of the interface to use */
	char *pidfile;		/* Optionally store the process ID */
	char *script;		/* User script to run at dhcp events */
	unsigned char *clientid;	/* Optional client id to use */
	unsigned char *hostname;	/* Optional hostname to use */
	int ifindex;		/* Index number of the interface to use */
	unsigned char arp[6];	/* Our arp address */
};

struct client_config_t client_config = {
	/* Default options. */
	abort_if_no_lease:0,
	foreground:0,
	quit_after_lease:0,
	interface:"eth0",
	pidfile:NULL,
	script:DEFAULT_SCRIPT,
	clientid:NULL,
	hostname:NULL,
	ifindex:0,
	arp:"\0\0\0\0\0\0",	/* appease gcc-3.0 */
};

struct dhcp_option {
	char name[10];
	char flags;
	unsigned char code;
};

struct udp_dhcp_packet {
	struct iphdr ip;
	struct udphdr udp;
	struct dhcpMessage data;
};

static const struct dhcp_option options[] = {
	/* name[10] flags                   code */
	{"subnet", OPTION_IP | OPTION_REQ, 0x01},
	{"timezone", OPTION_S32, 0x02},
	{"router", OPTION_IP | OPTION_LIST | OPTION_REQ, 0x03},
	{"timesvr", OPTION_IP | OPTION_LIST, 0x04},
	{"namesvr", OPTION_IP | OPTION_LIST, 0x05},
	{"dns", OPTION_IP | OPTION_LIST | OPTION_REQ, 0x06},
	{"logsvr", OPTION_IP | OPTION_LIST, 0x07},
	{"cookiesvr", OPTION_IP | OPTION_LIST, 0x08},
	{"lprsvr", OPTION_IP | OPTION_LIST, 0x09},
	{"hostname", OPTION_STRING | OPTION_REQ, 0x0c},
	{"bootsize", OPTION_U16, 0x0d},
	{"domain", OPTION_STRING | OPTION_REQ, 0x0f},
	{"swapsvr", OPTION_IP, 0x10},
	{"rootpath", OPTION_STRING, 0x11},
	{"ipttl", OPTION_U8, 0x17},
	{"mtu", OPTION_U16, 0x1a},
	{"broadcast", OPTION_IP | OPTION_REQ, 0x1c},
	{"ntpsrv", OPTION_IP | OPTION_LIST, 0x2a},
	{"wins", OPTION_IP | OPTION_LIST, 0x2c},
	{"requestip", OPTION_IP, 0x32},
	{"lease", OPTION_U32, 0x33},
	{"dhcptype", OPTION_U8, 0x35},
	{"serverid", OPTION_IP, 0x36},
	{"message", OPTION_STRING, 0x38},
	{"tftp", OPTION_STRING, 0x42},
	{"bootfile", OPTION_STRING, 0x43},
	{"", 0x00, 0x00}
};

/* Lengths of the different option types */
static const unsigned char option_lengths[] = {
	[OPTION_IP] = 4,
	[OPTION_IP_PAIR] = 8,
	[OPTION_BOOLEAN] = 1,
	[OPTION_STRING] = 1,
	[OPTION_U8] = 1,
	[OPTION_U16] = 2,
	[OPTION_S16] = 2,
	[OPTION_U32] = 4,
	[OPTION_S32] = 4
};

/* get a rough idea of how long an option will be (rounding up...) */
static const unsigned char max_option_length[] = {
	[OPTION_IP] = sizeof("255.255.255.255 "),
	[OPTION_IP_PAIR] = sizeof("255.255.255.255 ") * 2,
	[OPTION_STRING] = 1,
	[OPTION_BOOLEAN] = sizeof("yes "),
	[OPTION_U8] = sizeof("255 "),
	[OPTION_U16] = sizeof("65535 "),
	[OPTION_S16] = sizeof("-32768 "),
	[OPTION_U32] = sizeof("4294967295 "),
	[OPTION_S32] = sizeof("-2147483684 "),
};

/* return the position of the 'end' option (no bounds checking) */
static int end_option(unsigned char *optionptr)
{
	int i = 0;

	while (optionptr[i] != DHCP_END) {
		if (optionptr[i] == DHCP_PADDING)
			i++;
		else
			i += optionptr[i + OPT_LEN] + 2;
	}
	return i;
}

/* add an option string to the options (an option string contains an option code,
 * length, then data) */
static int add_option_string(unsigned char *optionptr, unsigned char *string)
{
	int end = end_option(optionptr);

	/* end position + string length + option code/length + end option */
	if (end + string[OPT_LEN] + 2 + 1 >= 308) {
		LOG(LOG_ERR, "Option 0x%02x did not fit into the packet!",
			string[OPT_CODE]);
		return 0;
	}
	DEBUG(LOG_INFO, "adding option 0x%02x", string[OPT_CODE]);
	memcpy(optionptr + end, string, string[OPT_LEN] + 2);
	optionptr[end + string[OPT_LEN] + 2] = DHCP_END;
	return string[OPT_LEN] + 2;
}

/* add a one to four byte option to a packet */
static int add_simple_option(unsigned char *optionptr, unsigned char code,
							 u_int32_t data)
{
	char length = 0;
	int i;
	unsigned char option[2 + 4];
	unsigned char *u8;
	u_int16_t *u16;
	u_int32_t *u32;
	u_int32_t aligned;

	u8 = (unsigned char *) &aligned;
	u16 = (u_int16_t *) & aligned;
	u32 = &aligned;

	for (i = 0; options[i].code; i++)
		if (options[i].code == code) {
			length = option_lengths[options[i].flags & TYPE_MASK];
		}

	if (!length) {
		DEBUG(LOG_ERR, "Could not add option 0x%02x", code);
		return 0;
	}

	option[OPT_CODE] = code;
	option[OPT_LEN] = length;

	switch (length) {
	case 1:
		*u8 = data;
		break;
	case 2:
		*u16 = data;
		break;
	case 4:
		*u32 = data;
		break;
	}

	memcpy(option + 2, &aligned, length);
	return add_option_string(optionptr, option);
}

static u_int16_t checksum(void *addr, int count)
{
	/* Compute Internet Checksum for "count" bytes
	 *         beginning at location "addr".
	 */
	register int32_t sum = 0;
	u_int16_t *source = (u_int16_t *) addr;

	while (count > 1) {
		/*  This is the inner loop */
		sum += *source++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if (count > 0) {
		sum += *(unsigned char *) source;
	}

	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}

	return ~sum;
}

/* Constuct a ip/udp header for a packet, and specify the source and dest hardware address */
static int raw_packet(struct dhcpMessage *payload, u_int32_t source_ip,
					  int source_port, u_int32_t dest_ip, int dest_port,
					  unsigned char *dest_arp, int ifindex)
{
	int l_fd;
	int result;
	struct sockaddr_ll dest;
	struct udp_dhcp_packet packet;

	if ((l_fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
		DEBUG(LOG_ERR, "socket call failed: %s", strerror(errno));
		return -1;
	}

	memset(&dest, 0, sizeof(dest));
	memset(&packet, 0, sizeof(packet));

	dest.sll_family = AF_PACKET;
	dest.sll_protocol = htons(ETH_P_IP);
	dest.sll_ifindex = ifindex;
	dest.sll_halen = 6;
	memcpy(dest.sll_addr, dest_arp, 6);
	if (bind(l_fd, (struct sockaddr *) &dest, sizeof(struct sockaddr_ll)) < 0) {
		DEBUG(LOG_ERR, "bind call failed: %s", strerror(errno));
		close(l_fd);
		return -1;
	}

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source_ip;
	packet.ip.daddr = dest_ip;
	packet.udp.source = htons(source_port);
	packet.udp.dest = htons(dest_port);
	packet.udp.len = htons(sizeof(packet.udp) + sizeof(struct dhcpMessage));	/* cheat on the psuedo-header */
	packet.ip.tot_len = packet.udp.len;
	memcpy(&(packet.data), payload, sizeof(struct dhcpMessage));
	packet.udp.check = checksum(&packet, sizeof(struct udp_dhcp_packet));

	packet.ip.tot_len = htons(sizeof(struct udp_dhcp_packet));
	packet.ip.ihl = sizeof(packet.ip) >> 2;
	packet.ip.version = IPVERSION;
	packet.ip.ttl = IPDEFTTL;
	packet.ip.check = checksum(&(packet.ip), sizeof(packet.ip));

	result =
		sendto(l_fd, &packet, sizeof(struct udp_dhcp_packet), 0,
			   (struct sockaddr *) &dest, sizeof(dest));
	if (result <= 0) {
		DEBUG(LOG_ERR, "write on socket failed: %s", strerror(errno));
	}
	close(l_fd);
	return result;
}

/* Let the kernel do all the work for packet generation */
static int kernel_packet(struct dhcpMessage *payload, u_int32_t source_ip,
						 int source_port, u_int32_t dest_ip, int dest_port)
{
	int n = 1;
	int l_fd, result;
	struct sockaddr_in client;

	if ((l_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		return -1;
	}

	if (setsockopt(l_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof(n)) ==
		-1) {
		return -1;
	}

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(source_port);
	client.sin_addr.s_addr = source_ip;

	if (bind(l_fd, (struct sockaddr *) &client, sizeof(struct sockaddr)) ==
		-1) {
		return -1;
	}

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(dest_port);
	client.sin_addr.s_addr = dest_ip;

	if (connect(l_fd, (struct sockaddr *) &client, sizeof(struct sockaddr)) ==
		-1) {
		return -1;
	}

	result = write(l_fd, payload, sizeof(struct dhcpMessage));
	close(l_fd);
	return result;
}

/* initialize a packet with the proper defaults */
static void init_packet(struct dhcpMessage *packet, char type)
{
	struct vendor {
		char vendor, length;
		char str[sizeof("udhcp " VERSION)];
	}
	vendor_id = {
	DHCP_VENDOR, sizeof("udhcp " VERSION) - 1, "udhcp " VERSION};

	memset(packet, 0, sizeof(struct dhcpMessage));
	switch (type) {
	case DHCPDISCOVER:
	case DHCPREQUEST:
	case DHCPRELEASE:
	case DHCPINFORM:
		packet->op = BOOTREQUEST;
		break;
	case DHCPOFFER:
	case DHCPACK:
	case DHCPNAK:
		packet->op = BOOTREPLY;
	}
	packet->htype = ETH_10MB;
	packet->hlen = ETH_10MB_LEN;
	packet->cookie = htonl(DHCP_MAGIC);
	packet->options[0] = DHCP_END;
	add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);

	memcpy(packet->chaddr, client_config.arp, 6);
	add_option_string(packet->options, client_config.clientid);
	if (client_config.hostname) {
		add_option_string(packet->options, client_config.hostname);
	}
	add_option_string(packet->options, (unsigned char *) &vendor_id);
}


/* Add a paramater request list for stubborn DHCP servers. Pull the data
 * from the struct in options.c. Don't do bounds checking here because it
 * goes towards the head of the packet. */
static void add_requests(struct dhcpMessage *packet)
{
	int end = end_option(packet->options);
	int i, len = 0;

	packet->options[end + OPT_CODE] = DHCP_PARAM_REQ;
	for (i = 0; options[i].code; i++) {
		if (options[i].flags & OPTION_REQ) {
			packet->options[end + OPT_DATA + len++] = options[i].code;
		}
	}
	packet->options[end + OPT_LEN] = len;
	packet->options[end + OPT_DATA + len] = DHCP_END;

}

/* Broadcast a DHCP discover packet to the network, with an optionally requested IP */
static inline int send_discover(unsigned long xid, unsigned long requested)
{
	struct dhcpMessage packet;

	init_packet(&packet, DHCPDISCOVER);
	packet.xid = xid;
	if (requested) {
		add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	}
	add_requests(&packet);
	DEBUG(LOG_DEBUG, "Sending discover...");
	return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
					  SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}

/* Broadcasts a DHCP request message */
static inline int send_selecting(unsigned long xid, unsigned long server,
								 unsigned long requested)
{
	struct dhcpMessage packet;
	struct in_addr addr;

	init_packet(&packet, DHCPREQUEST);
	packet.xid = xid;

	add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	add_requests(&packet);
	addr.s_addr = requested;
	DEBUG(LOG_DEBUG, "Sending select for %s...", inet_ntoa(addr));
	return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
					  SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}


/* Unicasts or broadcasts a DHCP renew message */
static int send_renew(unsigned long xid, unsigned long server,
					  unsigned long ciaddr)
{
	struct dhcpMessage packet;

	init_packet(&packet, DHCPREQUEST);
	packet.xid = xid;
	packet.ciaddr = ciaddr;

	add_requests(&packet);
	DEBUG(LOG_DEBUG, "Sending renew...");
	if (server) {
		return kernel_packet(&packet, ciaddr, CLIENT_PORT, server,
							 SERVER_PORT);
	}
	return raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
					  SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}

/* Create a random xid */
static unsigned long random_xid(void)
{
	static int initialized;

	if (!initialized) {
		srand(time(0));
		initialized++;
	}
	return rand();
}

/* just a little helper */
static void change_mode(int new_mode)
{
	DEBUG(LOG_INFO, "entering %s listen mode",
		  new_mode ? (new_mode == 1 ? "kernel" : "raw") : "none");
	close(fd_main);
	fd_main = -1;
	listen_mode = new_mode;
}


/* SIGUSR1 handler (renew) */
static void renew_requested(int sig)
{
	sig = 0;
	LOG(LOG_INFO, "Received SIGUSR1");
	if (state == BOUND || state == RENEWING || state == REBINDING ||
		state == RELEASED) {
		change_mode(LISTEN_KERNEL);
		packet_num = 0;
		state = RENEW_REQUESTED;
	}

	if (state == RELEASED) {
		change_mode(LISTEN_RAW);
		state = INIT_SELECTING;
	}

	/* Kill any timeouts because the user wants this to hurry along */
	timeout = 0;
}

/* get an option with bounds checking (warning, not aligned). */
static unsigned char *get_option(struct dhcpMessage *packet, int code)
{
	int i, length;
	unsigned char *optionptr;
	int over = 0, done = 0, curr = OPTION_FIELD;

	optionptr = packet->options;
	i = 0;
	length = 308;
	while (!done) {
		if (i >= length) {
			LOG(LOG_WARNING, "bogus packet, option fields too long.");
			return NULL;
		}
		if (optionptr[i + OPT_CODE] == code) {
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return NULL;
			}
			return optionptr + i + 2;
		}
		switch (optionptr[i + OPT_CODE]) {
		case DHCP_PADDING:
			i++;
			break;
		case DHCP_OPTION_OVER:
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				LOG(LOG_WARNING, "bogus packet, option fields too long.");
				return NULL;
			}
			over = optionptr[i + 3];
			i += optionptr[OPT_LEN] + 2;
			break;
		case DHCP_END:
			if (curr == OPTION_FIELD && over & FILE_FIELD) {
				optionptr = packet->file;
				i = 0;
				length = 128;
				curr = FILE_FIELD;
			} else if (curr == FILE_FIELD && over & SNAME_FIELD) {
				optionptr = packet->sname;
				i = 0;
				length = 64;
				curr = SNAME_FIELD;
			} else {
				done = 1;
			}
			break;
		default:
			i += optionptr[OPT_LEN + i] + 2;
		}
	}
	return NULL;
}

static int sprintip(char *dest, char *pre, unsigned char *ip)
{
	return sprintf(dest, "%s%d.%d.%d.%d ", pre, ip[0], ip[1], ip[2], ip[3]);
}


/* Fill dest with the text of option 'option'. */
static inline void fill_options(char *dest, unsigned char *option,
								const struct dhcp_option *type_p)
{
	int type, optlen;
	u_int16_t val_u16;
	int16_t val_s16;
	u_int32_t val_u32;
	int32_t val_s32;
	int len = option[OPT_LEN - 2];

	dest += sprintf(dest, "%s=", type_p->name);

	type = type_p->flags & TYPE_MASK;
	optlen = option_lengths[type];
	for (;;) {
		switch (type) {
		case OPTION_IP_PAIR:
			dest += sprintip(dest, "", option);
			*(dest++) = '/';
			option += 4;
			optlen = 4;
		case OPTION_IP:	/* Works regardless of host byte order. */
			dest += sprintip(dest, "", option);
			break;
		case OPTION_BOOLEAN:
			dest += sprintf(dest, *option ? "yes " : "no ");
			break;
		case OPTION_U8:
			dest += sprintf(dest, "%u ", *option);
			break;
		case OPTION_U16:
			memcpy(&val_u16, option, 2);
			dest += sprintf(dest, "%u ", ntohs(val_u16));
			break;
		case OPTION_S16:
			memcpy(&val_s16, option, 2);
			dest += sprintf(dest, "%d ", ntohs(val_s16));
			break;
		case OPTION_U32:
			memcpy(&val_u32, option, 4);
			dest += sprintf(dest, "%lu ", (unsigned long) ntohl(val_u32));
			break;
		case OPTION_S32:
			memcpy(&val_s32, option, 4);
			dest += sprintf(dest, "%ld ", (long) ntohl(val_s32));
			break;
		case OPTION_STRING:
			memcpy(dest, option, len);
			dest[len] = '\0';
			return;		/* Short circuit this case */
		}
		option += optlen;
		len -= optlen;
		if (len <= 0) {
			break;
		}
	}
}

static char *find_env(const char *prefix, char *defaultstr)
{
	extern char **environ;
	char **ptr;
	const int len = strlen(prefix);

	for (ptr = environ; *ptr != NULL; ptr++) {
		if (strncmp(prefix, *ptr, len) == 0) {
			return *ptr;
		}
	}
	return defaultstr;
}

/* put all the paramaters into an environment */
static char **fill_envp(struct dhcpMessage *packet)
{
	/* supported options are easily added here */
	int num_options = 0;
	int i, j;
	char **envp;
	unsigned char *temp;
	char over = 0;

	if (packet == NULL) {
		num_options = 0;
	} else {
		for (i = 0; options[i].code; i++) {
			if (get_option(packet, options[i].code)) {
				num_options++;
			}
		}
		if (packet->siaddr) {
			num_options++;
		}
		if ((temp = get_option(packet, DHCP_OPTION_OVER))) {
			over = *temp;
		}
		if (!(over & FILE_FIELD) && packet->file[0]) {
			num_options++;
		}
		if (!(over & SNAME_FIELD) && packet->sname[0]) {
			num_options++;
		}
	}

	envp = xmalloc((num_options + 5) * sizeof(char *));
	envp[0] = xmalloc(sizeof("interface=") + strlen(client_config.interface));
	sprintf(envp[0], "interface=%s", client_config.interface);
	envp[1] = find_env("PATH", "PATH=/bin:/usr/bin:/sbin:/usr/sbin");
	envp[2] = find_env("HOME", "HOME=/");

	if (packet == NULL) {
		envp[3] = NULL;
		return envp;
	}

	envp[3] = xmalloc(sizeof("ip=255.255.255.255"));
	sprintip(envp[3], "ip=", (unsigned char *) &packet->yiaddr);
	for (i = 0, j = 4; options[i].code; i++) {
		if ((temp = get_option(packet, options[i].code))) {
			envp[j] =
				xmalloc(max_option_length[(&options[i])->flags & TYPE_MASK] *
						(temp[OPT_LEN - 2] /
						 option_lengths[(&options[i])->flags & TYPE_MASK]) +
						strlen((&options[i])->name) + 2);
			fill_options(envp[j], temp, &options[i]);
			j++;
		}
	}
	if (packet->siaddr) {
		envp[j] = xmalloc(sizeof("siaddr=255.255.255.255"));
		sprintip(envp[j++], "siaddr=", (unsigned char *) &packet->yiaddr);
	}
	if (!(over & FILE_FIELD) && packet->file[0]) {
		/* watch out for invalid packets */
		packet->file[sizeof(packet->file) - 1] = '\0';
		envp[j] = xmalloc(sizeof("boot_file=") + strlen(packet->file));
		sprintf(envp[j++], "boot_file=%s", packet->file);
	}
	if (!(over & SNAME_FIELD) && packet->sname[0]) {
		/* watch out for invalid packets */
		packet->sname[sizeof(packet->sname) - 1] = '\0';
		envp[j] = xmalloc(sizeof("sname=") + strlen(packet->sname));
		sprintf(envp[j++], "sname=%s", packet->sname);
	}
	envp[j] = NULL;
	return envp;
}

/* Call a script with a par file and env vars */
static void run_script(struct dhcpMessage *packet, const char *name)
{
	int pid;
	char **envp;

	if (client_config.script == NULL) {
		return;
	}

	/* call script */
	pid = fork();
	if (pid) {
		waitpid(pid, NULL, 0);
		return;
	} else if (pid == 0) {
		envp = fill_envp(packet);

		/* close fd's? */

		/* exec script */
		DEBUG(LOG_INFO, "execle'ing %s", client_config.script);
		execle(client_config.script, client_config.script, name, NULL, envp);
		LOG(LOG_ERR, "script %s failed: %s",
			client_config.script, strerror(errno));
		exit(1);
	}
}

/* SIGUSR2 handler (release) */
static void release_requested(int sig)
{
	sig = 0;
	LOG(LOG_INFO, "Received SIGUSR2");
	/* send release packet */
	if (state == BOUND || state == RENEWING || state == REBINDING) {
		struct dhcpMessage packet;

		init_packet(&packet, DHCPRELEASE);
		packet.xid = random_xid();
		packet.ciaddr = requested_ip;

		add_simple_option(packet.options, DHCP_REQUESTED_IP, requested_ip);
		add_simple_option(packet.options, DHCP_SERVER_ID, server_addr);

		DEBUG(LOG_DEBUG, "Sending release...");
		kernel_packet(&packet, requested_ip, CLIENT_PORT, server_addr,
					  SERVER_PORT);
		run_script(NULL, "deconfig");
	}

	change_mode(LISTEN_NONE);
	state = RELEASED;
	timeout = 0x7fffffff;
}


static int pidfile_acquire(char *pidfile)
{
	int pid_fd;

	if (pidfile == NULL) {
		return -1;
	}
	pid_fd = open(pidfile, O_CREAT | O_WRONLY, 0644);
	if (pid_fd < 0) {
		LOG(LOG_ERR, "Unable to open pidfile %s: %s\n",
			pidfile, strerror(errno));
	} else {
		lockf(pid_fd, F_LOCK, 0);
	}

	return pid_fd;
}


static void pidfile_write_release(int pid_fd)
{
	FILE *out;

	if (pid_fd < 0) {
		return;
	}
	if ((out = fdopen(pid_fd, "w")) != NULL) {
		fprintf(out, "%d\n", getpid());
		fclose(out);
	}
	lockf(pid_fd, F_UNLCK, 0);
	close(pid_fd);
}

/* Exit and cleanup */
static void exit_client(int retval)
{
	unlink(client_config.pidfile);
	if (client_config.pidfile) {
		unlink(client_config.pidfile);
	}
	CLOSE_LOG();
	exit(retval);
}


/* SIGTERM handler */
static void terminate(int sig)
{
	sig = 0;
	LOG(LOG_INFO, "Received SIGTERM");
	exit_client(0);
}


static inline int read_interface(char *interface, int *ifindex,
								 u_int32_t * addr, unsigned char *arp)
{
	int l_fd;
	struct ifreq ifr;
	struct sockaddr_in *s_in;

	memset(&ifr, 0, sizeof(struct ifreq));
	if ((l_fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
		ifr.ifr_addr.sa_family = AF_INET;
		strcpy(ifr.ifr_name, interface);

		if (addr) {
			if (ioctl(l_fd, SIOCGIFADDR, &ifr) == 0) {
				s_in = (struct sockaddr_in *) &ifr.ifr_addr;
				*addr = s_in->sin_addr.s_addr;
				DEBUG(LOG_INFO, "%s (our ip) = %s", ifr.ifr_name,
					  inet_ntoa(s_in->sin_addr));
			} else {
				LOG(LOG_ERR, "SIOCGIFADDR failed!: %s", strerror(errno));
				return -1;
			}
		}

		if (ioctl(l_fd, SIOCGIFINDEX, &ifr) == 0) {
			DEBUG(LOG_INFO, "adapter index %d", ifr.ifr_ifindex);
			*ifindex = ifr.ifr_ifindex;
		} else {
			LOG(LOG_ERR, "SIOCGIFINDEX failed!: %s", strerror(errno));
			return -1;
		}
		if (ioctl(l_fd, SIOCGIFHWADDR, &ifr) == 0) {
			memcpy(arp, ifr.ifr_hwaddr.sa_data, 6);
			DEBUG(LOG_INFO,
				  "adapter hardware address %02x:%02x:%02x:%02x:%02x:%02x",
				  arp[0], arp[1], arp[2], arp[3], arp[4], arp[5]);
		} else {
			LOG(LOG_ERR, "SIOCGIFHWADDR failed!: %s", strerror(errno));
			return -1;
		}
	} else {
		LOG(LOG_ERR, "socket failed!: %s", strerror(errno));
		return -1;
	}
	close(l_fd);
	return 0;
}


static inline int listen_socket(unsigned int ip, int port, char *inf)
{
	struct ifreq interface;
	int l_fd;
	struct sockaddr_in addr;
	int n = 1;

	DEBUG(LOG_INFO, "Opening listen socket on 0x%08x:%d %s\n", ip, port, inf);
	if ((l_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		DEBUG(LOG_ERR, "socket call failed: %s", strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = ip;

	if (setsockopt(l_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &n, sizeof(n)) ==
		-1) {
		close(l_fd);
		return -1;
	}
	if (setsockopt(l_fd, SOL_SOCKET, SO_BROADCAST, (char *) &n, sizeof(n)) ==
		-1) {
		close(l_fd);
		return -1;
	}

	strncpy(interface.ifr_ifrn.ifrn_name, inf, IFNAMSIZ);
	if (setsockopt
		(l_fd, SOL_SOCKET, SO_BINDTODEVICE, (char *) &interface,
		 sizeof(interface)) < 0) {
		close(l_fd);
		return -1;
	}

	if (bind(l_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr)) == -1) {
		close(l_fd);
		return -1;
	}

	return l_fd;
}


static int raw_socket(int ifindex)
{
	int l_fd;
	struct sockaddr_ll sock;

	DEBUG(LOG_INFO, "Opening raw socket on ifindex %d\n", ifindex);
	if ((l_fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) < 0) {
		DEBUG(LOG_ERR, "socket call failed: %s", strerror(errno));
		return -1;
	}

	sock.sll_family = AF_PACKET;
	sock.sll_protocol = htons(ETH_P_IP);
	sock.sll_ifindex = ifindex;
	if (bind(l_fd, (struct sockaddr *) &sock, sizeof(sock)) < 0) {
		DEBUG(LOG_ERR, "bind call failed: %s", strerror(errno));
		close(l_fd);
		return -1;
	}

	return l_fd;

}

/* read a packet from socket fd, return -1 on read error, -2 on packet error */
static int get_packet(struct dhcpMessage *packet, int l_fd)
{
	int bytes;
	int i;
	const char broken_vendors[][8] = {
		"MSFT 98",
		""
	};
	char unsigned *vendor;

	memset(packet, 0, sizeof(struct dhcpMessage));
	bytes = read(l_fd, packet, sizeof(struct dhcpMessage));
	if (bytes < 0) {
		DEBUG(LOG_INFO, "couldn't read on listening socket, ignoring");
		return -1;
	}

	if (ntohl(packet->cookie) != DHCP_MAGIC) {
		LOG(LOG_ERR, "received bogus message, ignoring");
		return -2;
	}
	DEBUG(LOG_INFO, "Received a packet");

	if (packet->op == BOOTREQUEST
		&& (vendor = get_option(packet, DHCP_VENDOR))) {
		for (i = 0; broken_vendors[i][0]; i++) {
			if (vendor[OPT_LEN - 2] ==
				(unsigned char) strlen(broken_vendors[i])
				&& !strncmp(vendor, broken_vendors[i], vendor[OPT_LEN - 2])) {
				DEBUG(LOG_INFO, "broken client (%s), forcing broadcast",
					  broken_vendors[i]);
				packet->flags |= htons(BROADCAST_FLAG);
			}
		}
	}

	return bytes;
}

static inline int get_raw_packet(struct dhcpMessage *payload, int l_fd)
{
	int bytes;
	struct udp_dhcp_packet packet;
	u_int32_t source, dest;
	u_int16_t check;

	memset(&packet, 0, sizeof(struct udp_dhcp_packet));
	bytes = read(l_fd, &packet, sizeof(struct udp_dhcp_packet));
	if (bytes < 0) {
		DEBUG(LOG_INFO, "couldn't read on raw listening socket -- ignoring");
		usleep(500000);	/* possible down interface, looping condition */
		return -1;
	}

	if (bytes < (int) (sizeof(struct iphdr) + sizeof(struct udphdr))) {
		DEBUG(LOG_INFO, "message too short, ignoring");
		return -1;
	}

	if (bytes < ntohs(packet.ip.tot_len)) {
		DEBUG(LOG_INFO, "Truncated packet");
		return -1;
	}

	/* ignore any extra garbage bytes */
	bytes = ntohs(packet.ip.tot_len);

	/* Make sure its the right packet for us, and that it passes sanity checks */
	if (packet.ip.protocol != IPPROTO_UDP || packet.ip.version != IPVERSION ||
		packet.ip.ihl != sizeof(packet.ip) >> 2
		|| packet.udp.dest != htons(CLIENT_PORT)
		|| bytes > (int) sizeof(struct udp_dhcp_packet)
		|| ntohs(packet.udp.len) != (short) (bytes - sizeof(packet.ip))) {
		DEBUG(LOG_INFO, "unrelated/bogus packet");
		return -1;
	}

	/* check IP checksum */
	check = packet.ip.check;
	packet.ip.check = 0;
	if (check != checksum(&(packet.ip), sizeof(packet.ip))) {
		DEBUG(LOG_INFO, "bad IP header checksum, ignoring");
		return -1;
	}

	/* verify the UDP checksum by replacing the header with a psuedo header */
	source = packet.ip.saddr;
	dest = packet.ip.daddr;
	check = packet.udp.check;
	packet.udp.check = 0;
	memset(&packet.ip, 0, sizeof(packet.ip));

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source;
	packet.ip.daddr = dest;
	packet.ip.tot_len = packet.udp.len;	/* cheat on the psuedo-header */
	if (check && check != checksum(&packet, bytes)) {
		DEBUG(LOG_ERR, "packet with bad UDP checksum received, ignoring");
		return -1;
	}

	memcpy(payload, &(packet.data),
		   bytes - (sizeof(packet.ip) + sizeof(packet.udp)));

	if (ntohl(payload->cookie) != DHCP_MAGIC) {
		LOG(LOG_ERR, "received bogus message (bad magic) -- ignoring");
		return -1;
	}
	DEBUG(LOG_INFO, "oooooh!!! got some!");
	return bytes - (sizeof(packet.ip) + sizeof(packet.udp));
}


int udhcpc_main(int argc, char *argv[])
{
	unsigned char *temp, *message;
	unsigned long t1 = 0, t2 = 0, xid = 0;
	unsigned long start = 0, lease;
	fd_set rfds;
	int retval;
	struct timeval tv;
	int c, len;
	struct dhcpMessage packet;
	struct in_addr temp_addr;
	int pid_fd;
	time_t now;

	static struct option l_options[] = {
		{"clientid", required_argument, 0, 'c'},
		{"foreground", no_argument, 0, 'f'},
		{"hostname", required_argument, 0, 'H'},
		{"help", no_argument, 0, 'h'},
		{"interface", required_argument, 0, 'i'},
		{"now", no_argument, 0, 'n'},
		{"pidfile", required_argument, 0, 'p'},
		{"quit", no_argument, 0, 'q'},
		{"request", required_argument, 0, 'r'},
		{"script", required_argument, 0, 's'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	/* get options */
	while (1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "c:fH:i:np:qr:s:v", l_options,
						&option_index);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'c':
			len = strlen(optarg);
			if (len > 255) {
				len = 255;
			}
			if (client_config.clientid) {
				free(client_config.clientid);
			}
			client_config.clientid = xmalloc(len + 2);
			client_config.clientid[OPT_CODE] = DHCP_CLIENT_ID;
			client_config.clientid[OPT_LEN] = len;
			client_config.clientid[OPT_DATA] = '\0';
			strncpy(client_config.clientid + 3, optarg, len - 1);
			break;
		case 'f':
			client_config.foreground = 1;
			break;
		case 'H':
			len = strlen(optarg);
			if (len > 255) {
				len = 255;
			}
			if (client_config.hostname) {
				free(client_config.hostname);
			}
			client_config.hostname = xmalloc(len + 2);
			client_config.hostname[OPT_CODE] = DHCP_HOST_NAME;
			client_config.hostname[OPT_LEN] = len;
			strncpy(client_config.hostname + 2, optarg, len);
			break;
		case 'i':
			client_config.interface = optarg;
			break;
		case 'n':
			client_config.abort_if_no_lease = 1;
			break;
		case 'p':
			client_config.pidfile = optarg;
			break;
		case 'q':
			client_config.quit_after_lease = 1;
			break;
		case 'r':
			requested_ip = inet_addr(optarg);
			break;
		case 's':
			client_config.script = optarg;
			break;
		case 'v':
			printf("udhcpcd, version %s\n\n", VERSION);
			exit_client(0);
			break;
		default:
			show_usage();
		}
	}

	OPEN_LOG("udhcpc");
	LOG(LOG_INFO, "udhcp client (v%s) started", VERSION);

	pid_fd = pidfile_acquire(client_config.pidfile);
	pidfile_write_release(pid_fd);

	if (read_interface
		(client_config.interface, &client_config.ifindex, NULL,
		 client_config.arp) < 0) {
		exit_client(1);
	}

	if (!client_config.clientid) {
		client_config.clientid = xmalloc(6 + 3);
		client_config.clientid[OPT_CODE] = DHCP_CLIENT_ID;
		client_config.clientid[OPT_LEN] = 7;
		client_config.clientid[OPT_DATA] = 1;
		memcpy(client_config.clientid + 3, client_config.arp, 6);
	}

	/* setup signal handlers */
	signal(SIGUSR1, renew_requested);
	signal(SIGUSR2, release_requested);
	signal(SIGTERM, terminate);

	state = INIT_SELECTING;
	run_script(NULL, "deconfig");
	change_mode(LISTEN_RAW);

	for (;;) {
		tv.tv_sec = timeout - time(0);
		tv.tv_usec = 0;
		FD_ZERO(&rfds);

		if (listen_mode != LISTEN_NONE && fd_main < 0) {
			if (listen_mode == LISTEN_KERNEL) {
				fd_main =
					listen_socket(INADDR_ANY, CLIENT_PORT,
								  client_config.interface);
			} else {
				fd_main = raw_socket(client_config.ifindex);
			}
			if (fd_main < 0) {
				LOG(LOG_ERR, "FATAL: couldn't listen on socket, %s",
					strerror(errno));
				exit_client(0);
			}
		}
		if (fd_main >= 0) {
			FD_SET(fd_main, &rfds);
		}

		if (tv.tv_sec > 0) {
			DEBUG(LOG_INFO, "Waiting on select...\n");
			retval = select(fd_main + 1, &rfds, NULL, NULL, &tv);
		} else {
			retval = 0;	/* If we already timed out, fall through */
		}

		now = time(0);
		if (retval == 0) {
			/* timeout dropped to zero */
			switch (state) {
			case INIT_SELECTING:
				if (packet_num < 3) {
					if (packet_num == 0) {
						xid = random_xid();
					}
					/* send discover packet */
					send_discover(xid, requested_ip);	/* broadcast */

					timeout = now + ((packet_num == 2) ? 10 : 2);
					packet_num++;
				} else {
					if (client_config.abort_if_no_lease) {
						LOG(LOG_INFO, "No lease, failing.");
						exit_client(1);
					}
					/* wait to try again */
					packet_num = 0;
					timeout = now + 60;
				}
				break;
			case RENEW_REQUESTED:
			case REQUESTING:
				if (packet_num < 3) {
					/* send request packet */
					if (state == RENEW_REQUESTED) {
						send_renew(xid, server_addr, requested_ip);	/* unicast */
					} else {
						send_selecting(xid, server_addr, requested_ip);	/* broadcast */
					}
					timeout = now + ((packet_num == 2) ? 10 : 2);
					packet_num++;
				} else {
					/* timed out, go back to init state */
					state = INIT_SELECTING;
					timeout = now;
					packet_num = 0;
					change_mode(LISTEN_RAW);
				}
				break;
			case BOUND:
				/* Lease is starting to run out, time to enter renewing state */
				state = RENEWING;
				change_mode(LISTEN_KERNEL);
				DEBUG(LOG_INFO, "Entering renew state");
				/* fall right through */
			case RENEWING:
				/* Either set a new T1, or enter REBINDING state */
				if ((t2 - t1) <= (lease / 14400 + 1)) {
					/* timed out, enter rebinding state */
					state = REBINDING;
					timeout = now + (t2 - t1);
					DEBUG(LOG_INFO, "Entering rebinding state");
				} else {
					/* send a request packet */
					send_renew(xid, server_addr, requested_ip);	/* unicast */

					t1 = (t2 - t1) / 2 + t1;
					timeout = t1 + start;
				}
				break;
			case REBINDING:
				/* Either set a new T2, or enter INIT state */
				if ((lease - t2) <= (lease / 14400 + 1)) {
					/* timed out, enter init state */
					state = INIT_SELECTING;
					LOG(LOG_INFO, "Lease lost, entering init state");
					run_script(NULL, "deconfig");
					timeout = now;
					packet_num = 0;
					change_mode(LISTEN_RAW);
				} else {
					/* send a request packet */
					send_renew(xid, 0, requested_ip);	/* broadcast */

					t2 = (lease - t2) / 2 + t2;
					timeout = t2 + start;
				}
				break;
			case RELEASED:
				/* yah, I know, *you* say it would never happen */
				timeout = 0x7fffffff;
				break;
			}
		} else if (retval > 0 && listen_mode != LISTEN_NONE
				   && FD_ISSET(fd_main, &rfds)) {
			/* a packet is ready, read it */

			if (listen_mode == LISTEN_KERNEL) {
				len = get_packet(&packet, fd_main);
			} else {
				len = get_raw_packet(&packet, fd_main);
			}
			if (len == -1 && errno != EINTR) {
				DEBUG(LOG_INFO, "error on read, %s, reopening socket",
					  strerror(errno));
				change_mode(listen_mode);	/* just close and reopen */
			}
			if (len < 0) {
				continue;
			}

			if (packet.xid != xid) {
				DEBUG(LOG_INFO, "Ignoring XID %lx (our xid is %lx)",
					  (unsigned long) packet.xid, xid);
				continue;
			}

			if ((message = get_option(&packet, DHCP_MESSAGE_TYPE)) == NULL) {
				DEBUG(LOG_ERR, "couldnt get option from packet -- ignoring");
				continue;
			}

			switch (state) {
			case INIT_SELECTING:
				/* Must be a DHCPOFFER to one of our xid's */
				if (*message == DHCPOFFER) {
					if ((temp = get_option(&packet, DHCP_SERVER_ID))) {
						memcpy(&server_addr, temp, 4);
						xid = packet.xid;
						requested_ip = packet.yiaddr;

						/* enter requesting state */
						state = REQUESTING;
						timeout = now;
						packet_num = 0;
					} else {
						DEBUG(LOG_ERR, "No server ID in message");
					}
				}
				break;
			case RENEW_REQUESTED:
			case REQUESTING:
			case RENEWING:
			case REBINDING:
				if (*message == DHCPACK) {
					if (!(temp = get_option(&packet, DHCP_LEASE_TIME))) {
						LOG(LOG_ERR,
							"No lease time with ACK, using 1 hour lease");
						lease = 60 * 60;
					} else {
						memcpy(&lease, temp, 4);
						lease = ntohl(lease);
					}

					/* enter bound state */
					t1 = lease / 2;

					/* little fixed point for n * .875 */
					t2 = (lease * 0x7) >> 3;
					temp_addr.s_addr = packet.yiaddr;
					LOG(LOG_INFO, "Lease of %s obtained, lease time %ld",
						inet_ntoa(temp_addr), lease);
					start = now;
					timeout = t1 + start;
					requested_ip = packet.yiaddr;
					run_script(&packet,
							   ((state == RENEWING
								 || state == REBINDING) ? "renew" : "bound"));

					state = BOUND;
					change_mode(LISTEN_NONE);
					{
						int pid_fd2;

						if (client_config.quit_after_lease) {
							exit_client(0);
						} else if (!client_config.foreground) {
							pid_fd2 = pidfile_acquire(client_config.pidfile);	/* hold lock during fork. */
							if (daemon(0, 0) == -1) {
								perror("fork");
								exit_client(1);
							}
							client_config.foreground = 1;	/* Do not fork again. */
							pidfile_write_release(pid_fd2);
						}
					}
				} else if (*message == DHCPNAK) {
					/* return to init state */
					LOG(LOG_INFO, "Received DHCP NAK");
					run_script(&packet, "nak");
					if (state != REQUESTING) {
						run_script(NULL, "deconfig");
					}
					state = INIT_SELECTING;
					timeout = now;
					requested_ip = 0;
					packet_num = 0;
					change_mode(LISTEN_RAW);
					sleep(3);	/* avoid excessive network traffic */
				}
				break;
				/* case BOUND, RELEASED: - ignore all packets */
			}
		} else if (retval == -1 && errno == EINTR) {
			/* a signal was caught */

		} else {
			/* An error occured */
			DEBUG(LOG_ERR, "Error on select");
		}

	}
	return 0;
}
