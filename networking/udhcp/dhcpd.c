/* vi: set sw=4 ts=4: */
/* dhcpd.c
 *
 * udhcp Server
 * Copyright (C) 1999 Matthew Ramsay <matthewr@moreton.com.au>
 *			Chris Trew <ctrew@moreton.com.au>
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <syslog.h>
#include "common.h"
#include "dhcpc.h"
#include "dhcpd.h"
#include "options.h"


/* globals */
struct dhcpOfferedAddr *leases;
/* struct server_config_t server_config is in bb_common_bufsiz1 */


int udhcpd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int udhcpd_main(int argc UNUSED_PARAM, char **argv)
{
	fd_set rfds;
	int server_socket = -1, retval, max_sock;
	struct dhcpMessage packet;
	uint8_t *state, *server_id, *requested;
	uint32_t server_id_aligned = server_id_aligned; /* for compiler */
	uint32_t requested_aligned = requested_aligned;
	uint32_t static_lease_ip;
	unsigned timeout_end;
	unsigned num_ips;
	unsigned opt;
	struct option_set *option;
	struct dhcpOfferedAddr *lease, static_lease;
	USE_FEATURE_UDHCP_PORT(char *str_P;)

#if ENABLE_FEATURE_UDHCP_PORT
	SERVER_PORT = 67;
	CLIENT_PORT = 68;
#endif

	opt = getopt32(argv, "fS" USE_FEATURE_UDHCP_PORT("P:", &str_P));
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
	/* if (!..) bb_perror_msg("cannot create pidfile %s", pidfile); */

	bb_info_msg("%s (v"BB_VER") started", applet_name);

	option = find_option(server_config.options, DHCP_LEASE_TIME);
	server_config.lease = LEASE_TIME;
	if (option) {
		move_from_unaligned32(server_config.lease, option->data + 2);
		server_config.lease = ntohl(server_config.lease);
	}

	/* Sanity check */
	num_ips = server_config.end_ip - server_config.start_ip + 1;
	if (server_config.max_leases > num_ips) {
		bb_error_msg("max_leases=%u is too big, setting to %u",
			(unsigned)server_config.max_leases, num_ips);
		server_config.max_leases = num_ips;
	}

	leases = xzalloc(server_config.max_leases * sizeof(*leases));
	read_leases(server_config.lease_file);

	if (udhcp_read_interface(server_config.interface, &server_config.ifindex,
			   &server_config.server, server_config.arp)) {
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
			DEBUG("error on select");
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
				DEBUG("error on read, %s, reopening socket", strerror(errno));
				close(server_socket);
				server_socket = -1;
			}
			continue;
		}

		state = get_option(&packet, DHCP_MESSAGE_TYPE);
		if (state == NULL) {
			bb_error_msg("cannot get option from packet, ignoring");
			continue;
		}

		/* Look for a static lease */
		static_lease_ip = getIpByMac(server_config.static_leases, &packet.chaddr);
		if (static_lease_ip) {
			bb_info_msg("Found static lease: %x", static_lease_ip);

			memcpy(&static_lease.chaddr, &packet.chaddr, 16);
			static_lease.yiaddr = static_lease_ip;
			static_lease.expires = 0;

			lease = &static_lease;
		} else {
			lease = find_lease_by_chaddr(packet.chaddr);
		}

		switch (state[0]) {
		case DHCPDISCOVER:
			DEBUG("Received DISCOVER");

			if (send_offer(&packet) < 0) {
				bb_error_msg("send OFFER failed");
			}
			break;
		case DHCPREQUEST:
			DEBUG("received REQUEST");

			requested = get_option(&packet, DHCP_REQUESTED_IP);
			server_id = get_option(&packet, DHCP_SERVER_ID);

			if (requested)
				move_from_unaligned32(requested_aligned, requested);
			if (server_id)
				move_from_unaligned32(server_id_aligned, server_id);

			if (lease) {
				if (server_id) {
					/* SELECTING State */
					DEBUG("server_id = %08x", ntohl(server_id_aligned));
					if (server_id_aligned == server_config.server
					 && requested
					 && requested_aligned == lease->yiaddr
					) {
						send_ACK(&packet, lease->yiaddr);
					}
				} else if (requested) {
					/* INIT-REBOOT State */
					if (lease->yiaddr == requested_aligned)
						send_ACK(&packet, lease->yiaddr);
					else
						send_NAK(&packet);
				} else if (lease->yiaddr == packet.ciaddr) {
					/* RENEWING or REBINDING State */
					send_ACK(&packet, lease->yiaddr);
				} else { /* don't know what to do!!!! */
					send_NAK(&packet);
				}

			/* what to do if we have no record of the client */
			} else if (server_id) {
				/* SELECTING State */

			} else if (requested) {
				/* INIT-REBOOT State */
				lease = find_lease_by_yiaddr(requested_aligned);
				if (lease) {
					if (lease_expired(lease)) {
						/* probably best if we drop this lease */
						memset(lease->chaddr, 0, 16);
					/* make some contention for this address */
					} else
						send_NAK(&packet);
				} else {
					uint32_t r = ntohl(requested_aligned);
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
		case DHCPDECLINE:
			DEBUG("Received DECLINE");
			if (lease) {
				memset(lease->chaddr, 0, 16);
				lease->expires = time(NULL) + server_config.decline_time;
			}
			break;
		case DHCPRELEASE:
			DEBUG("Received RELEASE");
			if (lease)
				lease->expires = time(NULL);
			break;
		case DHCPINFORM:
			DEBUG("Received INFORM");
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
