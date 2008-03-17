/* vi: set sw=4 ts=4: */
/* dhcpc.c
 *
 * udhcp DHCP client
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include <getopt.h>
#include <syslog.h>

/* Override ENABLE_FEATURE_PIDFILE - ifupdown needs our pidfile to always exist */
#define WANT_PIDFILE 1
#include "common.h"
#include "dhcpd.h"
#include "dhcpc.h"
#include "options.h"


static int timeout; /* = 0. Must be signed */
static uint32_t requested_ip; /* = 0 */
static uint32_t server_addr;
static int sockfd = -1;

#define LISTEN_NONE 0
#define LISTEN_KERNEL 1
#define LISTEN_RAW 2
static smallint listen_mode;

static smallint state;

/* struct client_config_t client_config is in bb_common_bufsiz1 */


/* just a little helper */
static void change_listen_mode(int new_mode)
{
	DEBUG("entering %s listen mode",
		new_mode ? (new_mode == 1 ? "kernel" : "raw") : "none");
	if (sockfd >= 0) {
		close(sockfd);
		sockfd = -1;
	}
	listen_mode = new_mode;
}


/* perform a renew */
static void perform_renew(void)
{
	bb_info_msg("Performing a DHCP renew");
	switch (state) {
	case BOUND:
		change_listen_mode(LISTEN_KERNEL);
	case RENEWING:
	case REBINDING:
		state = RENEW_REQUESTED;
		break;
	case RENEW_REQUESTED: /* impatient are we? fine, square 1 */
		udhcp_run_script(NULL, "deconfig");
	case REQUESTING:
	case RELEASED:
		change_listen_mode(LISTEN_RAW);
		state = INIT_SELECTING;
		break;
	case INIT_SELECTING:
		break;
	}
}


/* perform a release */
static void perform_release(void)
{
	char buffer[sizeof("255.255.255.255")];
	struct in_addr temp_addr;

	/* send release packet */
	if (state == BOUND || state == RENEWING || state == REBINDING) {
		temp_addr.s_addr = server_addr;
		strcpy(buffer, inet_ntoa(temp_addr));
		temp_addr.s_addr = requested_ip;
		bb_info_msg("Unicasting a release of %s to %s",
				inet_ntoa(temp_addr), buffer);
		send_release(server_addr, requested_ip); /* unicast */
		udhcp_run_script(NULL, "deconfig");
	}
	bb_info_msg("Entering released state");

	change_listen_mode(LISTEN_NONE);
	state = RELEASED;
	timeout = INT_MAX;
}


static void client_background(void)
{
#if !BB_MMU
	bb_error_msg("cannot background in uclinux (yet)");
/* ... mainly because udhcpc calls client_background()
 * in _the _middle _of _udhcpc _run_, not at the start!
 * If that will be properly disabled for NOMMU, client_background()
 * will work on NOMMU too */
#else
	bb_daemonize(0);
	logmode &= ~LOGMODE_STDIO;
	/* rewrite pidfile, as our pid is different now */
	write_pidfile(client_config.pidfile);
#endif
	/* Do not fork again. */
	client_config.foreground = 1;
	client_config.background_if_no_lease = 0;
}


static uint8_t* alloc_dhcp_option(int code, const char *str, int extra)
{
	uint8_t *storage;
	int len = strlen(str);
	if (len > 255) len = 255;
	storage = xzalloc(len + extra + OPT_DATA);
	storage[OPT_CODE] = code;
	storage[OPT_LEN] = len + extra;
	memcpy(storage + extra + OPT_DATA, str, len);
	return storage;
}


int udhcpc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int udhcpc_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	uint8_t *temp, *message;
	char *str_c, *str_V, *str_h, *str_F, *str_r;
	USE_FEATURE_UDHCP_PORT(char *str_P;)
	llist_t *list_O = NULL;
#if ENABLE_FEATURE_UDHCPC_ARPING
	char *str_W;
