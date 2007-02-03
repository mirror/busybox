/* vi: set sw=4 ts=4: */
/* Port to Busybox Copyright (C) 2006 Jesse Dutton <jessedutton@gmail.com>
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 *
 * DHCP Relay for 'DHCPv4 Configuration of IPSec Tunnel Mode' support
 * Copyright (C) 2002 Mario Strasser <mast@gmx.net>,
 *                   Zuercher Hochschule Winterthur,
 *                   Netbeat AG
 * Upstream has GPL v2 or later
 */

#include "common.h"
#include "dhcpd.h"
#include "options.h"

/* constants */
#define SELECT_TIMEOUT 5 /* select timeout in sec. */
#define MAX_LIFETIME 2*60 /* lifetime of an xid entry in sec. */
#define MAX_INTERFACES 9


/* This list holds information about clients. The xid_* functions manipulate this list. */
static struct xid_item {
	uint32_t xid;
	struct sockaddr_in ip;
	int client;
	time_t timestamp;
	struct xid_item *next;
} dhcprelay_xid_list = {0, {0}, 0, 0, NULL};


static struct xid_item * xid_add(uint32_t xid, struct sockaddr_in *ip, int client)
{
	struct xid_item *item;

	/* create new xid entry */
	item = xmalloc(sizeof(struct xid_item));

	/* add xid entry */
	item->ip = *ip;
	item->xid = xid;
	item->client = client;
	item->timestamp = time(NULL);
	item->next = dhcprelay_xid_list.next;
	dhcprelay_xid_list.next = item;

	return item;
}


static void xid_expire(void)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	struct xid_item *last = &dhcprelay_xid_list;
	time_t current_time = time(NULL);

	while (item != NULL) {
		if ((current_time-item->timestamp) > MAX_LIFETIME) {
			last->next = item->next;
			free(item);
			item = last->next;
		} else {
			last = item;
			item = item->next;
		}
	}
}

static struct xid_item * xid_find(uint32_t xid)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	while (item != NULL) {
		if (item->xid == xid) {
			return item;
		}
		item = item->next;
	}
	return NULL;
}

static void xid_del(uint32_t xid)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	struct xid_item *last = &dhcprelay_xid_list;
	while (item != NULL) {
		if (item->xid == xid) {
			last->next = item->next;
			free(item);
			item = last->next;
		} else {
			last = item;
			item = item->next;
		}
	}
}


/**
 * get_dhcp_packet_type - gets the message type of a dhcp packet
 * p - pointer to the dhcp packet
 * returns the message type on success, -1 otherwise
 */
static int get_dhcp_packet_type(struct dhcpMessage *p)
{
	uint8_t *op;

	/* it must be either a BOOTREQUEST or a BOOTREPLY */
	if (p->op != BOOTREQUEST && p->op != BOOTREPLY)
		return -1;
	/* get message type option */
	op = get_option(p, DHCP_MESSAGE_TYPE);
	if (op != NULL)
		return op[0];
	return -1;
}

/**
 * signal_handler - handles signals ;-)
 * sig - sent signal
 */
static int dhcprelay_stopflag;
static void dhcprelay_signal_handler(int sig)
{
	dhcprelay_stopflag = 1;
}

/**
 * get_client_devices - parses the devices list
 * dev_list - comma separated list of devices
 * returns array
 */
static char ** get_client_devices(char *dev_list, int *client_number)
{
	char *s, *list, **client_dev;
	int i, cn;

	/* copy list */
	list = xstrdup(dev_list);
	if (list == NULL) return NULL;

	/* get number of items */
	for (s = dev_list, cn = 1; *s; s++)
		if (*s == ',')
			cn++;

	client_dev = xzalloc(cn * sizeof(*client_dev));

	/* parse list */
	s = strtok(list, ",");
	i = 0;
	while (s != NULL) {
		client_dev[i++] = xstrdup(s);
		s = strtok(NULL, ",");
	}

	/* free copy and exit */
	free(list);
	*client_number = cn;
	return client_dev;
}


/* Creates listen sockets (in fds) and returns the number allocated. */
static int init_sockets(char **client, int num_clients,
			char *server, int *fds, int *max_socket)
{
	int i;

	/* talk to real server on bootps */
	fds[0] = listen_socket(htonl(INADDR_ANY), 67, server);
	*max_socket = fds[0];

	/* array starts at 1 since server is 0 */
	num_clients++;

	for (i=1; i < num_clients; i++) {
		/* listen for clients on bootps */
		fds[i] = listen_socket(htonl(INADDR_ANY), 67, client[i-1]);
		if (fds[i] > *max_socket) *max_socket = fds[i];
	}

	return i;
}


/**
 * pass_on() - forwards dhcp packets from client to server
 * p - packet to send
 * client - number of the client
 */
