/* vi: set sw=4 ts=4: */
/* serverpacket.c
 *
 * Construct and send DHCP server packets
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
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

#include "common.h"
#include "dhcpc.h"
#include "dhcpd.h"
#include "options.h"


/* send a packet to gateway_nip using the kernel ip stack */
static int send_packet_to_relay(struct dhcp_packet *dhcp_pkt)
{
	log1("Forwarding packet to relay");

	return udhcp_send_kernel_packet(dhcp_pkt,
			server_config.server_nip, SERVER_PORT,
			dhcp_pkt->gateway_nip, SERVER_PORT);
}


/* send a packet to a specific mac address and ip address by creating our own ip packet */
static int send_packet_to_client(struct dhcp_packet *dhcp_pkt, int force_broadcast)
{
	const uint8_t *chaddr;
	uint32_t ciaddr;

	// Was:
	//if (force_broadcast) { /* broadcast */ }
	//else if (dhcp_pkt->ciaddr) { /* unicast to dhcp_pkt->ciaddr */ }
	//else if (dhcp_pkt->flags & htons(BROADCAST_FLAG)) { /* broadcast */ }
	//else { /* unicast to dhcp_pkt->yiaddr */ }
	// But this is wrong: yiaddr is _our_ idea what client's IP is
	// (for example, from lease file). Client may not know that,
	// and may not have UDP socket listening on that IP!
	// We should never unicast to dhcp_pkt->yiaddr!
	// dhcp_pkt->ciaddr, OTOH, comes from client's request packet,
	// and can be used.

	if (force_broadcast
	 || (dhcp_pkt->flags & htons(BROADCAST_FLAG))
	 || !dhcp_pkt->ciaddr
	) {
		log1("Broadcasting packet to client");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else {
		log1("Unicasting packet to client ciaddr");
		ciaddr = dhcp_pkt->ciaddr;
		chaddr = dhcp_pkt->chaddr;
	}

	return udhcp_send_raw_packet(dhcp_pkt,
		/*src*/ server_config.server_nip, SERVER_PORT,
		/*dst*/ ciaddr, CLIENT_PORT, chaddr,
		server_config.ifindex);
}


/* send a dhcp packet, if force broadcast is set, the packet will be broadcast to the client */
static int send_packet(struct dhcp_packet *dhcp_pkt, int force_broadcast)
{
	if (dhcp_pkt->gateway_nip)
		return send_packet_to_relay(dhcp_pkt);
	return send_packet_to_client(dhcp_pkt, force_broadcast);
}


static void init_packet(struct dhcp_packet *packet, struct dhcp_packet *oldpacket, char type)
{
	udhcp_init_header(packet, type);
	packet->xid = oldpacket->xid;
	memcpy(packet->chaddr, oldpacket->chaddr, sizeof(oldpacket->chaddr));
	packet->flags = oldpacket->flags;
	packet->gateway_nip = oldpacket->gateway_nip;
	packet->ciaddr = oldpacket->ciaddr;
	add_simple_option(packet->options, DHCP_SERVER_ID, server_config.server_nip);
}


/* add in the bootp options */
static void add_bootp_options(struct dhcp_packet *packet)
{
	packet->siaddr_nip = server_config.siaddr_nip;
	if (server_config.sname)
		strncpy((char*)packet->sname, server_config.sname, sizeof(packet->sname) - 1);
	if (server_config.boot_file)
		strncpy((char*)packet->file, server_config.boot_file, sizeof(packet->file) - 1);
}


static uint32_t select_lease_time(struct dhcp_packet *packet)
{
	uint32_t lease_time_sec = server_config.max_lease_sec;
	uint8_t *lease_time_opt = get_option(packet, DHCP_LEASE_TIME);
	if (lease_time_opt) {
		move_from_unaligned32(lease_time_sec, lease_time_opt);
		lease_time_sec = ntohl(lease_time_sec);
		if (lease_time_sec > server_config.max_lease_sec)
			lease_time_sec = server_config.max_lease_sec;
		if (lease_time_sec < server_config.min_lease_sec)
			lease_time_sec = server_config.min_lease_sec;
	}
	return lease_time_sec;
}


/* send a DHCP OFFER to a DHCP DISCOVER */
int FAST_FUNC send_offer(struct dhcp_packet *oldpacket)
{
	struct dhcp_packet packet;
	uint32_t req_nip;
	uint32_t lease_time_sec = server_config.max_lease_sec;
	uint32_t static_lease_ip;
	uint8_t *req_ip_opt;
	const char *p_host_name;
	struct option_set *curr;
	struct in_addr addr;

	init_packet(&packet, oldpacket, DHCPOFFER);

	static_lease_ip = get_static_nip_by_mac(server_config.static_leases, oldpacket->chaddr);

	/* ADDME: if static, short circuit */
	if (!static_lease_ip) {
		struct dyn_lease *lease;

		lease = find_lease_by_mac(oldpacket->chaddr);
		/* The client is in our lease/offered table */
		if (lease) {
			signed_leasetime_t tmp = lease->expires - time(NULL);
			if (tmp >= 0)
				lease_time_sec = tmp;
			packet.yiaddr = lease->lease_nip;
		}
		/* Or the client has requested an IP */
		else if ((req_ip_opt = get_option(oldpacket, DHCP_REQUESTED_IP)) != NULL
		 /* (read IP) */
		 && (move_from_unaligned32(req_nip, req_ip_opt), 1)
		 /* and the IP is in the lease range */
		 && ntohl(req_nip) >= server_config.start_ip
		 && ntohl(req_nip) <= server_config.end_ip
		 /* and is not already taken/offered */
		 && (!(lease = find_lease_by_nip(req_nip))
			/* or its taken, but expired */
			|| is_expired_lease(lease))
		) {
			packet.yiaddr = req_nip;
		}
		/* Otherwise, find a free IP */
		else {
			packet.yiaddr = find_free_or_expired_nip(oldpacket->chaddr);
		}

		if (!packet.yiaddr) {
			bb_error_msg("no IP addresses to give - OFFER abandoned");
			return -1;
		}
		p_host_name = (const char*) get_option(oldpacket, DHCP_HOST_NAME);
		if (add_lease(packet.chaddr, packet.yiaddr,
				server_config.offer_time,
				p_host_name,
				p_host_name ? (unsigned char)p_host_name[OPT_LEN - OPT_DATA] : 0
			) == 0
		) {
			bb_error_msg("lease pool is full - OFFER abandoned");
			return -1;
		}
		lease_time_sec = select_lease_time(oldpacket);
	} else {
		/* It is a static lease... use it */
		packet.yiaddr = static_lease_ip;
	}

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_sec));

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	addr.s_addr = packet.yiaddr;
	bb_info_msg("Sending OFFER of %s", inet_ntoa(addr));
	return send_packet(&packet, 0);
}