#endif
	int tryagain_timeout = 20;
	int discover_timeout = 3;
	int discover_retries = 3;
	uint32_t xid = 0;
	uint32_t lease_seconds = 0; /* can be given as 32-bit quantity */
	int packet_num;
	/* t1, t2... what a wonderful names... */
	unsigned t1 = t1; /* for gcc */
	unsigned t2 = t2;
	unsigned timestamp_got_lease = timestamp_got_lease;
	unsigned opt;
	int max_fd;
	int retval;
	int len;
	struct timeval tv;
	struct in_addr temp_addr;
	struct dhcpMessage packet;
	fd_set rfds;

	enum {
		OPT_c = 1 << 0,
		OPT_C = 1 << 1,
		OPT_V = 1 << 2,
		OPT_f = 1 << 3,
		OPT_b = 1 << 4,
		OPT_H = 1 << 5,
		OPT_h = 1 << 6,
		OPT_F = 1 << 7,
		OPT_i = 1 << 8,
		OPT_n = 1 << 9,
		OPT_p = 1 << 10,
		OPT_q = 1 << 11,
		OPT_R = 1 << 12,
		OPT_r = 1 << 13,
		OPT_s = 1 << 14,
		OPT_T = 1 << 15,
		OPT_t = 1 << 16,
		OPT_v = 1 << 17,
		OPT_S = 1 << 18,
		OPT_A = 1 << 19,
#if ENABLE_FEATURE_UDHCPC_ARPING
		OPT_a = 1 << 20,
		OPT_W = 1 << 21,
#endif
		OPT_P = 1 << 22,
	};
#if ENABLE_GETOPT_LONG
	static const char udhcpc_longopts[] ALIGN1 =
		"clientid\0"       Required_argument "c"
		"clientid-none\0"  No_argument       "C"
		"vendorclass\0"    Required_argument "V"
		"foreground\0"     No_argument       "f"
		"background\0"     No_argument       "b"
		"hostname\0"       Required_argument "H"
		"fqdn\0"           Required_argument "F"
		"interface\0"      Required_argument "i"
		"now\0"            No_argument       "n"
		"pidfile\0"        Required_argument "p"
		"quit\0"           No_argument       "q"
		"release\0"        No_argument       "R"
		"request\0"        Required_argument "r"
		"script\0"         Required_argument "s"
		"timeout\0"        Required_argument "T"
		"version\0"        No_argument       "v"
		"retries\0"        Required_argument "t"
		"tryagain\0"       Required_argument "A"
		"syslog\0"         No_argument       "S"
#if ENABLE_FEATURE_UDHCPC_ARPING
		"arping\0"         No_argument       "a"
#endif
		"request-option\0" Required_argument "O"
#if ENABLE_FEATURE_UDHCP_PORT
		"client-port\0"	   Required_argument "P"
#endif
		;
#endif
	/* Default options. */
#if ENABLE_FEATURE_UDHCP_PORT
	SERVER_PORT = 67;
	CLIENT_PORT = 68;
#endif
	client_config.interface = "eth0";
	client_config.script = DEFAULT_SCRIPT;

	/* Parse command line */
	/* Cc: mutually exclusive; O: list; -T,-t,-A take numeric param */
	opt_complementary = "c--C:C--c:O::T+:t+:A+";
#if ENABLE_GETOPT_LONG
	applet_long_options = udhcpc_longopts;
