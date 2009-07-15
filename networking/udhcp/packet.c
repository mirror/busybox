/* vi: set sw=4 ts=4: */
/*
 * packet.c -- packet ops
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */
#include <netinet/in.h>
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
#include "options.h"

void FAST_FUNC udhcp_init_header(struct dhcp_packet *packet, char type)
{
	memset(packet, 0, sizeof(struct dhcp_packet));
	packet->op = BOOTREQUEST; /* if client to a server */
	switch (type) {
	case DHCPOFFER:
	case DHCPACK:
	case DHCPNAK:
		packet->op = BOOTREPLY; /* if server to client */
	}
	packet->htype = ETH_10MB;
	packet->hlen = ETH_10MB_LEN;
	packet->cookie = htonl(DHCP_MAGIC);
	packet->options[0] = DHCP_END;
	add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);
}

#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 2
void FAST_FUNC udhcp_dump_packet(struct dhcp_packet *packet)
{
	char buf[sizeof(packet->chaddr)*2 + 1];

	if (dhcp_verbose < 2)
		return;

	bb_info_msg(
		//" op %x"
		//" htype %x"
		" hlen %x"
		//" hops %x"
		" xid %x"
		//" secs %x"
		//" flags %x"
		" ciaddr %x"
		" yiaddr %x"
		" siaddr %x"
		" giaddr %x"
		//" chaddr %s"
		//" sname %s"
		//" file %s"
		//" cookie %x"
		//" options %s"
		//, packet->op
		//, packet->htype
		, packet->hlen
		//, packet->hops
		, packet->xid
		//, packet->secs
		//, packet->flags
		, packet->ciaddr
		, packet->yiaddr
		, packet->siaddr_nip
		, packet->gateway_nip
		//, packet->chaddr[16]
		//, packet->sname[64]
		//, packet->file[128]
		//, packet->cookie
		//, packet->options[]
	);
	*bin2hex(buf, (void *) packet->chaddr, sizeof(packet->chaddr)) = '\0';
	bb_info_msg(" chaddr %s", buf);
}
#endif

/* Read a packet from socket fd, return -1 on read error, -2 on packet error */
int FAST_FUNC udhcp_recv_kernel_packet(struct dhcp_packet *packet, int fd)
{
	int bytes;
	unsigned char *vendor;

	memset(packet, 0, sizeof(*packet));
	bytes = safe_read(fd, packet, sizeof(*packet));
	if (bytes < 0) {
		log1("Packet read error, ignoring");
		return bytes; /* returns -1 */
	}

	if (packet->cookie != htonl(DHCP_MAGIC)) {
		bb_info_msg("Packet with bad magic, ignoring");
		return -2;
	}
	log1("Received a packet");
	udhcp_dump_packet(packet);

	if (packet->op == BOOTREQUEST) {
		vendor = get_option(packet, DHCP_VENDOR);
		if (vendor) {
#if 0
			static const char broken_vendors[][8] = {
				"MSFT 98",
				""
			};
			int i;
			for (i = 0; broken_vendors[i][0]; i++) {
				if (vendor[OPT_LEN - 2] == (uint8_t)strlen(broken_vendors[i])
				 && !strncmp((char*)vendor, broken_vendors[i], vendor[OPT_LEN - 2])
				) {
					log1("Broken client (%s), forcing broadcast replies",
						broken_vendors[i]);
					packet->flags |= htons(BROADCAST_FLAG);
				}
			}
#else
			if (vendor[OPT_LEN - 2] == (uint8_t)(sizeof("MSFT 98")-1)
			 && memcmp(vendor, "MSFT 98", sizeof("MSFT 98")-1) == 0
			) {
				log1("Broken client (%s), forcing broadcast replies", "MSFT 98");
				packet->flags |= htons(BROADCAST_FLAG);
			}
#endif
		}
	}

	return bytes;
}

uint16_t FAST_FUNC udhcp_checksum(void *addr, int count)
{
	/* Compute Internet Checksum for "count" bytes
	 * beginning at location "addr".
	 */
	int32_t sum = 0;
	uint16_t *source = (uint16_t *) addr;

	while (count > 1)  {
		/*  This is the inner loop */
		sum += *source++;
		count -= 2;
	}

	/*  Add left-over byte, if any */
	if (count > 0) {
		/* Make sure that the left-over byte is added correctly both
		 * with little and big endian hosts */
		uint16_t tmp = 0;
		*(uint8_t*)&tmp = *(uint8_t*)source;
		sum += tmp;
	}
	/*  Fold 32-bit sum to 16 bits */
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum;
}

