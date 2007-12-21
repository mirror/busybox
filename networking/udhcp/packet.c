/* vi: set sw=4 ts=4: */

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


void udhcp_init_header(struct dhcpMessage *packet, char type)
{
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
}


/* read a packet from socket fd, return -1 on read error, -2 on packet error */
int udhcp_recv_packet(struct dhcpMessage *packet, int fd)
{
#if 0
	static const char broken_vendors[][8] = {
		"MSFT 98",
		""
	};
#endif
	int bytes;
	unsigned char *vendor;

	memset(packet, 0, sizeof(*packet));
	bytes = read(fd, packet, sizeof(*packet));
	if (bytes < 0) {
		DEBUG("cannot read on listening socket, ignoring");
		return -1;
	}

	if (packet->cookie != htonl(DHCP_MAGIC)) {
		bb_error_msg("received bogus message, ignoring");
		return -2;
	}
	DEBUG("Received a packet");

	if (packet->op == BOOTREQUEST) {
		vendor = get_option(packet, DHCP_VENDOR);
		if (vendor) {
#if 0
			int i;
			for (i = 0; broken_vendors[i][0]; i++) {
				if (vendor[OPT_LEN - 2] == (uint8_t)strlen(broken_vendors[i])
				 && !strncmp((char*)vendor, broken_vendors[i], vendor[OPT_LEN - 2])
				) {
					DEBUG("broken client (%s), forcing broadcast",
						broken_vendors[i]);
					packet->flags |= htons(BROADCAST_FLAG);
				}
			}
#else
			if (vendor[OPT_LEN - 2] == (uint8_t)(sizeof("MSFT 98")-1)
			 && memcmp(vendor, "MSFT 98", sizeof("MSFT 98")-1) == 0
			) {
				DEBUG("broken client (%s), forcing broadcast", "MSFT 98");
				packet->flags |= htons(BROADCAST_FLAG);
			}
#endif
		}
	}

	return bytes;
}


uint16_t udhcp_checksum(void *addr, int count)
{
	/* Compute Internet Checksum for "count" bytes
	 *         beginning at location "addr".
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
int udhcp_send_raw_packet(struct dhcpMessage *payload,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port, const uint8_t *dest_arp, int ifindex)
{
	int fd;
	int result;
	struct sockaddr_ll dest;
	struct udp_dhcp_packet packet;

	enum {
		IP_UPD_DHCP_SIZE = sizeof(struct udp_dhcp_packet) - CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS,
		UPD_DHCP_SIZE    = IP_UPD_DHCP_SIZE - offsetof(struct udp_dhcp_packet, udp),
	};

	fd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	if (fd < 0) {
		bb_perror_msg("socket");
		return -1;
	}

	memset(&dest, 0, sizeof(dest));
	memset(&packet, 0, sizeof(packet));
	packet.data = *payload; /* struct copy */

	dest.sll_family = AF_PACKET;
	dest.sll_protocol = htons(ETH_P_IP);
	dest.sll_ifindex = ifindex;
	dest.sll_halen = 6;
	memcpy(dest.sll_addr, dest_arp, 6);
	if (bind(fd, (struct sockaddr *)&dest, sizeof(struct sockaddr_ll)) < 0) {
		bb_perror_msg("bind");
		close(fd);
		return -1;
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
	result = sendto(fd, &packet, IP_UPD_DHCP_SIZE, 0, (struct sockaddr *) &dest, sizeof(dest));
	if (result <= 0) {
		bb_perror_msg("sendto");
	}
	close(fd);
	return result;
}


/* Let the kernel do all the work for packet generation */
int udhcp_send_kernel_packet(struct dhcpMessage *payload,
		uint32_t source_ip, int source_port,
		uint32_t dest_ip, int dest_port)
{
	int fd, result;
	struct sockaddr_in client;

	enum {
		DHCP_SIZE = sizeof(struct dhcpMessage) - CONFIG_UDHCPC_SLACK_FOR_BUGGY_SERVERS,
	};

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0)
		return -1;

	setsockopt_reuseaddr(fd);

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(source_port);
	client.sin_addr.s_addr = source_ip;

	if (bind(fd, (struct sockaddr *)&client, sizeof(client)) == -1) {
		close(fd);
		return -1;
	}

	memset(&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_port = htons(dest_port);
	client.sin_addr.s_addr = dest_ip;

	if (connect(fd, (struct sockaddr *)&client, sizeof(struct sockaddr)) == -1) {
		close(fd);
		return -1;
	}

	/* Currently we send full-sized DHCP packets (see above) */
	result = write(fd, payload, DHCP_SIZE);
	close(fd);
	return result;
}
