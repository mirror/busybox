/* vi: set sw=4 ts=4: */
/* dhcpc.c
 *
 * udhcp DHCP client
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include <syslog.h>

/* Override ENABLE_FEATURE_PIDFILE - ifupdown needs our pidfile to always exist */
#define WANT_PIDFILE 1
#include "common.h"
#include "dhcpd.h"
#include "dhcpc.h"
#include "options.h"


static int sockfd = -1;

#define LISTEN_NONE 0
#define LISTEN_KERNEL 1
#define LISTEN_RAW 2
static smallint listen_mode;

#define INIT_SELECTING  0
#define REQUESTING      1
#define BOUND           2
#define RENEWING        3
#define REBINDING       4
#define INIT_REBOOT     5
#define RENEW_REQUESTED 6
#define RELEASED        7
static smallint state;

/* struct client_config_t client_config is in bb_common_bufsiz1 */


/* just a little helper */
static void change_listen_mode(int new_mode)
{
	DEBUG("entering %s listen mode",
		new_mode ? (new_mode == 1 ? "kernel" : "raw") : "none");

	listen_mode = new_mode;
	if (sockfd >= 0) {
		close(sockfd);
		sockfd = -1;
	}
	if (new_mode == LISTEN_KERNEL)
		sockfd = udhcp_listen_socket(/*INADDR_ANY,*/ CLIENT_PORT, client_config.interface);
	else if (new_mode != LISTEN_NONE)
		sockfd = udhcp_raw_socket(client_config.ifindex);
	/* else LISTEN_NONE: sockfd stay closed */
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
static void perform_release(uint32_t requested_ip, uint32_t server_addr)
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
}


