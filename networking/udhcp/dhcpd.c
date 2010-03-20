/* vi: set sw=4 ts=4: */
/*
 * udhcp Server
 * Copyright (C) 1999 Matthew Ramsay <matthewr@moreton.com.au>
 *			Chris Trew <ctrew@moreton.com.au>
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
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

#include <syslog.h>
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
static int send_offer(struct dhcp_packet *oldpacket, uint32_t static_lease_nip, struct dyn_lease *lease)
{
	struct dhcp_packet packet;
	uint32_t req_nip;
	uint32_t lease_time_sec = server_config.max_lease_sec;
	uint8_t *req_ip_opt;
	const char *p_host_name;
	struct option_set *curr;
	struct in_addr addr;

	init_packet(&packet, oldpacket, DHCPOFFER);

	/* ADDME: if static, short circuit */
	if (!static_lease_nip) {
		/* The client is in our lease/offered table */
		if (lease) {
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
		packet.yiaddr = static_lease_nip;
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

static int send_NAK(struct dhcp_packet *oldpacket)
{
	struct dhcp_packet packet;

	init_packet(&packet, oldpacket, DHCPNAK);

	log1("Sending NAK");
	return send_packet(&packet, 1);
}

static int send_ACK(struct dhcp_packet *oldpacket, uint32_t yiaddr)
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

static int send_inform(struct dhcp_packet *oldpacket)
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


/* globals */
struct dyn_lease *g_leases;
/* struct server_config_t server_config is in bb_common_bufsiz1 */


int udhcpd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int udhcpd_main(int argc UNUSED_PARAM, char **argv)
{
	fd_set rfds;
	int server_socket = -1, retval, max_sock;
	struct dhcp_packet packet;
	uint8_t *state;
	uint32_t static_lease_nip;
	unsigned timeout_end;
	unsigned num_ips;
	unsigned opt;
	struct option_set *option;
	struct dyn_lease *lease, fake_lease;
	IF_FEATURE_UDHCP_PORT(char *str_P;)

#if ENABLE_FEATURE_UDHCP_PORT
	SERVER_PORT = 67;
	CLIENT_PORT = 68;
#endif

#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
	opt_complementary = "vv";
#endif
	opt = getopt32(argv, "fSv"
		IF_FEATURE_UDHCP_PORT("P:", &str_P)
#if defined CONFIG_UDHCP_DEBUG && CONFIG_UDHCP_DEBUG >= 1
		, &dhcp_verbose
#endif
		);
	argv += optind;
	if (!(opt & 1)) { /* no -f */
		bb_daemonize_or_rexec(0, argv);
		logmode = LOGMODE_NONE;
	}
	if (opt & 2) { /* -S */
		openlog(applet_name, LOG_PID, LOG_DAEMON);
		logmode |= LOGMODE_SYSLOG;
	}
#if ENABLE_FEATURE_UDHCP_PORT
	if (opt & 4) { /* -P */
		SERVER_PORT = xatou16(str_P);
		CLIENT_PORT = SERVER_PORT + 1;
	}
#endif
	/* Would rather not do read_config before daemonization -
	 * otherwise NOMMU machines will parse config twice */
	read_config(argv[0] ? argv[0] : DHCPD_CONF_FILE);

	/* Make sure fd 0,1,2 are open */
	bb_sanitize_stdio();
	/* Equivalent of doing a fflush after every \n */
	setlinebuf(stdout);

	/* Create pidfile */
	write_pidfile(server_config.pidfile);
	/* if (!..) bb_perror_msg("can't create pidfile %s", pidfile); */

	bb_info_msg("%s (v"BB_VER") started", applet_name);

	option = find_option(server_config.options, DHCP_LEASE_TIME);
	server_config.max_lease_sec = LEASE_TIME;
	if (option) {
		move_from_unaligned32(server_config.max_lease_sec, option->data + OPT_DATA);
		server_config.max_lease_sec = ntohl(server_config.max_lease_sec);
	}

	/* Sanity check */
	num_ips = server_config.end_ip - server_config.start_ip + 1;
	if (server_config.max_leases > num_ips) {
		bb_error_msg("max_leases=%u is too big, setting to %u",
			(unsigned)server_config.max_leases, num_ips);
		server_config.max_leases = num_ips;
	}

	g_leases = xzalloc(server_config.max_leases * sizeof(g_leases[0]));
	read_leases(server_config.lease_file);

	if (udhcp_read_interface(server_config.interface,
			&server_config.ifindex,
			&server_config.server_nip,
			server_config.server_mac)
	) {
		retval = 1;
		goto ret;
	}

	/* Setup the signal pipe */
	udhcp_sp_setup();

	timeout_end = monotonic_sec() + server_config.auto_time;
	while (1) { /* loop until universe collapses */
		int bytes;
		struct timeval tv;

		if (server_socket < 0) {
			server_socket = udhcp_listen_socket(/*INADDR_ANY,*/ SERVER_PORT,
					server_config.interface);
		}

		max_sock = udhcp_sp_fd_set(&rfds, server_socket);
		if (server_config.auto_time) {
			tv.tv_sec = timeout_end - monotonic_sec();
			tv.tv_usec = 0;
		}
		retval = 0;
		if (!server_config.auto_time || tv.tv_sec > 0) {
			retval = select(max_sock + 1, &rfds, NULL, NULL,
					server_config.auto_time ? &tv : NULL);
		}
		if (retval == 0) {
			write_leases();
			timeout_end = monotonic_sec() + server_config.auto_time;
			continue;
		}
		if (retval < 0 && errno != EINTR) {
			log1("Error on select");
			continue;
		}

		switch (udhcp_sp_read(&rfds)) {
		case SIGUSR1:
			bb_info_msg("Received a SIGUSR1");
			write_leases();
			/* why not just reset the timeout, eh */
			timeout_end = monotonic_sec() + server_config.auto_time;
			continue;
		case SIGTERM:
			bb_info_msg("Received a SIGTERM");
			goto ret0;
		case 0:	/* no signal: read a packet */
			break;
		default: /* signal or error (probably EINTR): back to select */
			continue;
		}

		bytes = udhcp_recv_kernel_packet(&packet, server_socket);
		if (bytes < 0) {
			/* bytes can also be -2 ("bad packet data") */
			if (bytes == -1 && errno != EINTR) {
				log1("Read error: %s, reopening socket", strerror(errno));
				close(server_socket);
				server_socket = -1;
			}
			continue;
		}

		if (packet.hlen != 6) {
			bb_error_msg("MAC length != 6, ignoring packet");
			continue;
		}

		state = get_option(&packet, DHCP_MESSAGE_TYPE);
		if (state == NULL) {
			bb_error_msg("no message type option, ignoring packet");
			continue;
		}

		/* Look for a static lease */
		static_lease_nip = get_static_nip_by_mac(server_config.static_leases, &packet.chaddr);
		if (static_lease_nip) {
			bb_info_msg("Found static lease: %x", static_lease_nip);

			memcpy(&fake_lease.lease_mac, &packet.chaddr, 6);
			fake_lease.lease_nip = static_lease_nip;
			fake_lease.expires = 0;

			lease = &fake_lease;
		} else {
			lease = find_lease_by_mac(packet.chaddr);
		}

		switch (state[0]) {
		case DHCPDISCOVER:
			log1("Received DISCOVER");

			if (send_offer(&packet, static_lease_nip, lease) < 0) {
				bb_error_msg("send OFFER failed");
			}
			break;
		case DHCPREQUEST: {
			uint8_t *server_id_opt, *requested_opt;
			uint32_t server_id_net = server_id_net; /* for compiler */
			uint32_t requested_nip = requested_nip; /* for compiler */

			log1("Received REQUEST");

			requested_opt = get_option(&packet, DHCP_REQUESTED_IP);
			server_id_opt = get_option(&packet, DHCP_SERVER_ID);
			if (requested_opt)
				move_from_unaligned32(requested_nip, requested_opt);
			if (server_id_opt)
				move_from_unaligned32(server_id_net, server_id_opt);

			if (lease) {
				if (server_id_opt) {
					/* SELECTING State */
					if (server_id_net == server_config.server_nip
					 && requested_opt
					 && requested_nip == lease->lease_nip
					) {
						send_ACK(&packet, lease->lease_nip);
					}
				} else if (requested_opt) {
					/* INIT-REBOOT State */
					if (lease->lease_nip == requested_nip)
						send_ACK(&packet, lease->lease_nip);
					else
						send_NAK(&packet);
				} else if (lease->lease_nip == packet.ciaddr) {
					/* RENEWING or REBINDING State */
					send_ACK(&packet, lease->lease_nip);
				} else { /* don't know what to do!!!! */
					send_NAK(&packet);
				}

			/* what to do if we have no record of the client */
			} else if (server_id_opt) {
				/* SELECTING State */

			} else if (requested_opt) {
				/* INIT-REBOOT State */
				lease = find_lease_by_nip(requested_nip);
				if (lease) {
					if (is_expired_lease(lease)) {
						/* probably best if we drop this lease */
						memset(lease->lease_mac, 0, sizeof(lease->lease_mac));
					} else {
						/* make some contention for this address */
						send_NAK(&packet);
					}
				} else {
					uint32_t r = ntohl(requested_nip);
					if (r < server_config.start_ip
				         || r > server_config.end_ip
					) {
						send_NAK(&packet);
					}
					/* else remain silent */
				}

			} else {
				/* RENEWING or REBINDING State */
			}
			break;
		}
		case DHCPDECLINE:
			log1("Received DECLINE");
			if (lease) {
				memset(lease->lease_mac, 0, sizeof(lease->lease_mac));
				lease->expires = time(NULL) + server_config.decline_time;
			}
			break;
		case DHCPRELEASE:
			log1("Received RELEASE");
			if (lease)
				lease->expires = time(NULL);
			break;
		case DHCPINFORM:
			log1("Received INFORM");
			send_inform(&packet);
			break;
		default:
			bb_info_msg("Unsupported DHCP message (%02x) - ignoring", state[0]);
		}
	}
 ret0:
	retval = 0;
 ret:
	/*if (server_config.pidfile) - server_config.pidfile is never NULL */
		remove_pidfile(server_config.pidfile);
	return retval;
}