/* Construct a ip/udp header for a packet, send packet */
int FAST_FUNC udhcp_send_raw_packet(struct dhcp_packet *dhcp_pkt,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port, const uint8_t *dest_arp,
		int ifindex)
{
	struct sockaddr_ll dest;
	struct ip_udp_dhcp_packet packet;
	int fd;
	int result = -1;
	const char *msg;

	enum {
		IP_UPD_DHCP_SIZE = sizeof(struct ip_udp_dhcp_packet) - CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS,
		UPD_DHCP_SIZE    = IP_UPD_DHCP_SIZE - offsetof(struct ip_udp_dhcp_packet, udp),
	};

	fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	if (fd < 0) {
		msg = "socket(%s)";
		goto ret_msg;
	}

	memset(&dest, 0, sizeof(dest));
	memset(&packet, 0, sizeof(packet));
	packet.data = *dhcp_pkt; /* struct copy */

	dest.sll_family = AF_PACKET;
	dest.sll_protocol = htons(ETH_P_IP);
	dest.sll_ifindex = ifindex;
	dest.sll_halen = 6;
	memcpy(dest.sll_addr, dest_arp, 6);
	if (bind(fd, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
		msg = "bind(%s)";
		goto ret_close;
	}

	packet.ip.protocol = IPPROTO_UDP;
	packet.ip.saddr = source_ip;
	packet.ip.daddr = dest_ip;
	packet.udp.source = htons(source_port);
	packet.udp.dest = htons(dest_port);
	/* size, excluding IP header: */
	packet.udp.len = htons(UPD_DHCP_SIZE);
	/* for UDP checksumming, ip.len is set to UDP packet len */
	packet.ip.tot_len = packet.udp.len;
	packet.udp.check = udhcp_checksum(&packet, IP_UPD_DHCP_SIZE);
	/* but for sending, it is set to IP packet len */
	packet.ip.tot_len = htons(IP_UPD_DHCP_SIZE);
	packet.ip.ihl = sizeof(packet.ip) >> 2;
	packet.ip.version = IPVERSION;
	packet.ip.ttl = IPDEFTTL;
	packet.ip.check = udhcp_checksum(&packet.ip, sizeof(packet.ip));

	/* Currently we send full-sized DHCP packets (zero padded).
	 * If you need to change this: last byte of the packet is
	 * packet.data.options[end_option(packet.data.options)]
	 */
	udhcp_dump_packet(dhcp_pkt);
	result = sendto(fd, &packet, IP_UPD_DHCP_SIZE, 0,
				(struct sockaddr *) &dest, sizeof(dest));
	msg = "sendto";
 ret_close:
	close(fd);
	if (result < 0) {
 ret_msg:
		bb_perror_msg(msg, "PACKET");
	}
	return result;
}

/* Let the kernel do all the work for packet generation */
int FAST_FUNC udhcp_send_kernel_packet(struct dhcp_packet *dhcp_pkt,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port)
{
	struct sockaddr_in client;
	int fd;
	int result = -1;
	const char *msg;

	enum {
		DHCP_SIZE = sizeof(struct dhcp_packet) - CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS,
	};

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		msg = "socket(%s)";
		goto ret_msg;
	}
	setsockopt_reuseaddr(fd);

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(source_port);
	client.sin_addr.s_addr = source_ip;
	if (bind(fd, (struct sockaddr *)&client, sizeof(client)) == -1) {
		msg = "bind(%s)";
		goto ret_close;
	}

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(dest_port);
	client.sin_addr.s_addr = dest_ip;
	if (connect(fd, (struct sockaddr *)&client, sizeof(client)) == -1) {
		msg = "connect";
		goto ret_close;
	}

	/* Currently we send full-sized DHCP packets (see above) */
	udhcp_dump_packet(dhcp_pkt);
	result = safe_write(fd, dhcp_pkt, DHCP_SIZE);
	msg = "write";
 ret_close:
	close(fd);
	if (result < 0) {
 ret_msg:
		bb_perror_msg(msg, "UDP");
	}
	return result;
}
