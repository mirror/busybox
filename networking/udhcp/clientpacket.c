/* vi: set sw=4 ts=4: */
/* clientpacket.c
 *
 * Packet generation and dispatching functions for the DHCP client.
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "common.h"
#include "dhcpd.h"
#include "dhcpc.h"
#include "options.h"

//#include <features.h>
#if (defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || defined _NEWLIB_VERSION
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif


/* Create a random xid */
uint32_t FAST_FUNC random_xid(void)
{
	static smallint initialized;

	if (!initialized) {
		srand(monotonic_us());
		initialized = 1;
	}
	return rand();
}


/* Initialize the packet with the proper defaults */
static void init_packet(struct dhcp_packet *packet, char type)
{
	udhcp_init_header(packet, type);
	memcpy(packet->chaddr, client_config.client_mac, 6);
	if (client_config.clientid)
		add_option_string(packet->options, client_config.clientid);
	if (client_config.hostname)
		add_option_string(packet->options, client_config.hostname);
	if (client_config.fqdn)
		add_option_string(packet->options, client_config.fqdn);
	if ((type != DHCPDECLINE) && (type != DHCPRELEASE))
		add_option_string(packet->options, client_config.vendorclass);
}


/* Add a parameter request list for stubborn DHCP servers. Pull the data
 * from the struct in options.c. Don't do bounds checking here because it
 * goes towards the head of the packet. */
static void add_param_req_option(struct dhcp_packet *packet)
{
	uint8_t c;
	int end = end_option(packet->options);
	int i, len = 0;

	for (i = 0; (c = dhcp_options[i].code) != 0; i++) {
		if (((dhcp_options[i].flags & OPTION_REQ)
		     && !client_config.no_default_options)
		 || (client_config.opt_mask[c >> 3] & (1 << (c & 7)))
		) {
			packet->options[end + OPT_DATA + len] = c;
			len++;
		}
	}
	if (len) {
		packet->options[end + OPT_CODE] = DHCP_PARAM_REQ;
		packet->options[end + OPT_LEN] = len;
		packet->options[end + OPT_DATA + len] = DHCP_END;
	}
}

/* RFC 2131
 * 4.4.4 Use of broadcast and unicast
 *
 * The DHCP client broadcasts DHCPDISCOVER, DHCPREQUEST and DHCPINFORM
 * messages, unless the client knows the address of a DHCP server.
 * The client unicasts DHCPRELEASE messages to the server. Because
 * the client is declining the use of the IP address supplied by the server,
 * the client broadcasts DHCPDECLINE messages.
 *
 * When the DHCP client knows the address of a DHCP server, in either
 * INIT or REBOOTING state, the client may use that address
 * in the DHCPDISCOVER or DHCPREQUEST rather than the IP broadcast address.
 * The client may also use unicast to send DHCPINFORM messages
 * to a known DHCP server. If the client receives no response to DHCP
 * messages sent to the IP address of a known DHCP server, the DHCP
 * client reverts to using the IP broadcast address.
 */

static int raw_bcast_from_client_config_ifindex(struct dhcp_packet *packet)
{
	return udhcp_send_raw_packet(packet,
		/*src*/ INADDR_ANY, CLIENT_PORT,
		/*dst*/ INADDR_BROADCAST, SERVER_PORT, MAC_BCAST_ADDR,
		client_config.ifindex);
}


#if ENABLE_FEATURE_UDHCPC_ARPING
/* Broadcast a DHCP decline message */
int FAST_FUNC send_decline(uint32_t xid, uint32_t server, uint32_t requested)
{
	struct dhcp_packet packet;

	init_packet(&packet, DHCPDECLINE);
	packet.xid = xid;
	add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	bb_info_msg("Sending decline...");

	return raw_bcast_from_client_config_ifindex(&packet);
}
#endif


/* Broadcast a DHCP discover packet to the network, with an optionally requested IP */
int FAST_FUNC send_discover(uint32_t xid, uint32_t requested)
{
	struct dhcp_packet packet;

	init_packet(&packet, DHCPDISCOVER);
	packet.xid = xid;
	if (requested)
		add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);

	/* Explicitly saying that we want RFC-compliant packets helps
	 * some buggy DHCP servers to NOT send bigger packets */
	add_simple_option(packet.options, DHCP_MAX_SIZE, htons(576));

	add_param_req_option(&packet);

	bb_info_msg("Sending discover...");
	return raw_bcast_from_client_config_ifindex(&packet);
}


/* Broadcast a DHCP request message */
/* RFC 2131 3.1 paragraph 3:
 * "The client _broadcasts_ a DHCPREQUEST message..."
 */