#endif
	opt = getopt32(argv, "c:CV:fbH:h:F:i:np:qRr:s:T:t:vSA:"
		USE_FEATURE_UDHCPC_ARPING("aW:")
		USE_FEATURE_UDHCP_PORT("P:")
		"O:"
		, &str_c, &str_V, &str_h, &str_h, &str_F
		, &client_config.interface, &client_config.pidfile, &str_r
		, &client_config.script
		, &discover_timeout, &discover_retries, &tryagain_timeout
		USE_FEATURE_UDHCPC_ARPING(, &str_W)
		USE_FEATURE_UDHCP_PORT(, &str_P)
		, &list_O
		);

	if (opt & OPT_c)
		client_config.clientid = alloc_dhcp_option(DHCP_CLIENT_ID, str_c, 0);
	//if (opt & OPT_C)
	if (opt & OPT_V)
		client_config.vendorclass = alloc_dhcp_option(DHCP_VENDOR, str_V, 0);
	if (opt & OPT_f)
		client_config.foreground = 1;
	if (opt & OPT_b)
		client_config.background_if_no_lease = 1;
	if (opt & (OPT_h|OPT_H))
		client_config.hostname = alloc_dhcp_option(DHCP_HOST_NAME, str_h, 0);
	if (opt & OPT_F) {
		client_config.fqdn = alloc_dhcp_option(DHCP_FQDN, str_F, 3);
		/* Flags: 0000NEOS
		S: 1 => Client requests Server to update A RR in DNS as well as PTR
		O: 1 => Server indicates to client that DNS has been updated regardless
		E: 1 => Name data is DNS format, i.e. <4>host<6>domain<4>com<0> not "host.domain.com"
		N: 1 => Client requests Server to not update DNS
		*/
		client_config.fqdn[OPT_DATA + 0] = 0x1;
		/* client_config.fqdn[OPT_DATA + 1] = 0; - redundant */
		/* client_config.fqdn[OPT_DATA + 2] = 0; - redundant */
	}
	// if (opt & OPT_i) client_config.interface = ...
	if (opt & OPT_n)
		client_config.abort_if_no_lease = 1;
	// if (opt & OPT_p) client_config.pidfile = ...
	if (opt & OPT_q)
		client_config.quit_after_lease = 1;
	if (opt & OPT_R)
		client_config.release_on_quit = 1;
	if (opt & OPT_r)
		requested_ip = inet_addr(str_r);
	// if (opt & OPT_s) client_config.script = ...
	// if (opt & OPT_T) discover_timeout = xatoi_u(str_T);
	// if (opt & OPT_t) discover_retries = xatoi_u(str_t);
	// if (opt & OPT_A) tryagain_timeout = xatoi_u(str_A);
	if (opt & OPT_v) {
		puts("version "BB_VER);
		return 0;
	}
	if (opt & OPT_S) {
		openlog(applet_name, LOG_PID, LOG_LOCAL0);
		logmode |= LOGMODE_SYSLOG;
	}
#if ENABLE_FEATURE_UDHCP_PORT
	if (opt & OPT_P) {
		CLIENT_PORT = xatou16(str_P);
		SERVER_PORT = CLIENT_PORT - 1;
	}