#if BB_MMU
static void client_background(void)
{
	bb_daemonize(0);
	logmode &= ~LOGMODE_STDIO;
	/* rewrite pidfile, as our pid is different now */
	write_pidfile(client_config.pidfile);
}
#endif


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
int udhcpc_main(int argc UNUSED_PARAM, char **argv)
{
	uint8_t *temp, *message;
	char *str_c, *str_V, *str_h, *str_F, *str_r;
	USE_FEATURE_UDHCP_PORT(char *str_P;)
	llist_t *list_O = NULL;
	int tryagain_timeout = 20;
	int discover_timeout = 3;
	int discover_retries = 3;
	uint32_t server_addr = server_addr; /* for compiler */
	uint32_t requested_ip = 0;
	uint32_t xid = 0;
	uint32_t lease_seconds = 0; /* can be given as 32-bit quantity */
	int packet_num;
	int timeout; /* must be signed */
	unsigned already_waited_sec;
	unsigned opt;
	int max_fd;
	int retval;
	struct timeval tv;
	struct dhcpMessage packet;
	fd_set rfds;

#if ENABLE_GETOPT_LONG
	static const char udhcpc_longopts[] ALIGN1 =
		"clientid\0"       Required_argument "c"
		"clientid-none\0"  No_argument       "C"
		"vendorclass\0"    Required_argument "V"
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
		"request-option\0" Required_argument "O"
		"no-default-options\0" No_argument   "o"
		"foreground\0"     No_argument       "f"
		"background\0"     No_argument       "b"
		USE_FEATURE_UDHCPC_ARPING("arping\0"	No_argument       "a")
		USE_FEATURE_UDHCP_PORT("client-port\0"	Required_argument "P")
		;
#endif
	enum {
		OPT_c = 1 << 0,
		OPT_C = 1 << 1,
		OPT_V = 1 << 2,
		OPT_H = 1 << 3,
		OPT_h = 1 << 4,
		OPT_F = 1 << 5,
		OPT_i = 1 << 6,
		OPT_n = 1 << 7,
		OPT_p = 1 << 8,
		OPT_q = 1 << 9,
		OPT_R = 1 << 10,
		OPT_r = 1 << 11,
		OPT_s = 1 << 12,
		OPT_T = 1 << 13,
		OPT_t = 1 << 14,
		OPT_v = 1 << 15,
		OPT_S = 1 << 16,
		OPT_A = 1 << 17,
		OPT_O = 1 << 18,
		OPT_o = 1 << 19,
		OPT_f = 1 << 20,
/* The rest has variable bit positions, need to be clever */
		OPTBIT_f = 20,
		USE_FOR_MMU(              OPTBIT_b,)
		USE_FEATURE_UDHCPC_ARPING(OPTBIT_a,)
		USE_FEATURE_UDHCP_PORT(   OPTBIT_P,)
		USE_FOR_MMU(              OPT_b = 1 << OPTBIT_b,)
		USE_FEATURE_UDHCPC_ARPING(OPT_a = 1 << OPTBIT_a,)
		USE_FEATURE_UDHCP_PORT(   OPT_P = 1 << OPTBIT_P,)
	};

	/* Default options. */
	USE_FEATURE_UDHCP_PORT(SERVER_PORT = 67;)
	USE_FEATURE_UDHCP_PORT(CLIENT_PORT = 68;)
	client_config.interface = "eth0";
	client_config.script = DEFAULT_SCRIPT;

	/* Parse command line */
	/* Cc: mutually exclusive; O: list; -T,-t,-A take numeric param */
	opt_complementary = "c--C:C--c:O::T+:t+:A+";
	USE_GETOPT_LONG(applet_long_options = udhcpc_longopts;)
	opt = getopt32(argv, "c:CV:H:h:F:i:np:qRr:s:T:t:vSA:O:of"
		USE_FOR_MMU("b")
		USE_FEATURE_UDHCPC_ARPING("a")
		USE_FEATURE_UDHCP_PORT("P:")
		, &str_c, &str_V, &str_h, &str_h, &str_F
		, &client_config.interface, &client_config.pidfile, &str_r /* i,p */
		, &client_config.script /* s */
		, &discover_timeout, &discover_retries, &tryagain_timeout /* T,t,A */
		, &list_O
		USE_FEATURE_UDHCP_PORT(, &str_P)
		);
	if (opt & OPT_c)
		client_config.clientid = alloc_dhcp_option(DHCP_CLIENT_ID, str_c, 0);
	if (opt & OPT_V)
		client_config.vendorclass = alloc_dhcp_option(DHCP_VENDOR, str_V, 0);
	if (opt & (OPT_h|OPT_H))
		client_config.hostname = alloc_dhcp_option(DHCP_HOST_NAME, str_h, 0);
	if (opt & OPT_F) {
		client_config.fqdn = alloc_dhcp_option(DHCP_FQDN, str_F, 3);
		/* Flags: 0000NEOS
		S: 1 => Client requests Server to update A RR in DNS as well as PTR
		O: 1 => Server indicates to client that DNS has been updated regardless
		E: 1 => Name data is DNS format, i.e. <4>host<6>domain<3>com<0> not "host.domain.com"
		N: 1 => Client requests Server to not update DNS
		*/
		client_config.fqdn[OPT_DATA + 0] = 0x1;
		/* client_config.fqdn[OPT_DATA + 1] = 0; - redundant */
		/* client_config.fqdn[OPT_DATA + 2] = 0; - redundant */
	}
	if (opt & OPT_r)
		requested_ip = inet_addr(str_r);
	if (opt & OPT_v) {
		puts("version "BB_VER);
		return 0;
	}
#if ENABLE_FEATURE_UDHCP_PORT
	if (opt & OPT_P) {
		CLIENT_PORT = xatou16(str_P);
		SERVER_PORT = CLIENT_PORT - 1;
	}
#endif
	if (opt & OPT_o)
		client_config.no_default_options = 1;
	while (list_O) {
		char *optstr = llist_pop(&list_O);
		int n = index_in_strings(dhcp_option_strings, optstr);
		if (n < 0)
			bb_error_msg_and_die("unknown option '%s'", optstr);
		n = dhcp_options[n].code;
		client_config.opt_mask[n >> 3] |= 1 << (n & 7);
	}

	if (udhcp_read_interface(client_config.interface, &client_config.ifindex,
			   NULL, client_config.arp))
		return 1;
#if !BB_MMU
	/* on NOMMU reexec (i.e., background) early */
	if (!(opt & OPT_f)) {
		bb_daemonize_or_rexec(0 /* flags */, argv);
		logmode = LOGMODE_NONE;
	}
#endif
	if (opt & OPT_S) {
		openlog(applet_name, LOG_PID, LOG_DAEMON);
		logmode |= LOGMODE_SYSLOG;
	}

	/* Make sure fd 0,1,2 are open */
	bb_sanitize_stdio();
	/* Equivalent of doing a fflush after every \n */
	setlinebuf(stdout);

	/* Create pidfile */
	write_pidfile(client_config.pidfile);

	/* Goes to stdout (unless NOMMU) and possibly syslog */
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
	timeout = 0;
	already_waited_sec = 0;

	/* Main event loop. select() waits on signal pipe and possibly
	 * on sockfd.
	 * "continue" statements in code below jump to the top of the loop.
	 */
	for (;;) {
		/* silence "uninitialized!" warning */
		unsigned timestamp_before_wait = timestamp_before_wait;

		//bb_error_msg("sockfd:%d, listen_mode:%d", sockfd, listen_mode);

		/* Was opening raw or udp socket here
		 * if (listen_mode != LISTEN_NONE && sockfd < 0),
		 * but on fast network renew responses return faster
		 * than we open sockets. Thus this code is moved
		 * to change_listen_mode(). Thus we open listen socket
		 * BEFORE we send renew request (see "case BOUND:"). */

		max_fd = udhcp_sp_fd_set(&rfds, sockfd);

		tv.tv_sec = timeout - already_waited_sec;
		tv.tv_usec = 0;
		retval = 0; /* If we already timed out, fall through, else... */
		if (tv.tv_sec > 0) {
			timestamp_before_wait = (unsigned)monotonic_sec();
			DEBUG("Waiting on select...");
			retval = select(max_fd + 1, &rfds, NULL, NULL, &tv);
			if (retval < 0) {
				/* EINTR? A signal was caught, don't panic */
				if (errno == EINTR)
					continue;
				/* Else: an error occured, panic! */
				bb_perror_msg_and_die("select");
			}
		}

		/* If timeout dropped to zero, time to become active:
		 * resend discover/renew/whatever
		 */
		if (retval == 0) {
			/* We will restart the wait in any case */
			already_waited_sec = 0;

			switch (state) {
			case INIT_SELECTING:
				if (packet_num < discover_retries) {
					if (packet_num == 0)
						xid = random_xid();

					send_discover(xid, requested_ip); /* broadcast */

					timeout = discover_timeout;
					packet_num++;
					continue;
				}
 leasefail:
				udhcp_run_script(NULL, "leasefail");
#if BB_MMU /* -b is not supported on NOMMU */
				if (opt & OPT_b) { /* background if no lease */
					bb_info_msg("No lease, forking to background");
					client_background();
					/* do not background again! */
					opt = ((opt & ~OPT_b) | OPT_f);
				} else
#endif
				if (opt & OPT_n) { /* abort if no lease */
					bb_info_msg("No lease, failing");
					retval = 1;
					goto ret;
				}
				/* wait before trying again */
				timeout = tryagain_timeout;
				packet_num = 0;
				continue;
			case RENEW_REQUESTED:
			case REQUESTING:
				if (packet_num < discover_retries) {
					/* send request packet */
					if (state == RENEW_REQUESTED) /* unicast */
						send_renew(xid, server_addr, requested_ip);
					else /* broadcast */
						send_select(xid, server_addr, requested_ip);

					timeout = discover_timeout;
					packet_num++;
					continue;
				}
				/* timed out, go back to init state */
				if (state == RENEW_REQUESTED)
					udhcp_run_script(NULL, "deconfig");
				change_listen_mode(LISTEN_RAW);
				/* "discover...select...discover..." loops
				 * were seen in the wild. Treat them similarly
				 * to "no response to discover" case */
				if (state == REQUESTING) {
					state = INIT_SELECTING;
					goto leasefail;
				}
				state = INIT_SELECTING;
				timeout = 0;
				packet_num = 0;
				continue;
			case BOUND:
				/* Half of the lease passed, time to enter renewing state */
				change_listen_mode(LISTEN_KERNEL);
				DEBUG("Entering renew state");
				state = RENEWING;
				/* fall right through */
			case RENEWING:
				if (timeout > 60) {
					/* send a request packet */
					send_renew(xid, server_addr, requested_ip); /* unicast */
					timeout >>= 1;
					continue;
				}
				/* Timed out, enter rebinding state */
				DEBUG("Entering rebinding state");
				state = REBINDING;
				/* fall right through */
			case REBINDING:
				/* Lease is *really* about to run out,
				 * try to find DHCP server using broadcast */
				if (timeout > 0) {
					/* send a request packet */
					send_renew(xid, 0 /*INADDR_ANY*/, requested_ip); /* broadcast */
					timeout >>= 1;
					continue;
				}
				/* Timed out, enter init state */
				bb_info_msg("Lease lost, entering init state");
				udhcp_run_script(NULL, "deconfig");
				change_listen_mode(LISTEN_RAW);
				state = INIT_SELECTING;
				/*timeout = 0; - already is */
				packet_num = 0;
				continue;
			/* case RELEASED: */
			}
			/* yah, I know, *you* say it would never happen */
			timeout = INT_MAX;
			continue; /* back to main loop */
		}

		/* select() didn't timeout, something did happen. */
		/* Is it a packet? */
		if (listen_mode != LISTEN_NONE && FD_ISSET(sockfd, &rfds)) {
			int len;
			/* A packet is ready, read it */

			if (listen_mode == LISTEN_KERNEL)
				len = udhcp_recv_kernel_packet(&packet, sockfd);
			else
				len = udhcp_recv_raw_packet(&packet, sockfd);
			if (len == -1) { /* error is severe, reopen socket */
				DEBUG("error on read, %s, reopening socket", strerror(errno));
				sleep(discover_timeout); /* 3 seconds by default */
				change_listen_mode(listen_mode); /* just close and reopen */
			}
			/* If this packet will turn out to be unrelated/bogus,
			 * we will go back and wait for next one.
			 * Be sure timeout is properly decreased. */
			already_waited_sec += (unsigned)monotonic_sec() - timestamp_before_wait;
			if (len < 0)
				continue;

			if (packet.xid != xid) {
				DEBUG("Ignoring xid %x (our xid is %x)",
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
					/* it IS unaligned sometimes, don't "optimize" */
					move_from_unaligned32(server_addr, temp);
					xid = packet.xid;
					requested_ip = packet.yiaddr;

					/* enter requesting state */
					state = REQUESTING;
					timeout = 0;
					packet_num = 0;
					already_waited_sec = 0;
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
						/* it IS unaligned sometimes, don't "optimize" */
						move_from_unaligned32(lease_seconds, temp);
						lease_seconds = ntohl(lease_seconds);
						lease_seconds &= 0x0fffffff; /* paranoia: must not be prone to overflows */
						if (lease_seconds < 10) /* and not too small */
							lease_seconds = 10;
					}
#if ENABLE_FEATURE_UDHCPC_ARPING
					if (opt & OPT_a) {
/* RFC 2131 3.1 paragraph 5:
 * "The client receives the DHCPACK message with configuration
 * parameters. The client SHOULD perform a final check on the
 * parameters (e.g., ARP for allocated network address), and notes
 * the duration of the lease specified in the DHCPACK message. At this
 * point, the client is configured. If the client detects that the
 * address is already in use (e.g., through the use of ARP),
 * the client MUST send a DHCPDECLINE message to the server and restarts
 * the configuration process..." */
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
							already_waited_sec = 0;
							continue; /* back to main loop */
						}
					}
#endif
					/* enter bound state */
					timeout = lease_seconds / 2;
					{
						struct in_addr temp_addr;
						temp_addr.s_addr = packet.yiaddr;
						bb_info_msg("Lease of %s obtained, lease time %u",
							inet_ntoa(temp_addr), (unsigned)lease_seconds);
					}
					requested_ip = packet.yiaddr;
					udhcp_run_script(&packet,
							((state == RENEWING || state == REBINDING) ? "renew" : "bound"));

					state = BOUND;
					change_listen_mode(LISTEN_NONE);
					if (opt & OPT_q) { /* quit after lease */
						if (opt & OPT_R) /* release on quit */
							perform_release(requested_ip, server_addr);
						goto ret0;
					}
#if BB_MMU /* NOMMU case backgrounded earlier */
					if (!(opt & OPT_f)) {
						client_background();
						/* do not background again! */
						opt = ((opt & ~OPT_b) | OPT_f);
					}
#endif
					already_waited_sec = 0;
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
					already_waited_sec = 0;
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
				perform_release(requested_ip, server_addr);
				timeout = INT_MAX;
				break;
			case SIGTERM:
				bb_info_msg("Received SIGTERM");
				if (opt & OPT_R) /* release on quit */
					perform_release(requested_ip, server_addr);
				goto ret0;
			}
		}
	} /* for (;;) - main loop ends */

 ret0:
	retval = 0;
 ret:
	/*if (client_config.pidfile) - remove_pidfile has its own check */
		remove_pidfile(client_config.pidfile);
	return retval;
}