static void pass_on(struct dhcpMessage *p, int packet_len, int client, int *fds,
			struct sockaddr_in *client_addr, struct sockaddr_in *server_addr)
{
	int res, type;
	struct xid_item *item;

	/* check packet_type */
	type = get_dhcp_packet_type(p);
	if (type != DHCPDISCOVER && type != DHCPREQUEST
	 && type != DHCPDECLINE && type != DHCPRELEASE
	 && type != DHCPINFORM
	) {
		return;
	}

	/* create new xid entry */
	item = xid_add(p->xid, client_addr, client);

	/* forward request to LAN (server) */
	res = sendto(fds[0], p, packet_len, 0, (struct sockaddr*)server_addr,
			sizeof(struct sockaddr_in));
	if (res != packet_len) {
		bb_perror_msg("pass_on");
		return;
	}
}

/**
 * pass_back() - forwards dhcp packets from server to client
 * p - packet to send
 */
static void pass_back(struct dhcpMessage *p, int packet_len, int *fds)
{
	int res, type;
	struct xid_item *item;

	/* check xid */
	item = xid_find(p->xid);
	if (!item) {
		return;
	}

	/* check packet type */
	type = get_dhcp_packet_type(p);
	if (type != DHCPOFFER && type != DHCPACK && type != DHCPNAK) {
		return;
	}

	if (item->ip.sin_addr.s_addr == htonl(INADDR_ANY))
		item->ip.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	if (item->client > MAX_INTERFACES)
		return;
	res = sendto(fds[item->client], p, packet_len, 0, (struct sockaddr*)(&item->ip),
				sizeof(item->ip));
	if (res != packet_len) {
		bb_perror_msg("pass_back");
		return;
	}

	/* remove xid entry */
	xid_del(p->xid);
}

static void dhcprelay_loop(int *fds, int num_sockets, int max_socket, char **clients,
		struct sockaddr_in *server_addr, uint32_t gw_ip)
{
	struct dhcpMessage dhcp_msg;
	fd_set rfds;
	size_t packlen;
	socklen_t addr_size;
	struct sockaddr_in client_addr;
	struct timeval tv;
	int i;

	while (!dhcprelay_stopflag) {
		FD_ZERO(&rfds);
		for (i = 0; i < num_sockets; i++)
			FD_SET(fds[i], &rfds);
		tv.tv_sec = SELECT_TIMEOUT;
		tv.tv_usec = 0;
		if (select(max_socket + 1, &rfds, NULL, NULL, &tv) > 0) {
			/* server */
			if (FD_ISSET(fds[0], &rfds)) {
				packlen = udhcp_get_packet(&dhcp_msg, fds[0]);
				if (packlen > 0) {
					pass_back(&dhcp_msg, packlen, fds);
				}
			}
			for (i = 1; i < num_sockets; i++) {
				/* clients */
				if (!FD_ISSET(fds[i], &rfds))
					continue;
				addr_size = sizeof(struct sockaddr_in);
				packlen = recvfrom(fds[i], &dhcp_msg, sizeof(dhcp_msg), 0,
							(struct sockaddr *)(&client_addr), &addr_size);
				if (packlen <= 0)
					continue;
				if (read_interface(clients[i-1], NULL, &dhcp_msg.giaddr, NULL) < 0)
					dhcp_msg.giaddr = gw_ip;
				pass_on(&dhcp_msg, packlen, i, fds, &client_addr, server_addr);
			}
		}
		xid_expire();
	}
}

int dhcprelay_main(int argc, char **argv);
int dhcprelay_main(int argc, char **argv)
{
	int i, num_sockets, max_socket, fds[MAX_INTERFACES];
	uint32_t gw_ip;
	char **clients;
	struct sockaddr_in server_addr;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(67);
	if (argc == 4) {
		if (!inet_aton(argv[3], &server_addr.sin_addr))
			bb_perror_msg_and_die("didn't grok server");
	} else if (argc == 3) {
		server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	} else {
		bb_show_usage();
	}
	clients = get_client_devices(argv[1], &num_sockets);
	if (!clients) return 0;

	signal(SIGTERM, dhcprelay_signal_handler);
	signal(SIGQUIT, dhcprelay_signal_handler);
	signal(SIGINT, dhcprelay_signal_handler);

	num_sockets = init_sockets(clients, num_sockets, argv[2], fds, &max_socket);

	if (read_interface(argv[2], NULL, &gw_ip, NULL) == -1)
		return 1;

	dhcprelay_loop(fds, num_sockets, max_socket, clients, &server_addr, gw_ip);

	if (ENABLE_FEATURE_CLEAN_UP) {
		for (i = 0; i < num_sockets; i++) {
			close(fds[i]);
			free(clients[i]);
		}
	}

	return 0;
}
