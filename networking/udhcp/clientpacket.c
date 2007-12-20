/* vi: set sw=4 ts=4: */
/* clientpacket.c
 *
 * Packet generation and dispatching functions for the DHCP client.
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <features.h>
#if (defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || defined _NEWLIB_VERSION
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif

#include "common.h"
#include "dhcpd.h"
#include "dhcpc.h"
#include "options.h"


/* Create a random xid */
uint32_t random_xid(void)
{
	static smallint initialized;

	if (!initialized) {
		srand(monotonic_us());
		initialized = 1;
	}
	return rand();
}


/* initialize a packet with the proper defaults */
static void init_packet(struct dhcpMessage *packet, char type)
{
	udhcp_init_header(packet, type);
	memcpy(packet->chaddr, client_config.arp, 6);
	if (client_config.clientid)
		add_option_string(packet->options, client_config.clientid);
	if (client_config.hostname)
		add_option_string(packet->options, client_config.hostname);
	if (client_config.fqdn)
		add_option_string(packet->options, client_config.fqdn);
	add_option_string(packet->options, client_config.vendorclass);
}


/* Add a parameter request list for stubborn DHCP servers. Pull the data
 * from the struct in options.c. Don't do bounds checking here because it
 * goes towards the head of the packet. */
static void add_requests(struct dhcpMessage *packet)
{
	uint8_t c;
	int end = end_option(packet->options);
	int i, len = 0;

	packet->options[end + OPT_CODE] = DHCP_PARAM_REQ;
	for (i = 0; (c = dhcp_options[i].code) != 0; i++) {
		if (dhcp_options[i].flags & OPTION_REQ
		 || (client_config.opt_mask[c >> 3] & (1 << (c & 7)))
		) {
			packet->options[end + OPT_DATA + len++] = c;
		}
	}
	packet->options[end + OPT_LEN] = len;
	packet->options[end + OPT_DATA + len] = DHCP_END;

}

#if ENABLE_FEATURE_UDHCPC_ARPING
/* Unicast a DHCP decline message */
int send_decline(uint32_t xid, uint32_t server)
{
	struct dhcpMessage packet;

	init_packet(&packet, DHCPDECLINE);
	packet.xid = xid;
	add_requests(&packet);

	bb_info_msg("Sending decline...");

	return udhcp_send_raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
		SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}
#endif

/* Broadcast a DHCP discover packet to the network, with an optionally requested IP */
int send_discover(uint32_t xid, uint32_t requested)
{
	struct dhcpMessage packet;

	init_packet(&packet, DHCPDISCOVER);
	packet.xid = xid;
	if (requested)
		add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);

	/* Explicitly saying that we want RFC-compliant packets helps
	 * some buggy DHCP servers to NOT send bigger packets */
	add_simple_option(packet.options, DHCP_MAX_SIZE, htons(576));
	add_requests(&packet);
	bb_info_msg("Sending discover...");
	return udhcp_send_raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
			SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}


/* Broadcasts a DHCP request message */
int send_selecting(uint32_t xid, uint32_t server, uint32_t requested)
{
	struct dhcpMessage packet;
	struct in_addr addr;

	init_packet(&packet, DHCPREQUEST);
	packet.xid = xid;

	add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	add_requests(&packet);
	addr.s_addr = requested;
	bb_info_msg("Sending select for %s...", inet_ntoa(addr));
	return udhcp_send_raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}


/* Unicasts or broadcasts a DHCP renew message */
int send_renew(uint32_t xid, uint32_t server, uint32_t ciaddr)
{
	struct dhcpMessage packet;

	init_packet(&packet, DHCPREQUEST);
	packet.xid = xid;
	packet.ciaddr = ciaddr;

	add_requests(&packet);
	bb_info_msg("Sending renew...");
	if (server)
		return udhcp_send_kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT);

	return udhcp_send_raw_packet(&packet, INADDR_ANY, CLIENT_PORT, INADDR_BROADCAST,
				SERVER_PORT, MAC_BCAST_ADDR, client_config.ifindex);
}


/* Unicasts a DHCP release message */
int send_release(uint32_t server, uint32_t ciaddr)
{
	struct dhcpMessage packet;

	init_packet(&packet, DHCPRELEASE);
	packet.xid = random_xid();
	packet.ciaddr = ciaddr;

	add_simple_option(packet.options, DHCP_REQUESTED_IP, ciaddr);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	bb_info_msg("Sending release...");
	return udhcp_send_kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT);
}


/* return -1 on errors that are fatal for the socket, -2 for those that aren't */
int get_raw_packet(struct dhcpMessage *payload, int fd)
{
	int bytes;
	struct udp_dhcp_packet packet;
	uint16_t check;

	memset(&packet, 0, sizeof(struct udp_dhcp_packet));
	bytes = read(fd, &packet, sizeof(struct udp_dhcp_packet));
	if (bytes < 0) {
		DEBUG("Cannot read on raw listening socket - ignoring");
		sleep(1); /* possible down interface, looping condition */
		return -1;
	}

	if (bytes < (int) (sizeof(struct iphdr) + sizeof(struct udphdr))) {
		DEBUG("Message too short, ignoring");
		return -2;
	}

	if (bytes < ntohs(packet.ip.tot_len)) {
		DEBUG("Truncated packet");
		return -2;
	}

	/* ignore any extra garbage bytes */
	bytes = ntohs(packet.ip.tot_len);

	/* Make sure its the right packet for us, and that it passes sanity checks */
	if (packet.ip.protocol != IPPROTO_UDP || packet.ip.version != IPVERSION
	 || packet.ip.ihl != (sizeof(packet.ip) >> 2)
	 || packet.udp.dest != htons(CLIENT_PORT)
	 || bytes > (int) sizeof(struct udp_dhcp_packet)
	 || ntohs(packet.udp.len) != (uint16_t)(bytes - sizeof(packet.ip))
	) {
		DEBUG("Unrelated/bogus packet");
		return -2;
	}

	/* verify IP checksum */
	check = packet.ip.check;
	packet.ip.check = 0;
	if (check != udhcp_checksum(&packet.ip, sizeof(packet.ip))) {
		DEBUG("Bad IP header checksum, ignoring");
		return -1;
	}

	/* verify UDP checksum. IP header has to be modified for this */
	memset(&packet.ip, 0, offsetof(struct iphdr, protocol));
	/* fields which are not memset: protocol, check, saddr, daddr */
	packet.ip.tot_len = packet.udp.len; /* yes, this is needed */
	check = packet.udp.check;
	packet.udp.check = 0;
	if (check && check != udhcp_checksum(&packet, bytes)) {
		bb_error_msg("packet with bad UDP checksum received, ignoring");
		return -2;
	}

	memcpy(payload, &packet.data, bytes - (sizeof(packet.ip) + sizeof(packet.udp)));

	if (payload->cookie != htonl(DHCP_MAGIC)) {
		bb_error_msg("received bogus message (bad magic) - ignoring");
		return -2;
	}
	DEBUG("Got valid DHCP packet");
	return bytes - (sizeof(packet.ip) + sizeof(packet.udp));
}