int FAST_FUNC send_select(uint32_t xid, uint32_t server, uint32_t requested)
{
	struct dhcp_packet packet;
	struct in_addr addr;

	init_packet(&packet, DHCPREQUEST);
	packet.xid = xid;

	add_simple_option(packet.options, DHCP_REQUESTED_IP, requested);
	add_simple_option(packet.options, DHCP_SERVER_ID, server);
	add_param_req_option(&packet);

	addr.s_addr = requested;
	bb_info_msg("Sending select for %s...", inet_ntoa(addr));
	return raw_bcast_from_client_config_ifindex(&packet);
}


/* Unicast or broadcast a DHCP renew message */
int FAST_FUNC send_renew(uint32_t xid, uint32_t server, uint32_t ciaddr)
{
	struct dhcp_packet packet;

	init_packet(&packet, DHCPREQUEST);
	packet.xid = xid;
	packet.ciaddr = ciaddr;

	add_param_req_option(&packet);
	bb_info_msg("Sending renew...");
	if (server)
		return udhcp_send_kernel_packet(&packet,
			ciaddr, CLIENT_PORT,
			server, SERVER_PORT);

	return raw_bcast_from_client_config_ifindex(&packet);
}


/* Unicast a DHCP release message */
int FAST_FUNC send_release(uint32_t server, uint32_t ciaddr)
{
	struct dhcp_packet packet;

	init_packet(&packet, DHCPRELEASE);
	packet.xid = random_xid();
	packet.ciaddr = ciaddr;

	add_simple_option(packet.options, DHCP_SERVER_ID, server);

	bb_info_msg("Sending release...");
	return udhcp_send_kernel_packet(&packet, ciaddr, CLIENT_PORT, server, SERVER_PORT);
}


/* Returns -1 on errors that are fatal for the socket, -2 for those that aren't */
int FAST_FUNC udhcp_recv_raw_packet(struct dhcp_packet *dhcp_pkt, int fd)
{
	int bytes;
	struct ip_udp_dhcp_packet packet;
	uint16_t check;

	memset(&packet, 0, sizeof(packet));
	bytes = safe_read(fd, &packet, sizeof(packet));
	if (bytes < 0) {
		log1("Packet read error, ignoring");
		/* NB: possible down interface, etc. Caller should pause. */
		return bytes; /* returns -1 */
	}

	if (bytes < (int) (sizeof(packet.ip) + sizeof(packet.udp))) {
		log1("Packet is too short, ignoring");
		return -2;
	}

	if (bytes < ntohs(packet.ip.tot_len)) {
		/* packet is bigger than sizeof(packet), we did partial read */
		log1("Oversized packet, ignoring");
		return -2;
	}

	/* ignore any extra garbage bytes */
	bytes = ntohs(packet.ip.tot_len);

	/* make sure its the right packet for us, and that it passes sanity checks */
	if (packet.ip.protocol != IPPROTO_UDP || packet.ip.version != IPVERSION
	 || packet.ip.ihl != (sizeof(packet.ip) >> 2)
	 || packet.udp.dest != htons(CLIENT_PORT)
	/* || bytes > (int) sizeof(packet) - can't happen */
	 || ntohs(packet.udp.len) != (uint16_t)(bytes - sizeof(packet.ip))
	) {
		log1("Unrelated/bogus packet, ignoring");
		return -2;
	}

	/* verify IP checksum */
	check = packet.ip.check;
	packet.ip.check = 0;
	if (check != udhcp_checksum(&packet.ip, sizeof(packet.ip))) {
		log1("Bad IP header checksum, ignoring");
		return -2;
	}

	/* verify UDP checksum. IP header has to be modified for this */
	memset(&packet.ip, 0, offsetof(struct iphdr, protocol));
	/* ip.xx fields which are not memset: protocol, check, saddr, daddr */
	packet.ip.tot_len = packet.udp.len; /* yes, this is needed */
	check = packet.udp.check;
	packet.udp.check = 0;
	if (check && check != udhcp_checksum(&packet, bytes)) {
		log1("Packet with bad UDP checksum received, ignoring");
		return -2;
	}

	memcpy(dhcp_pkt, &packet.data, bytes - (sizeof(packet.ip) + sizeof(packet.udp)));

	if (dhcp_pkt->cookie != htonl(DHCP_MAGIC)) {
		bb_info_msg("Packet with bad magic, ignoring");
		return -2;
	}
	log1("Got valid DHCP packet");
	udhcp_dump_packet(dhcp_pkt);
	return bytes - (sizeof(packet.ip) + sizeof(packet.udp));
}
