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


/* send a packet to giaddr using the kernel ip stack */
static int send_packet_to_relay(struct dhcpMessage *payload)
{
	DEBUG("Forwarding packet to relay");

	return udhcp_send_kernel_packet(payload, server_config.server, SERVER_PORT,
			payload->giaddr, SERVER_PORT);
}


/* send a packet to a specific arp address and ip address by creating our own ip packet */
static int send_packet_to_client(struct dhcpMessage *payload, int force_broadcast)
{
	const uint8_t *chaddr;
	uint32_t ciaddr;

	if (force_broadcast) {
		DEBUG("broadcasting packet to client (NAK)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else if (payload->ciaddr) {
		DEBUG("unicasting packet to client ciaddr");
		ciaddr = payload->ciaddr;
		chaddr = payload->chaddr;
	} else if (payload->flags & htons(BROADCAST_FLAG)) {
		DEBUG("broadcasting packet to client (requested)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else {
		DEBUG("unicasting packet to client yiaddr");
		ciaddr = payload->yiaddr;
		chaddr = payload->chaddr;
	}
	return udhcp_send_raw_packet(payload,
		/*src*/ server_config.server, SERVER_PORT,
		/*dst*/ ciaddr, CLIENT_PORT, chaddr,
		server_config.ifindex);
}


/* send a dhcp packet, if force broadcast is set, the packet will be broadcast to the client */
static int send_packet(struct dhcpMessage *payload, int force_broadcast)
{
	if (payload->giaddr)
		return send_packet_to_relay(payload);
	return send_packet_to_client(payload, force_broadcast);
}


static void init_packet(struct dhcpMessage *packet, struct dhcpMessage *oldpacket, char type)
{
	udhcp_init_header(packet, type);
	packet->xid = oldpacket->xid;
	memcpy(packet->chaddr, oldpacket->chaddr, 16);
	packet->flags = oldpacket->flags;
	packet->giaddr = oldpacket->giaddr;
	packet->ciaddr = oldpacket->ciaddr;
	add_simple_option(packet->options, DHCP_SERVER_ID, server_config.server);
}


/* add in the bootp options */
static void add_bootp_options(struct dhcpMessage *packet)
{
	packet->siaddr = server_config.siaddr;
	if (server_config.sname)
		strncpy((char*)packet->sname, server_config.sname, sizeof(packet->sname) - 1);
	if (server_config.boot_file)
		strncpy((char*)packet->file, server_config.boot_file, sizeof(packet->file) - 1);
}


/* send a DHCP OFFER to a DHCP DISCOVER */
int FAST_FUNC send_offer(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;
	uint32_t req_align;
	uint32_t lease_time_aligned = server_config.lease;
	uint32_t static_lease_ip;
	uint8_t *req, *lease_time, *p_host_name;
	struct option_set *curr;
	struct in_addr addr;

	init_packet(&packet, oldpacket, DHCPOFFER);

	static_lease_ip = getIpByMac(server_config.static_leases, oldpacket->chaddr);

	/* ADDME: if static, short circuit */
	if (!static_lease_ip) {
		struct dhcpOfferedAddr *lease;

		lease = find_lease_by_chaddr(oldpacket->chaddr);
		/* the client is in our lease/offered table */
		if (lease) {
			signed_leasetime_t tmp = lease->expires - time(NULL);
			if (tmp >= 0)
				lease_time_aligned = tmp;
			packet.yiaddr = lease->yiaddr;
		/* Or the client has requested an ip */
		} else if ((req = get_option(oldpacket, DHCP_REQUESTED_IP)) != NULL
		 /* Don't look here (ugly hackish thing to do) */
		 && (move_from_unaligned32(req_align, req), 1)
		 /* and the ip is in the lease range */
		 && ntohl(req_align) >= server_config.start_ip
		 && ntohl(req_align) <= server_config.end_ip
		 /* and is not already taken/offered */
		 && (!(lease = find_lease_by_yiaddr(req_align))
			/* or its taken, but expired */
			|| lease_expired(lease))
		) {
			packet.yiaddr = req_align;
		/* otherwise, find a free IP */
		} else {
			packet.yiaddr = find_free_or_expired_address();
		}

		if (!packet.yiaddr) {
			bb_error_msg("no IP addresses to give - OFFER abandoned");
			return -1;
		}
		p_host_name = get_option(oldpacket, DHCP_HOST_NAME);
		if (!add_lease(packet.chaddr, packet.yiaddr, server_config.offer_time, p_host_name)) {
			bb_error_msg("lease pool is full - OFFER abandoned");
			return -1;
		}
		lease_time = get_option(oldpacket, DHCP_LEASE_TIME);
		if (lease_time) {
			move_from_unaligned32(lease_time_aligned, lease_time);
			lease_time_aligned = ntohl(lease_time_aligned);
			if (lease_time_aligned > server_config.lease)
				lease_time_aligned = server_config.lease;
		}

		/* Make sure we aren't just using the lease time from the previous offer */
		if (lease_time_aligned < server_config.min_lease)
			lease_time_aligned = server_config.min_lease;
	} else {
		/* It is a static lease... use it */
		packet.yiaddr = static_lease_ip;
	}

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_aligned));

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


int FAST_FUNC send_NAK(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;

	init_packet(&packet, oldpacket, DHCPNAK);

	DEBUG("Sending NAK");
	return send_packet(&packet, 1);
}


int FAST_FUNC send_ACK(struct dhcpMessage *oldpacket, uint32_t yiaddr)
{
	struct dhcpMessage packet;
	struct option_set *curr;
	uint8_t *lease_time;
	uint32_t lease_time_aligned = server_config.lease;
	struct in_addr addr;
	uint8_t *p_host_name;

	init_packet(&packet, oldpacket, DHCPACK);
	packet.yiaddr = yiaddr;

	lease_time = get_option(oldpacket, DHCP_LEASE_TIME);
	if (lease_time) {
		move_from_unaligned32(lease_time_aligned, lease_time);
		lease_time_aligned = ntohl(lease_time_aligned);
		if (lease_time_aligned > server_config.lease)
			lease_time_aligned = server_config.lease;
		else if (lease_time_aligned < server_config.min_lease)
			lease_time_aligned = server_config.min_lease;
	}

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_aligned));

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

	p_host_name = get_option(oldpacket, DHCP_HOST_NAME);
	add_lease(packet.chaddr, packet.yiaddr, lease_time_aligned, p_host_name);
	if (ENABLE_FEATURE_UDHCPD_WRITE_LEASES_EARLY) {
		/* rewrite the file with leases at every new acceptance */
		write_leases();
	}

	return 0;
}


int FAST_FUNC send_inform(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;
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