int FAST_FUNC send_NAK(struct dhcp_packet *oldpacket)
{
	struct dhcp_packet packet;

	init_packet(&packet, oldpacket, DHCPNAK);

	log1("Sending NAK");
	return send_packet(&packet, 1);
}


int FAST_FUNC send_ACK(struct dhcp_packet *oldpacket, uint32_t yiaddr)
{
	struct dhcp_packet packet;
	struct option_set *curr;
	uint32_t lease_time_sec;
	struct in_addr addr;
	const char *p_host_name;

	init_packet(&packet, oldpacket, DHCPACK);
	packet.yiaddr = yiaddr;

	lease_time_sec = select_lease_time(oldpacket);

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_sec));

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	addr.s_addr = packet.yiaddr;
	bb_info_msg("Sending ACK to %s", inet_ntoa(addr));

	if (send_packet(&packet, 0) < 0)
		return -1;

	p_host_name = (const char*) get_option(oldpacket, DHCP_HOST_NAME);
	add_lease(packet.chaddr, packet.yiaddr,
		lease_time_sec,
		p_host_name,
		p_host_name ? (unsigned char)p_host_name[OPT_LEN - OPT_DATA] : 0
	);
	if (ENABLE_FEATURE_UDHCPD_WRITE_LEASES_EARLY) {
		/* rewrite the file with leases at every new acceptance */
		write_leases();
	}

	return 0;
}


int FAST_FUNC send_inform(struct dhcp_packet *oldpacket)
{
	struct dhcp_packet packet;
	struct option_set *curr;

	init_packet(&packet, oldpacket, DHCPACK);

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	return send_packet(&packet, 0);
}