#endif
	while (list_O) {
		int n = index_in_strings(dhcp_option_strings, list_O->data);
		if (n < 0)
			bb_error_msg_and_die("unknown option '%s'", list_O->data);
		n = dhcp_options[n].code;
		client_config.opt_mask[n >> 3] |= 1 << (n & 7);
		list_O = list_O->link;
	}

	if (read_interface(client_config.interface, &client_config.ifindex,
			   NULL, client_config.arp))
		return 1;

	/* Make sure fd 0,1,2 are open */
	bb_sanitize_stdio();
	/* Equivalent of doing a fflush after every \n */
	setlinebuf(stdout);

	/* Create pidfile */
	write_pidfile(client_config.pidfile);
	/* if (!..) bb_perror_msg("cannot create pidfile %s", pidfile); */

	/* Goes to stdout and possibly syslog */
	bb_info_msg("%s (v"BB_VER") started", applet_name);

	/* if not set, and not suppressed, setup the default client ID */
	if (!client_config.clientid && !(opt & OPT_C)) {
		client_config.clientid = alloc_dhcp_option(DHCP_CLIENT_ID, "", 7);
		client_config.clientid[OPT_DATA] = 1;
		memcpy(client_config.clientid + OPT_DATA+1, client_config.arp, 6);
	}

	if (!client_config.vendorclass)
		client_config.vendorclass = alloc_dhcp_option(DHCP_VENDOR, "udhcp "BB_VER, 0);

	/* setup the signal pipe */
	udhcp_sp_setup();

	state = INIT_SELECTING;
	udhcp_run_script(NULL, "deconfig");
	change_listen_mode(LISTEN_RAW);
	packet_num = 0;

	/* Main event loop. select() waits on signal pipe and possibly
	 * on sockfd.
	 * "continue" statements in code below jump to the top of the loop.
	 */
	for (;;) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		if (listen_mode != LISTEN_NONE && sockfd < 0) {
			if (listen_mode == LISTEN_KERNEL)
				sockfd = listen_socket(/*INADDR_ANY,*/ CLIENT_PORT, client_config.interface);
			else
				sockfd = raw_socket(client_config.ifindex);
		}
		max_fd = udhcp_sp_fd_set(&rfds, sockfd);

		retval = 0; /* If we already timed out, fall through, else... */
		if (tv.tv_sec > 0) {
			DEBUG("Waiting on select...");
			retval = select(max_fd + 1, &rfds, NULL, NULL, &tv);
		}

		if (retval < 0) {
			/* EINTR? signal was caught, don't panic */
			if (errno != EINTR) {
				/* Else: an error occured, panic! */
				bb_perror_msg_and_die("select");
			}
			continue;
		}

		/* If timeout dropped to zero, time to become active:
		 * resend discover/renew/whatever
		 */
		if (retval == 0) {
			switch (state) {
			case INIT_SELECTING:
				if (packet_num < discover_retries) {
					if (packet_num == 0)
						xid = random_xid();

					/* send discover packet */
					send_discover(xid, requested_ip); /* broadcast */

					timeout = discover_timeout;
					packet_num++;
					continue;
				}
				udhcp_run_script(NULL, "leasefail");
				if (client_config.background_if_no_lease) {
					bb_info_msg("No lease, forking to background");
					client_background();
				} else if (client_config.abort_if_no_lease) {
					bb_info_msg("No lease, failing");
					retval = 1;
					goto ret;
				}
				/* wait to try again */
				timeout = tryagain_timeout;
				packet_num = 0;
				continue;
			case RENEW_REQUESTED:
			case REQUESTING:
				if (packet_num < discover_retries) {
					/* send request packet */
					if (state == RENEW_REQUESTED)
						send_renew(xid, server_addr, requested_ip); /* unicast */
					else send_selecting(xid, server_addr, requested_ip); /* broadcast */

					timeout = ((packet_num == 2) ? 10 : 2);
					packet_num++;
					continue;
				}
				/* timed out, go back to init state */
				if (state == RENEW_REQUESTED)
					udhcp_run_script(NULL, "deconfig");
				change_listen_mode(LISTEN_RAW);
				state = INIT_SELECTING;
				timeout = 0;
				packet_num = 0;
				continue;
			case BOUND:
				/* Lease is starting to run out, time to enter renewing state */
				change_listen_mode(LISTEN_KERNEL);
				DEBUG("Entering renew state");
				state = RENEWING;
				/* fall right through */
			case RENEWING:
				/* Either set a new T1, or enter REBINDING state */
				if ((t2 - t1) > (lease_seconds / (4*60*60) + 1)) {
					/* send a request packet */
					send_renew(xid, server_addr, requested_ip); /* unicast */
					t1 += (t2 - t1) / 2;
					timeout = t1 - ((int)monotonic_sec() - timestamp_got_lease);
					continue;
				}
				/* Timed out, enter rebinding state */
				DEBUG("Entering rebinding state");
				state = REBINDING;
				timeout = (t2 - t1);
				continue;
			case REBINDING:
				/* Lease is *really* about to run out,
				 * try to find DHCP server using broadcast */
				if ((lease_seconds - t2) > (lease_seconds / (4*60*60) + 1)) {
					/* send a request packet */
					send_renew(xid, 0, requested_ip); /* broadcast */
					t2 += (lease_seconds - t2) / 2;
					timeout = t2 - ((int)monotonic_sec() - timestamp_got_lease);
					continue;
				}
				/* Timed out, enter init state */
				bb_info_msg("Lease lost, entering init state");
				udhcp_run_script(NULL, "deconfig");
				change_listen_mode(LISTEN_RAW);
				state = INIT_SELECTING;
				timeout = 0;
				packet_num = 0;
				continue;
			/* case RELEASED: */
			}
			/* yah, I know, *you* say it would never happen */
			timeout = INT_MAX;
			continue; /* back to main loop */
		}

		/* select() didn't timeout, something did happen. */
		/* Is is a packet? */
		if (listen_mode != LISTEN_NONE && FD_ISSET(sockfd, &rfds)) {
			/* A packet is ready, read it */

			if (listen_mode == LISTEN_KERNEL)
				len = udhcp_recv_packet(&packet, sockfd);
			else
				len = get_raw_packet(&packet, sockfd);

			if (len == -1) { /* error is severe, reopen socket */
				DEBUG("error on read, %s, reopening socket", strerror(errno));
				change_listen_mode(listen_mode); /* just close and reopen */
			}
			if (len < 0) continue;

			if (packet.xid != xid) {
				DEBUG("Ignoring XID %x (our xid is %x)",
					(unsigned)packet.xid, (unsigned)xid);
				continue;
			}

			/* Ignore packets that aren't for us */
			if (memcmp(packet.chaddr, client_config.arp, 6)) {
				DEBUG("Packet does not have our chaddr - ignoring");
				continue;
			}

			message = get_option(&packet, DHCP_MESSAGE_TYPE);
			if (message == NULL) {
				bb_error_msg("cannot get message type from packet - ignoring");
				continue;
			}

			switch (state) {
			case INIT_SELECTING:
				/* Must be a DHCPOFFER to one of our xid's */
				if (*message == DHCPOFFER) {
			/* TODO: why we don't just fetch server's IP from IP header? */
					temp = get_option(&packet, DHCP_SERVER_ID);
					if (!temp) {
						bb_error_msg("no server ID in message");
						continue;
						/* still selecting - this server looks bad */
					}
					/* can be misaligned, thus memcpy */
					memcpy(&server_addr, temp, 4);
					xid = packet.xid;
					requested_ip = packet.yiaddr;

					/* enter requesting state */
					state = REQUESTING;
					timeout = 0;
					packet_num = 0;
				}
				continue;
			case RENEW_REQUESTED:
			case REQUESTING:
			case RENEWING:
			case REBINDING:
				if (*message == DHCPACK) {
					temp = get_option(&packet, DHCP_LEASE_TIME);
					if (!temp) {
						bb_error_msg("no lease time with ACK, using 1 hour lease");
						lease_seconds = 60 * 60;
					} else {
						/* can be misaligned, thus memcpy */
						memcpy(&lease_seconds, temp, 4);
						lease_seconds = ntohl(lease_seconds);
					}
#if ENABLE_FEATURE_UDHCPC_ARPING
					if (opt & OPT_a) {
						if (!arpping(packet.yiaddr,
							    (uint32_t) 0,
							    client_config.arp,
							    client_config.interface)
						) {
							bb_info_msg("offered address is in use "
								"(got ARP reply), declining");
							send_decline(xid, server_addr, packet.yiaddr);

							if (state != REQUESTING)
								udhcp_run_script(NULL, "deconfig");
							change_listen_mode(LISTEN_RAW);
							state = INIT_SELECTING;
							requested_ip = 0;
							timeout = tryagain_timeout;
							packet_num = 0;
							continue; /* back to main loop */
						}
					}
#endif
					/* enter bound state */
					t1 = lease_seconds / 2;

					/* little fixed point for n * .875 */
					t2 = (lease_seconds * 7) >> 3;
					temp_addr.s_addr = packet.yiaddr;
					bb_info_msg("Lease of %s obtained, lease time %u",
						inet_ntoa(temp_addr), (unsigned)lease_seconds);
					timestamp_got_lease = monotonic_sec();
					timeout = t1;
					requested_ip = packet.yiaddr;
					udhcp_run_script(&packet,
						   ((state == RENEWING || state == REBINDING) ? "renew" : "bound"));

					state = BOUND;
					change_listen_mode(LISTEN_NONE);
					if (client_config.quit_after_lease) {
						if (client_config.release_on_quit)
							perform_release();
						goto ret0;
					}
					if (!client_config.foreground)
						client_background();

					continue; /* back to main loop */
				}
				if (*message == DHCPNAK) {
					/* return to init state */
					bb_info_msg("Received DHCP NAK");
					udhcp_run_script(&packet, "nak");
					if (state != REQUESTING)
						udhcp_run_script(NULL, "deconfig");
					change_listen_mode(LISTEN_RAW);
					sleep(3); /* avoid excessive network traffic */
					state = INIT_SELECTING;
					requested_ip = 0;
					timeout = 0;
					packet_num = 0;
				}
				continue;
			/* case BOUND, RELEASED: - ignore all packets */
			}
			continue; /* back to main loop */
		}

		/* select() didn't timeout, something did happen.
		 * But it wasn't a packet. It's a signal pipe then. */
		{
			int signo = udhcp_sp_read(&rfds);
			switch (signo) {
			case SIGUSR1:
				perform_renew();
				/* start things over */
				packet_num = 0;
				/* Kill any timeouts because the user wants this to hurry along */
				timeout = 0;
				break;
			case SIGUSR2:
				perform_release();
				break;
			case SIGTERM:
				bb_info_msg("Received SIGTERM");
				if (client_config.release_on_quit)
					perform_release();
				goto ret0;
			}
		}
	} /* for (;;) - main loop ends */

 ret0:
	retval = 0;
 ret:
	/*if (client_config.pidfile) - remove_pidfile has it's own check */
		remove_pidfile(client_config.pidfile);
	return retval;
}
